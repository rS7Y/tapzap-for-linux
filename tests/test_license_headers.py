# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Guard the project's GPL notice and third-party license boundaries."""

import hashlib
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT_EXTENSIONS = {".c", ".cpp", ".h", ".hpp", ".lua", ".py", ".sh", ".desktop", ".svg"}
VARIANTS = (
    "src/x11",
    "src/gnome-wayland",
    "src/plasma-wayland",
    "src/sway",
    "src/omarchy-hyprland",
)


class LicenseBoundaryTests(unittest.TestCase):
    def test_project_source_files_have_the_gpl_spdx_identifier(self):
        files = [path for base in (ROOT / "src", ROOT / "tests")
                 for path in base.rglob("*")
                 if path.is_file() and path.suffix in PROJECT_EXTENSIONS]
        self.assertTrue(files, "project source files should be present")
        missing = [str(path.relative_to(ROOT)) for path in files
                   if "SPDX-License-Identifier: GPL-3.0-or-later" not in
                   "\n".join(path.read_text(encoding="utf-8").splitlines()[:8])]
        self.assertEqual([], missing, "missing GPL headers: " + ", ".join(missing))

    def test_license_file_is_the_unmodified_gpl_v3_text(self):
        license_file = ROOT / "LICENSE"
        self.assertTrue(license_file.is_file(), "LICENSE is required")
        text = license_file.read_text(encoding="utf-8")
        self.assertIn("GNU GENERAL PUBLIC LICENSE", text)
        self.assertIn("Version 3, 29 June 2007", text)
        self.assertIn("https://www.gnu.org/licenses/", text)
        self.assertNotIn("YOUR NAME", text)

    def test_third_party_notice_registers_separate_licenses(self):
        notice = ROOT / "THIRD-PARTY-NOTICES/README.md"
        self.assertTrue(notice.is_file(), "third-party notice register is required")
        text = notice.read_text(encoding="utf-8")
        for component in ("Anybody", "OFL-1.1", "wlr-gamma-control",
                          "Hyprland CTM", "wlr virtual pointer",
                          "No runtime system libraries are bundled"):
            with self.subTest(component=component):
                self.assertIn(component, text)

    def test_font_notices_are_present_and_identical_in_each_variant(self):
        notices = []
        for variant in VARIANTS:
            notice = ROOT / variant / "src/linux/assets/OFL.txt" if variant.endswith("omarchy-hyprland") else ROOT / variant / "assets/OFL.txt"
            self.assertTrue(notice.is_file(), f"missing {notice.relative_to(ROOT)}")
            self.assertIn("SIL OPEN FONT LICENSE Version 1.1", notice.read_text(encoding="utf-8"))
            notices.append(hashlib.sha256(notice.read_bytes()).hexdigest())
        self.assertEqual(1, len(set(notices)), "all variants must retain the same upstream OFL text")


if __name__ == "__main__":
    unittest.main()
