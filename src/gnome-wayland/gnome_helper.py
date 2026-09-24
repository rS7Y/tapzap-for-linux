#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mutter SDR gamma backend. The independent EOF guard restores owned ramps."""
import fcntl
import json
import math
import os
from pathlib import Path
import select
import signal
import subprocess
import sys
import time
from gi.repository import Gio, GLib

DEST = 'org.gnome.Mutter.DisplayConfig'
OBJ = '/org/gnome/Mutter/DisplayConfig'

class Display:
    def __init__(self):
        self.bus = Gio.bus_get_sync(Gio.BusType.SESSION, None)
        self.owner = self.bus.call_sync('org.freedesktop.DBus', '/org/freedesktop/DBus',
            'org.freedesktop.DBus', 'GetNameOwner', GLib.Variant('(s)', (DEST,)),
            None, Gio.DBusCallFlags.NONE, 3000, None).unpack()[0]
    def call(self, method, params=None):
        # Use the unique owner so a new desktop can never inherit an old baseline.
        return self.bus.call_sync(self.owner, OBJ, DEST, method, params, None,
            Gio.DBusCallFlags.NONE, 3000, None).unpack()
    def outputs(self, validate=True):
        serial, crtcs, outputs, *_ = self.call('GetResources')
        if validate:
            _, monitors, _, _ = self.call('GetCurrentState')
            active_names = {o[4] for o in outputs if o[2] >= 0}
            for spec, modes, props in monitors:
                if spec[0] in active_names and props.get('color-mode', 0) != 0:
                    raise RuntimeError('TAP ZAP GNOME currently requires SDR display mode')
        result = {}
        for c in crtcs:
            if not c[4] or not c[5]:
                continue
            names = sorted(o[4] for o in outputs if o[2] == c[0])
            if names:
                key = '|'.join(names)
                result[key] = {'serial': serial, 'id': c[0], 'hardware': c[1]}
        return result
    def get(self, o):
        g = self.call('GetCrtcGamma', GLib.Variant('(uu)', (o['serial'], o['id'])))
        if len(g) != 3 or len({len(c) for c in g}) != 1 or not 2 <= len(g[0]) <= 65536:
            raise RuntimeError('GNOME returned an unsupported display gamma table')
        return [list(c) for c in g]
    def put(self, o, g):
        self.call('SetCrtcGamma', GLib.Variant('(uuaqaqaq)', (o['serial'], o['id'], *g)))
        if self.get(o) != g:
            raise RuntimeError('GNOME did not accept the requested display colors')


def save(path, state):
    temp = path.with_suffix('.tmp')
    temp.write_text(json.dumps(state, separators=(',', ':')))
    temp.replace(path)


def owned(old, current):
    return current == old.get('expected') or current == old.get('previous')


def restore(path, display=None):
    if not path.exists():
        return
    state = json.loads(path.read_text())
    if not state['active']:
        return
    display = display or Display()
    if display.owner != state['owner']:
        # Mutter was restarted/logged out; its old gamma state no longer exists.
        state['active'] = False
        save(path, state)
        return
    current = display.outputs(validate=False)
    for key, old in state['outputs'].items():
        o = current.get(key)
        if o and owned(old, display.get(o)):
            display.put(o, old['baseline'])
    # Disconnected outputs get fresh compositor state when reconnected.
    state['active'] = False
    save(path, state)


def guard(path):
    print('READY', flush=True)
    while sys.stdin.buffer.read(1):
        pass
    for attempt in range(5):
        try:
            restore(path)
            print('gnome: exit guard restored owned display colors', file=sys.stderr, flush=True)
            return
        except Exception as exc:
            print('gnome: restoration retry: '+str(exc), file=sys.stderr, flush=True)
            time.sleep(.5)
    print('gnome: recovery journal retained at '+str(path), file=sys.stderr, flush=True)


