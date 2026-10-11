# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the user-facing archive installer against isolated home directories."""

import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
VARIANT = ROOT / "src/omarchy-hyprland"


class OmarchyInstallTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / "user with spaces"
        self.package = self.root / "tapzap-omarchy-test"
        files = {
            "LICENSE": (ROOT / "LICENSE").read_bytes(),
            "payload/tapzap-omarchy": (VARIANT / "integration/tapzap-omarchy").read_bytes(),
            "payload/bin/tapzap": b"#!/bin/sh\nexit 0\n",
            "payload/outside-click.lua": (VARIANT / "integration/outside-click.lua").read_bytes(),
            "install.py": (VARIANT / "install.py").read_bytes(),
            "BUILD-INFO.json": json.dumps(
                {"version": "0.2.0", "source_revision": "a" * 40}
            ).encode(),
        }
        for name in ("tapzap-omarchy.desktop", "tapzap-omarchy.svg"):
            files["integration/" + name] = (VARIANT / "integration" / name).read_bytes()
        for name, data in files.items():
            path = self.package / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            if name.startswith("payload/") and not name.endswith(".lua"):
                path.chmod(0o755)
        (self.package / "SHA256SUMS.json").write_text(
            json.dumps({name: hashlib.sha256(data).hexdigest() for name, data in files.items()})
        )
        self.archive = self.root / "addon.tar.gz"
        self.write_archive()

    def write_archive(self):
        with tarfile.open(self.archive, "w:gz") as archive:
            archive.add(self.package, arcname=self.package.name)
        Path(str(self.archive) + ".sha256").write_text(
            hashlib.sha256(self.archive.read_bytes()).hexdigest() + "  addon.tar.gz\n"
        )

    def run_installer(self, *args):
        return subprocess.run(
            [
                sys.executable,
                str(VARIANT / "install.py"),
                "--prefix",
                str(self.prefix),
                *map(str, args),
            ],
            capture_output=True,
            text=True,
            timeout=15,
        )

    def test_one_command_installs_archive_and_short_launcher(self):
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        launcher = self.prefix / "bin/tapzap"
        self.assertTrue(launcher.is_symlink())
        self.assertEqual(launcher.resolve(), (self.prefix / "bin/tapzap-omarchy").resolve())
        release = launcher.resolve().parent
        self.assertTrue((release / "setup/install.py").is_file())
        self.assertIn(
            "TAP ZAP", (self.prefix / "data/applications/tapzap-omarchy.desktop").read_text()
        )

    def test_update_and_uninstall_preserve_user_config_and_other_commands(self):
        config = self.prefix / "config/hypr/hyprland.lua"
        config.parent.mkdir(parents=True)
        original = '-- Personal config\nrequire("hypr.input")\n'
        config.write_text(original)
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        first_config = config.read_text()
        self.assertIn("outside-click.lua", first_config)
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        self.assertEqual(first_config, config.read_text())
        prefs = self.prefix / "config/TapZapOmarchy/settings.ini"
        prefs.parent.mkdir(parents=True)
        prefs.write_text("enabled=0\nbrightness=0.85\n")
        result = self.run_installer("--uninstall")
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(original, config.read_text())
        self.assertEqual("enabled=0\nbrightness=0.85\n", prefs.read_text())
        self.assertFalse((self.prefix / "bin/tapzap").is_symlink())
        self.assertFalse((self.prefix / "bin/tapzap-omarchy").is_symlink())

    def test_archive_payload_tampering_is_refused_before_install(self):
        (self.package / "payload/bin/tapzap").write_text("tampered")
        self.write_archive()
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("verification", result.stderr.lower())
        self.assertFalse((self.prefix / "data").exists())

    def test_existing_unrelated_short_command_is_preserved(self):
        command = self.prefix / "bin/tapzap"
        command.parent.mkdir(parents=True)
        command.write_text("OTHER APP\n")
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual("OTHER APP\n", command.read_text())
        self.assertTrue((self.prefix / "bin/tapzap-omarchy").is_symlink())

    def test_reinstall_refuses_a_changed_installed_manager(self):
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        release = (self.prefix / "bin/tapzap").resolve().parent
        (release / "setup/install.py").write_text("changed manager")
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("release differs", result.stderr)

    def test_upgrade_keeps_previous_release_and_preferences(self):
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        before = (self.prefix / "bin/tapzap").resolve()
        prefs = self.prefix / "config/TapZapOmarchy/settings.ini"
        prefs.parent.mkdir(parents=True)
        prefs.write_text("enabled=0\nbrightness=0.85\n")
        info = self.package / "BUILD-INFO.json"
        info.write_text(json.dumps({"version": "0.2.1", "source_revision": "b" * 40}))
        sums = self.package / "SHA256SUMS.json"
        hashes = json.loads(sums.read_text())
        hashes["BUILD-INFO.json"] = hashlib.sha256(info.read_bytes()).hexdigest()
        sums.write_text(json.dumps(hashes))
        self.write_archive()
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertNotEqual(before, (self.prefix / "bin/tapzap").resolve())
        self.assertTrue(before.is_file())
        self.assertEqual("enabled=0\nbrightness=0.85\n", prefs.read_text())

    def test_download_selects_published_omarchy_archive_and_verifies_it(self):
        spec = importlib.util.spec_from_file_location("addon_installer", VARIANT / "install.py")
        installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(installer)
        name = "tapzap-omarchy-0.2.0-linux-x86_64.tar.gz"
        archive_url = "https://example.test/" + name
        releases = [
            {"draft": True, "tag_name": "omarchy-v0.3.0", "assets": []},
            {"tag_name": "unrelated-v1", "assets": []},
            {
                "tag_name": "omarchy-v0.2.0",
                "assets": [
                    {"name": name, "browser_download_url": archive_url},
                    {"name": name + ".sha256", "browser_download_url": archive_url + ".sha256"},
                ],
            },
        ]
        responses = {
            installer.RELEASES_API: json.dumps(releases).encode(),
            archive_url: self.archive.read_bytes(),
            archive_url + ".sha256": Path(str(self.archive) + ".sha256").read_bytes(),
        }
        with patch.object(installer, "download", side_effect=responses.__getitem__):
            installer.install_download(self.prefix)
        self.assertTrue((self.prefix / "bin/tapzap").resolve().is_file())

    def test_archive_checksum_mismatch_does_not_change_install(self):
        self.archive.write_bytes(self.archive.read_bytes() + b"changed")
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("checksum mismatch", result.stderr)
        self.assertFalse(self.prefix.exists())

    def test_archive_symlink_is_refused_before_extraction(self):
        with tarfile.open(self.archive, "w:gz") as archive:
            link = tarfile.TarInfo("escape")
            link.type = tarfile.SYMTYPE
            link.linkname = str(self.prefix)
            archive.addfile(link)
        Path(str(self.archive) + ".sha256").write_text(
            hashlib.sha256(self.archive.read_bytes()).hexdigest()
        )
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("unsafe package entry", result.stderr)
        self.assertFalse(self.prefix.exists())


if __name__ == "__main__":
    unittest.main()
