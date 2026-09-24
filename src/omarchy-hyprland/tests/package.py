#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
import tarfile
import tempfile


SOURCE = Path(__file__).resolve().parents[3]
archive = Path(sys.argv[1]).resolve()
checksum = Path(str(archive) + ".sha256").read_text(encoding="utf-8").split()[0]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == checksum, "archive checksum mismatch"

temp = Path(tempfile.mkdtemp(prefix="tapzap-omarchy-package-")).resolve()
with tarfile.open(archive) as tar:
    members = tar.getmembers()
    for item in members:
        name = PurePosixPath(item.name)
        assert not name.is_absolute() and ".." not in name.parts, item.name
        assert item.isfile() or item.isdir(), f"unexpected archive link or device: {item.name}"
    tar.extractall(temp)

pkg = temp / archive.name.removesuffix(".tar.gz")
info = json.loads((pkg / "BUILD-INFO.json").read_text(encoding="utf-8"))
revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True).strip()
manifest_hash = hashlib.sha256((SOURCE / "docs/SOURCE-MANIFEST.json").read_bytes()).hexdigest()
assert info["source_revision"] == revision
assert info["source_manifest_sha256"] == manifest_hash
assert (pkg / "LICENSE").is_file() and (pkg / "THIRD-PARTY-NOTICES/README.md").is_file()
assert (pkg / "docs/KNOWN-ISSUES.md").is_file() and (pkg / "SECURITY.md").is_file()

manifest = json.loads((pkg / "SHA256SUMS.json").read_text(encoding="utf-8"))
for relative, expected in manifest.items():
    path = (pkg / relative).resolve()
    assert path.is_relative_to(pkg) and path.is_file(), relative
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, relative

spec = importlib.util.spec_from_file_location("trial_installer", pkg / "install.py")
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)
prefix = temp / "user with spaces"
legacy = prefix / "config/autostart/tapzap.desktop"
legacy.parent.mkdir(parents=True)
legacy.write_text("OTHER BUILD\n", encoding="utf-8")
installer.install(prefix)
launcher = prefix / "bin/tapzap-omarchy"
assert launcher.is_symlink() and launcher.resolve().is_file()
before = launcher.resolve()
desktop = prefix / "data/applications/tapzap-omarchy.desktop"
assert 'Exec="' in desktop.read_text(encoding="utf-8")
installer.install(prefix)
assert launcher.resolve() == before
prefs = prefix / "config/TapZapOmarchy/settings.ini"
prefs.parent.mkdir(parents=True)
prefs.write_text("enabled=0\n", encoding="utf-8")
installer.uninstall(prefix)
assert not launcher.exists() and not desktop.exists()
assert before.exists() and prefs.exists() and legacy.read_text(encoding="utf-8") == "OTHER BUILD\n"
print("PASS: archive checksum, source revision, notices, payload hashes, isolated install/uninstall")
