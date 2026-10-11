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

    def native_package(self):
        sums = self.package / "SHA256SUMS.json"
        hashes = json.loads(sums.read_text())
        for source in (VARIANT / "plugin").iterdir():
            target = self.package / "payload/plugin" / source.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
            hashes[str(target.relative_to(self.package))] = hashlib.sha256(
                target.read_bytes()
            ).hexdigest()
        sums.write_text(json.dumps(hashes))
        self.write_archive()

    def test_native_upgrade_removes_legacy_hook_and_keeps_user_input(self):
        config = self.prefix / "config/hypr/hyprland.lua"
        config.parent.mkdir(parents=True)
        original = '-- User configuration\nrequire("hypr.input")\n'
        config.write_text(original)
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        self.native_package()
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        plugin = self.prefix / "config/omarchy/plugins/app.tapzap"
        self.assertTrue(plugin.is_symlink())
        self.assertTrue((plugin / "Panel.qml").is_file())
        self.assertEqual(original, config.read_text())
        self.assertEqual(0, self.run_installer("--uninstall").returncode)
        self.assertFalse(plugin.is_symlink())
        self.assertEqual(original, config.read_text())

    def test_native_install_refuses_other_plugin_before_changes(self):
        self.native_package()
        plugin = self.prefix / "config/omarchy/plugins/app.tapzap"
        plugin.mkdir(parents=True)
        marker = plugin / "manifest.json"
        marker.write_text("OTHER PLUGIN")
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Another installation owns", result.stderr)
        self.assertEqual("OTHER PLUGIN", marker.read_text())
        self.assertFalse((self.prefix / "data").exists())

    def plugin_checkout(self, remote="https://github.com/rS7Y/tapzap-for-linux.git"):
        plugin = self.prefix / "config/omarchy/plugins/app.tapzap"
        plugin.mkdir(parents=True)
        subprocess.run(["git", "init", "-q", str(plugin)], check=True)
        subprocess.run(["git", "-C", str(plugin), "remote", "add", "origin", remote], check=True)
        (plugin / "manifest.json").write_text(
            json.dumps({"id": "app.tapzap", "entryPoints": {"barWidget": "Omarchy.qml"}})
        )
        (plugin / "Omarchy.qml").write_text("// Locally edited plugin\n")
        return plugin

    def test_native_install_and_remove_preserve_official_git_checkout_and_edits(self):
        self.native_package()
        plugin = self.plugin_checkout()
        before = {p.name: p.read_bytes() for p in plugin.iterdir() if p.is_file()}
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertFalse(plugin.is_symlink())
        self.assertTrue((self.prefix / "bin/tapzap").is_symlink())
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        self.assertEqual(0, self.run_installer("--uninstall").returncode)
        self.assertEqual(before, {p.name: p.read_bytes() for p in plugin.iterdir() if p.is_file()})
        self.assertTrue((plugin / ".git").is_dir())

    def test_native_install_refuses_lookalike_git_remote_before_changes(self):
        self.native_package()
        self.plugin_checkout("https://github.com/other/tapzap-for-linux.git")
        result = self.run_installer("--archive", self.archive)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("Another installation owns", result.stderr)
        self.assertFalse((self.prefix / "data").exists())

    def test_native_install_refuses_malformed_git_manifest_before_changes(self):
        self.native_package()
        plugin = self.plugin_checkout()
        for value in ([], {"id": "app.tapzap", "entryPoints": []}):
            with self.subTest(value=value):
                (plugin / "manifest.json").write_text(json.dumps(value))
                result = self.run_installer("--archive", self.archive)
                self.assertNotEqual(0, result.returncode)
                self.assertIn("Another installation owns", result.stderr)
                self.assertFalse((self.prefix / "data").exists())

    def live_installer(self):
        spec = importlib.util.spec_from_file_location("live_installer", VARIANT / "install.py")
        installer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(installer)
        return installer

    def test_locked_session_refuses_install_before_changes(self):
        self.native_package()
        installer = self.live_installer()
        with (
            patch.object(installer.platform, "system", return_value="Linux"),
            patch.object(installer.platform, "machine", return_value="x86_64"),
            patch.object(
                installer,
                "paths",
                return_value=(self.prefix / "data", self.prefix / "config", self.prefix / "bin"),
            ),
            patch.object(
                installer.shutil, "which", return_value="/usr/bin/omarchy-hyprland-session-locked"
            ),
            patch.object(
                installer.subprocess,
                "run",
                return_value=subprocess.CompletedProcess([], 0, stdout="", stderr=""),
            ),
        ):
            with self.assertRaisesRegex(RuntimeError, "Unlock"):
                installer.install(package=self.package)
        self.assertFalse(self.prefix.exists())

    def test_locked_session_refuses_uninstall_without_touching_install(self):
        self.native_package()
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        installer = self.live_installer()
        launcher = self.prefix / "bin/tapzap"
        before = launcher.resolve()
        with (
            patch.object(
                installer,
                "paths",
                return_value=(self.prefix / "data", self.prefix / "config", self.prefix / "bin"),
            ),
            patch.object(
                installer.shutil, "which", return_value="/usr/bin/omarchy-hyprland-session-locked"
            ),
            patch.object(
                installer.subprocess,
                "run",
                return_value=subprocess.CompletedProcess([], 0, stdout="", stderr=""),
            ),
        ):
            with self.assertRaisesRegex(RuntimeError, "Unlock"):
                installer.uninstall()
        self.assertEqual(before, launcher.resolve())
        self.assertTrue((self.prefix / "config/omarchy/plugins/app.tapzap").is_symlink())

    def test_session_check_refuses_unknown_state_and_allows_unlocked(self):
        installer = self.live_installer()
        for code in (1, 2):
            with (
                self.subTest(code=code),
                patch.object(
                    installer.shutil,
                    "which",
                    return_value="/usr/bin/omarchy-hyprland-session-locked",
                ),
                patch.object(
                    installer.subprocess, "run", return_value=subprocess.CompletedProcess([], code)
                ),
            ):
                if code == 1:
                    installer.require_unlocked_session(None)
                else:
                    with self.assertRaisesRegex(RuntimeError, "Cannot determine"):
                        installer.require_unlocked_session(None)

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

    def test_upgrade_backs_up_an_older_owned_icon(self):
        self.assertEqual(0, self.run_installer("--archive", self.archive).returncode)
        icon = self.prefix / "data/icons/hicolor/scalable/apps/tapzap-omarchy.svg"
        icon.write_text("older TAP ZAP icon")
        result = self.run_installer("--archive", self.archive)
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertEqual(
            (VARIANT / "integration/tapzap-omarchy.svg").read_bytes(), icon.read_bytes()
        )
        backups = self.prefix / "data/tapzap-omarchy-trial/backups"
        self.assertTrue(any(p.read_text() == "older TAP ZAP icon" for p in backups.iterdir()))

    def test_retries_do_not_take_ownership_of_an_unrelated_icon(self):
        icon = self.prefix / "data/icons/hicolor/scalable/apps/tapzap-omarchy.svg"
        icon.parent.mkdir(parents=True)
        icon.write_text("OTHER APP")
        for _ in range(2):
            result = self.run_installer("--archive", self.archive)
            self.assertNotEqual(0, result.returncode)
            self.assertEqual("OTHER APP", icon.read_text())
            self.assertFalse((self.prefix / "data/applications/tapzap-omarchy.desktop").exists())

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
