#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Isolated KWin SDR profile backend, with an independent exit-restoration guard."""
import fcntl
import json
import os
from pathlib import Path
import re
import select
import signal
import subprocess
import sys
import tempfile
import time

from make_profile import generate

ENV = dict(os.environ, QT_QPA_PLATFORM='wayland', LC_ALL='C.UTF-8')
# Bundled X11 UI libraries must not override system Qt/KScreen libraries.
ENV.pop('LD_LIBRARY_PATH', None)
ENV.pop('QT_PLUGIN_PATH', None)


def command(args):
    p = subprocess.run(['kscreen-doctor', *args], env=ENV, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=8)
    if p.returncode:
        raise RuntimeError('KScreen: ' + (p.stderr.strip() or p.stdout.strip())[:300])
    return p.stdout


def outputs():
    data = json.loads(command(['-j']))
    details = re.sub(r'\x1b\[[0-9;]*m', '', command(['-o']))
    metadata = {}
    current = None
    for line in details.splitlines():
        if line.startswith('Output:'):
            fields = line.split()
            current = fields[2]
            metadata[current] = {'uuid': fields[3]}
        elif current and 'Color profile source:' in line:
            metadata[current]['source'] = line.split(':', 1)[1].strip()
        elif current and 'HDR:' in line:
            metadata[current]['hdr'] = line.split(':', 1)[1].strip()
    result = {}
    for o in data['outputs']:
        if o['connected'] and o['enabled']:
            m = metadata.get(o['name'], {})
            if not m.get('uuid') or 'source' not in m:
                raise RuntimeError('Cannot read the original display color profile')
            o.update(m)
            result[o['uuid']] = o
    return result


def save(path, state):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(state, indent=2))
    temp.replace(path)


def restore(path):
    if not path.exists():
        return True
    state = json.loads(path.read_text())
    if not state['active']:
        return True
    current = outputs()
    args, changed = [], []
    missing = False
    for key, old in state['outputs'].items():
        o = current.get(key)
        if not o:
            missing = True
            continue
        # Never overwrite a profile the user or another tool selected afterward.
        if o['source'] != 'ICC' or Path(o['iccProfilePath']).parent != Path(state['directory']):
            continue
        args.extend([f"output.{o['name']}.iccprofile.{old['path']}",
                     f"output.{o['name']}.colorProfileSource.{old['source']}"])
        changed.append(key)
    if args:
        command(args)
        verified = outputs()
        for key in changed:
            o, old = verified.get(key), state['outputs'][key]
            if not o or o['iccProfilePath'] != old['path'] or o['source'] != old['source']:
                raise RuntimeError('Original profile restoration failed readback')
    if not missing:
        state['active'] = False
        save(path, state)
    return not missing


def guard(path):
    # This process owns no filter. EOF arrives even if the UI/helper is killed.
    print('READY', flush=True)
    while sys.stdin.buffer.read(1):
        pass
    for attempt in range(10):
        try:
            if restore(path):
                print('plasma: exit guard restored original profiles', file=sys.stderr, flush=True)
                return
        except Exception as e:
            print(f'plasma: restoration retry: {e}', file=sys.stderr, flush=True)
        time.sleep(1)
    print(f'plasma: restoration pending in {path}; will retry at next launch', file=sys.stderr, flush=True)


