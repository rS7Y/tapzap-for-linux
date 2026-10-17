#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the separate Pop!_OS prototype DEB without modifying installed files."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess

HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--cosmic', type=Path, required=True)
parser.add_argument('--tapzap', type=Path, required=True)
parser.add_argument('--tapzap-source', type=Path, required=True)
parser.add_argument('--start-cosmic', type=Path, default=Path('/usr/bin/start-cosmic'))
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
assert platform.system() == 'Linux' and platform.machine() == 'x86_64'
out = args.output.resolve()
out.mkdir(exist_ok=False)
root = out / 'package'
prefix = root / 'opt/tapzap-cosmic-prototype'
(prefix / 'bin').mkdir(parents=True)
(prefix / 'libexec').mkdir()
(prefix / 'share').mkdir()
for source, name in [(args.cosmic, 'cosmic-comp'), (args.tapzap, 'tapzap-cosmic')]:
    assert source.is_file() and not source.is_symlink(), source
    shutil.copy2(source, prefix / 'libexec' / name)
shutil.copytree(args.tapzap_source / 'src/sway/assets', prefix / 'share/assets')
shutil.copy2(HERE / 'tapzap-cosmic', prefix / 'bin/tapzap-cosmic')
(prefix / 'bin/tapzap-cosmic').chmod(0o755)
start = args.start_cosmic.read_text()
assert start.count('/usr/bin/cosmic-session') == 2
assert start.count('set -e\n') == 1
start = start.replace('set -e\n', 'set -e\nexport TAPZAP_COSMIC_ENABLE=1\n', 1)
start = start.replace('/usr/bin/cosmic-session', '/usr/bin/cosmic-session /opt/tapzap-cosmic-prototype/libexec/cosmic-comp')
launcher = prefix / 'bin/start-cosmic'
launcher.write_text(start)
launcher.chmod(0o755)
subprocess.run(['bash', '-n', str(launcher)], check=True)
for directory, name, command, extra in [
    ('wayland-sessions', 'TAP ZAP COSMIC (prototype)', '/opt/tapzap-cosmic-prototype/bin/start-cosmic', 'DesktopNames=COSMIC;\n'),
    ('applications', 'TAP ZAP COSMIC Prototype', '/opt/tapzap-cosmic-prototype/bin/tapzap-cosmic', 'Categories=Utility;\n')]:
    path = root / 'usr/share' / directory / 'tapzap-cosmic-prototype.desktop'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('[Desktop Entry]\nType=Application\nName=' + name + '\nComment=Experimental patched COSMIC session; VM validated only\nExec=' + command + '\nIcon=tapzap-cosmic-prototype\nTerminal=false\n' + extra)
icon = root / 'usr/share/icons/hicolor/scalable/apps/tapzap-cosmic-prototype.svg'
icon.parent.mkdir(parents=True)
shutil.copy2(args.tapzap_source / 'src/omarchy-hyprland/integration/tapzap-omarchy.svg', icon)
doc = root / 'usr/share/doc/tapzap-cosmic-prototype'
doc.mkdir(parents=True)
shutil.copy2(HERE / 'README.md', doc / 'README.md')
shutil.copy2(args.tapzap_source / 'LICENSE', doc / 'GPL-3')
shutil.copy2(args.tapzap_source / 'src/sway/assets/OFL.txt', doc / 'OFL.txt')
(doc / 'copyright').write_text('TAP ZAP: GPL-3.0-or-later. Patched cosmic-comp: GPL-3.0-only.\nAnybody fonts: SIL OFL 1.1. No shared system libraries are bundled.\nSee GPL-3, OFL.txt and the adjacent complete source archive with Cargo vendor sources and their original license notices.\n')
# Derive actual runtime library requirements from both native binaries.
control = 'Source: tapzap-cosmic-prototype\nSection: utils\nPriority: optional\nMaintainer: Tap Zap <support@tapzap.app>\n\nPackage: tapzap-cosmic-prototype\nArchitecture: amd64\nDescription: Experimental TAP ZAP patched COSMIC prototype\n'
(out / 'debian').mkdir()
(out / 'debian/control').write_text(control)
deps = subprocess.check_output(['dpkg-shlibdeps', '-O', '-e' + str((prefix / 'libexec/cosmic-comp').resolve()), '-e' + str((prefix / 'libexec/tapzap-cosmic').resolve())], cwd=out, text=True)
assert deps.startswith('shlibs:Depends=')
depends = deps.strip().split('=', 1)[1] + ', cosmic-session, cosmic-comp'
(root / 'DEBIAN').mkdir()
(root / 'DEBIAN/control').write_text('Package: tapzap-cosmic-prototype\nVersion: 0.1.0\nArchitecture: amd64\nSection: utils\nPriority: optional\nMaintainer: Tap Zap <support@tapzap.app>\nDepends: ' + depends + '\nDescription: Experimental TAP ZAP patched COSMIC prototype\n Separate selectable session; stock COSMIC files remain installed.\n Pop!_OS 24.04 amd64 VM validation only.\n')
archive = out / 'tapzap-cosmic-prototype_0.1.0_amd64.deb'
subprocess.run(['dpkg-deb', '--root-owner-group', '--build', str(root), str(archive)], check=True)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
metadata = {'filename': archive.name, 'sha256': sha(archive), 'bytes': archive.stat().st_size, 'native_binaries': {name: sha(prefix / 'libexec' / name) for name in ['cosmic-comp', 'tapzap-cosmic']}, 'runtime_dependencies': depends, 'prototype': True, 'physical_hardware_validated': False}
(out / 'package-info.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(json.dumps(metadata, indent=2))
