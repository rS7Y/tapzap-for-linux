#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the prototype's real autostart helper without touching user settings."""
import argparse
import os
from pathlib import Path
import subprocess
import socket
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--include', type=Path, required=True)
args = parser.parse_args()
with tempfile.TemporaryDirectory() as work:
    root = Path(work)
    source = root / 'autostart.cpp'
    source.write_text('#include "platform.hpp"\nint main(int argc,char**){return tapzap::setLaunchAtLogin(argc==1)?0:1;}\n')
    binary = root / 'autostart'
    subprocess.run(['c++','-std=c++17','-I',str(args.include.resolve()),str(source),'-o',str(binary)],check=True)
    desktop = root / 'desktop'
    (desktop / 'autostart').mkdir(parents=True)
    existing = desktop / 'autostart/tapzap.desktop'
    existing.write_bytes(b'production autostart sentinel\n')
    env = dict(os.environ, XDG_CONFIG_HOME=str(root / 'config'), TAPZAP_DESKTOP_CONFIG_HOME=str(desktop), TAPZAP_LAUNCHER='/opt/tapzap-cosmic-prototype/bin/tapzap-cosmic')
    subprocess.run([str(binary)],env=env,check=True)
    assert existing.read_bytes()==b'production autostart sentinel\n', 'Prototype replaced another TAP ZAP autostart entry'
    candidate=desktop/'autostart/tapzap-cosmic-prototype.desktop'
    assert candidate.is_file(), 'Prototype autostart entry is missing'
    assert '/opt/tapzap-cosmic-prototype/bin/tapzap-cosmic' in candidate.read_text()
    subprocess.run([str(binary),'disable'],env=env,check=True)
    assert not candidate.exists(), 'Disable did not remove prototype autostart'
    assert existing.read_bytes()==b'production autostart sentinel\n', 'Disable removed another TAP ZAP autostart entry'
    print('PASS: enable/disable preserves the existing TAP ZAP autostart entry')
    source.write_text('#include "ipc.hpp"\nint main(int argc,char**){if(argc==1)return tapzap::ipcSend("off")?0:1;int fd=tapzap::ipcListen();if(fd<0)return 2;tapzap::ipcClose(fd);return 0;}\n')
    subprocess.run(['c++','-std=c++17','-I',str(args.include.resolve()),str(source),'-o',str(binary)],check=True)
    runtime=root/'runtime';runtime.mkdir()
    env['XDG_RUNTIME_DIR']=str(runtime)
    legacy=runtime/'tapzap.sock'
    with socket.socket(socket.AF_UNIX) as server:
        server.bind(str(legacy));server.listen(1)
        inode=legacy.stat().st_ino
        result=subprocess.run([str(binary)],env=env)
        assert result.returncode==1, 'Prototype sent OFF to another TAP ZAP instance'
        subprocess.run([str(binary),'listen'],env=env,check=True)
        assert legacy.stat().st_ino==inode, 'Prototype replaced another TAP ZAP IPC socket'
    print('PASS: IPC does not control or unlink the ordinary TAP ZAP socket')