class Backend:
    def __init__(self):
        self.root = Path(os.environ['TAPZAP_PLASMA_STATE'])
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.lock = (self.root / 'backend.lock').open('a')
        deadline = time.monotonic() + 3
        while True:
            try:
                fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Another Plasma helper is active or still restoring colors')
                time.sleep(0.05)
        self.journal = self.root / 'state.json'
        if not restore(self.journal):
            raise RuntimeError('Reconnect the previous display to restore its saved profile first')
        directory = tempfile.mkdtemp(prefix='session-', dir=self.root)
        self.state = {'active': False, 'directory': directory, 'outputs': {}}
        self.last = None
        self.sequence = 0
        self.capture(outputs())
        if not self.state['outputs']:
            raise RuntimeError('No active displays')
        save(self.journal, self.state)
        read_fd, self.lease = os.pipe()
        self.watcher = subprocess.Popen(
            [sys.executable, __file__, '--guard', str(self.journal)],
            stdin=read_fd, stdout=subprocess.PIPE, env=ENV, start_new_session=True,
            pass_fds=(self.lock.fileno(),))
        os.close(read_fd)
        if not select.select([self.watcher.stdout], [], [], 3)[0] or self.watcher.stdout.readline() != b'READY\n':
            os.close(self.lease)
            raise RuntimeError('Could not arm display restoration guard')

    def capture(self, current):
        for key, o in current.items():
            if key in self.state['outputs']:
                continue
            if o.get('hdr') == 'enabled':
                raise RuntimeError('This proof currently requires an SDR output')
            if o['source'] not in ('sRGB', 'ICC'):
                raise RuntimeError('This proof needs an sRGB or ICC baseline; EDID support is pending')
            original = o['iccProfilePath']
            baseline = None
            if o['source'] == 'ICC':
                if not original or not Path(original).is_file():
                    raise RuntimeError('Original ICC profile cannot be read')
                baseline = str(Path(self.state['directory']) / f'{key}-baseline.icc')
                Path(baseline).write_bytes(Path(original).read_bytes())
            self.state['outputs'][key] = {'name': o['name'], 'path': original,
                                          'source': o['source'], 'baseline': baseline,
                                          'expected': ''}

    def apply(self, gains):
        if self.watcher.poll() is not None:
            raise RuntimeError('The restoration guard exited')
        current = outputs()
        if not current:
            raise RuntimeError('No active displays')
        if not self.state['active']:
            self.state['outputs'] = {}  # Respect profile changes made while ZAP was OFF.
        self.capture(current)
        self.sequence += 1
        args, previous = [], []
        for key, o in current.items():
            old = self.state['outputs'][key]
            if self.state['active']:
                own = Path(o['iccProfilePath']).parent == Path(self.state['directory'])
                baseline = o['iccProfilePath'] == old['path'] and o['source'] == old['source']
                if not own and not baseline:
                    raise RuntimeError('Another color profile was selected; turn ZAP off and restart')
            target = str(Path(self.state['directory']) / f'{key}-{self.sequence}.icc')
            generate(target, gains, old['baseline'])
            previous.append(old['expected'])
            old['expected'] = target
            args.extend([f"output.{o['name']}.iccprofile.{target}",
                         f"output.{o['name']}.colorProfileSource.ICC"])
        self.state['active'] = True
        save(self.journal, self.state)  # Recovery is durable before applying anything.
        command(args)
        verified = outputs()
        for key in current:
            o = verified.get(key)
            if not o or o['iccProfilePath'] != self.state['outputs'][key]['expected'] or o['source'] != 'ICC':
                raise RuntimeError('KWin did not confirm the requested color profile')
        self.last = gains
        for path in previous:
            if path and Path(path).parent == Path(self.state['directory']):
                Path(path).unlink(missing_ok=True)

    def off(self):
        if not restore(self.journal):
            raise RuntimeError('A disconnected output still needs restoration')
        self.state['active'] = False
        self.last = None

    def check(self):
        if self.watcher.poll() is not None:
            raise RuntimeError('The restoration guard exited')
        if not self.state['active']:
            return
        current = outputs()
        if any(key not in self.state['outputs'] or
               o['iccProfilePath'] != self.state['outputs'][key]['expected'] or
               o['source'] != 'ICC' for key, o in current.items()):
            print('plasma: restoring active profile after output change', file=sys.stderr, flush=True)
            self.apply(self.last)

    def close(self):
        try:
            self.off()
        finally:
            os.close(self.lease)
            try:
                self.watcher.wait(timeout=3)
            except subprocess.TimeoutExpired:
                pass  # The independent guard continues a pending restoration.


def main():
    backend = None
    try:
        backend = Backend()
        print(f'OK {len(backend.state["outputs"])}', flush=True)
        for line in sys.stdin:
            fields = line.split()
            try:
                if fields[:1] == ['set'] and len(fields) == 4:
                    backend.apply(tuple(map(float, fields[1:])))
                elif fields == ['off']:
                    backend.off()
                elif fields == ['check']:
                    backend.check()
                elif fields == ['quit']:
                    backend.off()
                    print('OK', flush=True)
                    break
                else:
                    raise ValueError('Unknown backend command')
                print('OK', flush=True)
            except Exception as e:
                try:
                    backend.off()
                except Exception as cleanup:
                    print(f'plasma: cleanup pending: {cleanup}', file=sys.stderr, flush=True)
                print('ERR ' + str(e).replace('\n', ' ')[:400], flush=True)
    except Exception as e:
        print('ERR ' + str(e).replace('\n', ' ')[:400], flush=True)
    finally:
        if backend:
            backend.close()


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--guard':
        guard(Path(sys.argv[2]))
    else:
        main()