class Backend:
    def __init__(self):
        self.root = Path(os.environ['TAPZAP_GNOME_STATE'])
        self.root.mkdir(parents=True, exist_ok=True, mode=0o700)
        self.lock = (self.root/'backend.lock').open('a')
        deadline = time.monotonic()+4
        while True:
            try:
                fcntl.flock(self.lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                if time.monotonic() >= deadline:
                    raise RuntimeError('Another GNOME filter is running or restoring colors')
                time.sleep(.05)
        self.journal = self.root/'state.json'
        self.display = Display()
        restore(self.journal, self.display)
        self.current = self.display.outputs()
        if not self.current:
            raise RuntimeError('No active GNOME displays')
        # Validate reads before claiming a functioning backend.
        for o in self.current.values():
            self.display.get(o)
        self.state = {'active': False, 'owner': self.display.owner, 'outputs': {}}
        self.last = None
        self.next_scan = 0
        save(self.journal, self.state)
        read_fd, self.lease = os.pipe()
        self.watcher = subprocess.Popen([sys.executable, __file__, '--guard', str(self.journal)],
            stdin=read_fd, stdout=subprocess.PIPE, start_new_session=True,
            pass_fds=(self.lock.fileno(),))
        os.close(read_fd)
        if not select.select([self.watcher.stdout], [], [], 3)[0] or self.watcher.stdout.readline() != b'READY\n':
            os.close(self.lease)
            raise RuntimeError('Could not arm GNOME display restoration')

    def apply(self, gains, rescan=False):
        if self.watcher.poll() is not None:
            raise RuntimeError('GNOME display restoration guard exited')
        if len(gains) != 3 or any(not math.isfinite(g) or not 0 <= g <= 1 for g in gains):
            raise ValueError('Invalid color gain')
        now = time.monotonic()
        if rescan or now >= self.next_scan or not self.state['active']:
            self.current = self.display.outputs()
            self.next_scan = now+1
        if not self.state['active']:
            self.state['outputs'] = {}
        self.last = gains
        for key, o in self.current.items():
            actual = self.display.get(o)
            old = self.state['outputs'].get(key)
            if old is None:
                old = {'baseline': actual, 'expected': None, 'previous': None}
                self.state['outputs'][key] = old
            elif not owned(old, actual):
                # Night Light, a color-profile update, or a display reset supplied
                # a fresh baseline. Compose with it instead of overwriting user settings.
                old['baseline'] = actual
            target = [[round(v*g) for v in channel] for channel,g in zip(old['baseline'],gains)]
            if actual == target:
                continue
            old['previous'] = old['expected']
            old['expected'] = target
            self.state['active'] = True
            # Journal before the call; guard handles death on either side of it.
            save(self.journal, self.state)
            self.display.put(o, target)
        if not self.state['active']:
            self.state['active'] = True
            save(self.journal, self.state)
        return len(self.current)

    def off(self):
        restore(self.journal, self.display)
        self.state['active'] = False
        self.last = None

    def check(self):
        if self.last is not None:
            return self.apply(self.last, rescan=True)
        return len(self.current)

    def close(self):
        try:
            self.off()
        finally:
            os.close(self.lease)
            try:
                self.watcher.wait(timeout=5)
            except subprocess.TimeoutExpired:
                pass  # The guard retains the lock while finishing restoration.


def main():
    backend = None
    def stop(*_): raise SystemExit(0)
    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        backend = Backend()
        print('OK '+str(len(backend.current)), flush=True)
        for line in sys.stdin:
            parts = line.split()
            try:
                if not parts: continue
                if parts[0] == 'set' and len(parts) == 4:
                    n = backend.apply([float(x) for x in parts[1:]])
                elif parts == ['check']:
                    n = backend.check()
                elif parts in (['off'], ['quit']):
                    backend.off(); n = len(backend.current)
                else: raise ValueError('Unknown GNOME color command')
                print('OK '+str(n), flush=True)
                if parts == ['quit']: break
            except Exception as exc:
                try: backend.off()
                except Exception as cleanup: print('gnome cleanup: '+str(cleanup), file=sys.stderr)
                print('ERR '+str(exc).replace('\n',' ')[:400], flush=True)
    except Exception as exc:
        print('ERR '+str(exc).replace('\n',' ')[:400], flush=True)
    finally:
        if backend is not None: backend.close()

if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--guard': guard(Path(sys.argv[2]))
    else: main()
