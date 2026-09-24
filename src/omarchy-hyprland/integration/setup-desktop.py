#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Opt-in integration for the installed user-local trial on Omarchy's Lua shell."""
import copy
import datetime
import json
from pathlib import Path

home = Path.home()
shell = home / '.config/omarchy/shell.json'
hypr = home / '.config/hypr/hyprland.lua'
hook = home / '.local/share/tapzap-omarchy-trial/current/outside-click.lua'
assert hook.is_file() and shell.is_file() and hypr.is_file()
before = json.loads(shell.read_text())
after = copy.deepcopy(before)
found = False
for entries in after['bar']['layout'].values():
    for entry in entries:
        if entry.get('id') == 'omarchy.tray':
            found = True
            pinned = entry.setdefault('pinned', [])
            if 'tapzap-omarchy' not in pinned:
                pinned.append('tapzap-omarchy')
            if 'hidden' in entry:
                entry['hidden'] = [v for v in entry['hidden'] if v != 'tapzap-omarchy']
assert found, 'Existing Omarchy tray entry required'
start = '-- BEGIN TAP ZAP Omarchy outside-click integration'
end = '-- END TAP ZAP Omarchy outside-click integration'
block = start + '''
do
  local path = os.getenv("HOME") .. "/.local/share/tapzap-omarchy-trial/current/outside-click.lua"
  local file = io.open(path, "r")
  if file then file:close(); dofile(path) end
end
''' + end
old = hypr.read_text()
if start in old:
    assert block in old, 'An existing TAP ZAP integration differs; inspect it first'
    new = old
else:
    new = old.rstrip() + '\n\n' + block + '\n'
backup = home / 'TAPZAP-OMARCHY-ADDON-TRIAL' / ('desktop-backup-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
backup.mkdir(parents=True)
(backup / 'shell.json').write_bytes(shell.read_bytes())
(backup / 'hyprland.lua').write_bytes(hypr.read_bytes())
if after != before:
    pending = shell.with_name('.tapzap-shell.json')
    pending.write_text(json.dumps(after, indent=2) + '\n')
    pending.replace(shell)
if new != old:
    pending = hypr.with_name('.tapzap-hyprland.lua')
    pending.write_text(new)
    pending.replace(hypr)
print(json.dumps({'backup': str(backup), 'pinned': True, 'outsideClickHook': str(hook)}))
