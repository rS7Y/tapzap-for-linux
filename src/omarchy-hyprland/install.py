#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Install this trial under the current user's data directory; preserve other builds."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parent

def quote_desktop(value):
    if '\n' in value or '\r' in value:
        raise ValueError('A desktop-entry path cannot contain a newline')
    return '"'+value.replace('\\','\\\\\\\\').replace('"','\\\\"').replace('`','\\\\`').replace('$','\\\\$').replace('%','%%')+'"'

def paths(prefix=None):
    if prefix:
        base=Path(prefix).resolve()
        return base/'data',base/'config',base/'bin'
    return (Path(os.environ.get('XDG_DATA_HOME',Path.home()/'.local/share')),
            Path(os.environ.get('XDG_CONFIG_HOME',Path.home()/'.config')),
            Path.home()/'.local/bin')

def checked_write(path,data):
    path.parent.mkdir(parents=True,exist_ok=True)
    if path.exists():
        if path.read_text()==data:return
        raise RuntimeError(f'Refusing to overwrite a different file: {path}')
    path.write_text(data)

def install(prefix=None):
    if platform.system()!='Linux' and not prefix:
        raise RuntimeError('Install the trial on Linux; --prefix is available for isolated staging tests')
    if platform.machine() not in ('x86_64','AMD64') and not prefix:
        raise RuntimeError('This archive contains x86_64 binaries')
    manifest=json.loads((ROOT/'SHA256SUMS.json').read_text())
    for relative,expected in manifest.items():
        p=(ROOT/relative).resolve()
        if not p.is_relative_to(ROOT) or hashlib.sha256(p.read_bytes()).hexdigest()!=expected:
            raise RuntimeError(f'Archive verification failed: {relative}')
    data,config,bin_dir=paths(prefix)
    version=hashlib.sha256((ROOT/'SHA256SUMS.json').read_bytes()).hexdigest()[:16]
    app_root=data/'tapzap-omarchy-trial'
    release=app_root/'releases'/version
    if release.exists():
        for p in (ROOT/'payload').rglob('*'):
            if p.is_file() and (not (release/p.relative_to(ROOT/'payload')).is_file() or
                               p.read_bytes()!=(release/p.relative_to(ROOT/'payload')).read_bytes()):
                raise RuntimeError(f'Existing trial release differs: {release}')
    else:
        release.parent.mkdir(parents=True,exist_ok=True)
        shutil.copytree(ROOT/'payload',release)
    current=app_root/'current'
    if current.exists() and not current.is_symlink():raise RuntimeError(f'Refusing to replace {current}')
    launcher=bin_dir/'tapzap-omarchy'
    if launcher.exists() or launcher.is_symlink():
        if not launcher.is_symlink() or os.readlink(launcher)!=str(current/'tapzap-omarchy'):
            raise RuntimeError(f'Another installation owns {launcher}')
    executable=str(current/'tapzap-omarchy')
    desktop=(ROOT/'integration/tapzap-omarchy.desktop').read_text()
    desktop=desktop.replace('Exec=tapzap-omarchy ',f'Exec={quote_desktop(executable)} ')
    desktop=desktop.replace('Icon=tapzap-omarchy',f'Icon={data}/icons/hicolor/scalable/apps/tapzap-omarchy.svg')
    checked_write(data/'applications/tapzap-omarchy.desktop',desktop)
    checked_write(data/'icons/hicolor/scalable/apps/tapzap-omarchy.svg',(ROOT/'integration/tapzap-omarchy.svg').read_text())
    pending=app_root/f'.current-{os.getpid()}'
    pending.symlink_to(release);pending.replace(current)
    bin_dir.mkdir(parents=True,exist_ok=True)
    if not launcher.is_symlink():launcher.symlink_to(current/'tapzap-omarchy')
    print(f'Installed trial: {release}')
    print('Open “TAP ZAP Omarchy” from the application launcher. It starts OFF on first use.')
    print('Launch at login is optional in Settings. Existing TAP ZAP builds and settings are preserved.')

def uninstall(prefix=None):
    data,config,bin_dir=paths(prefix)
    app_root=data/'tapzap-omarchy-trial'
    exe=app_root/'current/tapzap-omarchy'
    if not prefix and exe.exists():subprocess.run([str(exe),'quit'],timeout=8,check=False,capture_output=True)
    launcher=bin_dir/'tapzap-omarchy'
    if launcher.is_symlink() and os.readlink(launcher)==str(exe):launcher.unlink()
    desktop=data/'applications/tapzap-omarchy.desktop'
    if desktop.exists() and str(app_root) in desktop.read_text():desktop.unlink()
    login=config/'autostart/tapzap-omarchy.desktop'
    if login.exists() and str(app_root) in login.read_text():login.unlink()
    icon=data/'icons/hicolor/scalable/apps/tapzap-omarchy.svg'
    if icon.exists() and icon.read_bytes()==(ROOT/'integration/tapzap-omarchy.svg').read_bytes():icon.unlink()
    print('Removed trial launcher, icon and owned autostart entry. Trial releases and preferences remain for rollback.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',help='Stage integration files in an isolated directory instead of installing for this user')
    parser.add_argument('--uninstall',action='store_true')
    args=parser.parse_args()
    try:(uninstall if args.uninstall else install)(args.prefix)
    except (OSError,ValueError,RuntimeError,subprocess.TimeoutExpired) as error:
        print(f'Trial setup: {error}',file=sys.stderr);sys.exit(1)
