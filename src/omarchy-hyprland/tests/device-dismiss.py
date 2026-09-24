#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Focused live Dell check; no suspend, monitor, filter or hardware regression suite."""
import json, os, select, subprocess, time
from pathlib import Path
root = Path.home() / 'TAPZAP-OMARCHY-ADDON-TRIAL'
env = dict(os.environ, **json.loads((root / 'session-env.json').read_text()), OMARCHY_PATH='/usr/share/omarchy')
launcher = str(Path.home() / '.local/bin/tapzap-omarchy')
def run(*args):
    return subprocess.run(args, env=env, text=True, capture_output=True, timeout=8, check=True).stdout
def status(cmd='status'):
    return json.loads(run(launcher, cmd))
cursor = json.loads(run('hyprctl', '-i', '0', '-j', 'cursorpos'))
settings = (Path.home() / '.config/TapZapOmarchy/settings.ini').read_bytes()
pointer = subprocess.Popen([str(root / 'pointer')], env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
results = {'checks': []}
def send(command):
    pointer.stdin.write(command + '\n'); pointer.stdin.flush()
    assert select.select([pointer.stdout], [], [], 3)[0], 'pointer timeout'
    assert pointer.stdout.readline().strip() == 'ok'
    time.sleep(.15)
def passed(name, **detail):
    results['checks'].append(dict(name=name, **detail))
    print('PASS', name, detail, flush=True)
def hidden():
    reply = status()
    assert not reply['visible'] and reply['enabled'] and not reply['error'], reply
try:
    for cycle in range(3):
        status('hide'); time.sleep(.2)
        send('click 1710 13'); time.sleep(.35)
        assert status()['visible'], 'Pinned tray activation did not open'
        send('click 1840 62'); assert status()['visible']  # gear
        send('click 1668 62'); assert status()['visible']  # back
        send('move 1400 700')
        before = status()['visible']
        send('click 1400 700'); hidden()
        passed('tray/settings/desktop click cycle ' + str(cycle + 1), visibleBeforeClick=before)
    status('show'); time.sleep(.35)
    # Drag brightness at its existing maximum, then release outside. Same value.
    send('move 1830 128'); send('press'); send('move 1400 128'); send('release')
    send('click 1400 700'); hidden()
    passed('Outside click after slider drag and outside release')
    status('show'); time.sleep(.35)
    send('move 1830 128'); send('press')
    status('hide'); send('release')
    status('show'); time.sleep(.35)
    send('move 1830 240')  # Must not adjust the slider after interrupted drag.
    assert (Path.home() / '.config/TapZapOmarchy/settings.ini').read_bytes() == settings
    send('click 1400 700'); hidden()
    passed('Hide during held drag resets drag state on reopen')
    status('show'); time.sleep(.35); send('click 1878 62'); hidden()
    passed('Header close after repeated interaction')
    status('show'); time.sleep(.35)
    assert (Path.home() / '.config/TapZapOmarchy/settings.ini').read_bytes() == settings
    assert not run('hyprctl', '-i', '0', 'configerrors').strip()
    binds = [b for b in json.loads(run('hyprctl', '-i', '0', '-j', 'binds')) if b.get('description', '').startswith('TAP ZAP trial:')]
    assert len(binds) == 3 and all(b['non_consuming'] for b in binds)
    results.update(status=status(), settingsPreserved=True, nativeWaylandPointer=True,
                   geometry=json.loads(run('omarchy-shell', 'shell', 'debugBarGeometry')),
                   configErrors='', registeredNonConsumingBindings=len(binds))
finally:
    send('release')
    send('move %d %d' % (cursor['x'], cursor['y']))
    pointer.stdin.close(); pointer.wait(timeout=5)
    (root / 'dismiss-fix-results.json').write_text(json.dumps(results, indent=2) + '\n')
