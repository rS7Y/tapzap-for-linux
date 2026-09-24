# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Keep public documentation and imported-source provenance internally consistent."""

import hashlib
import json
import re
import unittest
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]


class PublicRepoIntegrityTests(unittest.TestCase):
    def test_public_markdown_links_resolve_inside_the_repository(self):
        pages = [ROOT / "README.md", ROOT / "CONTRIBUTING.md", ROOT / "CHANGELOG.md"]
        pages += list((ROOT / "docs").rglob("*.md"))
        pages += list((ROOT / "THIRD-PARTY-NOTICES").rglob("*.md"))
        missing = []
        for page in pages:
            if not page.is_file():
                missing.append(str(page.relative_to(ROOT)))
                continue
            for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", page.read_text(encoding="utf-8")):
                parsed = urlsplit(target)
                if parsed.scheme or parsed.netloc or not parsed.path:
                    continue
                resolved = (page.parent / parsed.path).resolve()
                if not resolved.is_relative_to(ROOT) or not resolved.exists():
                    missing.append(f"{page.relative_to(ROOT)} -> {target}")
        self.assertEqual([], missing, "broken or unsafe local links: " + "; ".join(missing))

    def test_source_manifest_covers_every_imported_file_and_current_digest(self):
        manifest = json.loads((ROOT / "docs/SOURCE-MANIFEST.json").read_text(encoding="utf-8"))
        rows = manifest["files"]
        listed = {row["path"] for row in rows}
        actual = {path.relative_to(ROOT).as_posix() for path in (ROOT / "src").rglob("*")
                  if path.is_file() and "build" not in path.relative_to(ROOT).parts
                  and "dist" not in path.relative_to(ROOT).parts
                  and "evidence" not in path.relative_to(ROOT).parts}
        self.assertEqual(actual, listed, "source manifest must cover the complete imported source tree")
        for row in rows:
            with self.subTest(path=row["path"]):
                self.assertRegex(row["sha256"], r"^[0-9a-f]{64}$")
                self.assertRegex(row["source_sha256"], r"^[0-9a-f]{64}$")
                self.assertEqual(row["sha256"], hashlib.sha256((ROOT / row["path"]).read_bytes()).hexdigest())

    def test_public_manifest_does_not_disclose_private_source_paths(self):
        text = (ROOT / "docs/SOURCE-MANIFEST.json").read_text(encoding="utf-8")
        for forbidden in ("/Users/", "/Volumes/", "Documents/Codex", "TapZap2.swift", "TapZap2.cpp"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, text)


if __name__ == "__main__":
    unittest.main()
