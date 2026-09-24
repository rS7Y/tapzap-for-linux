#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify public source completeness, hashes, and private-path boundaries."""

import hashlib
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/SOURCE-MANIFEST.json"
TEXT_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".lua", ".md", ".py", ".sh", ".svg", ".txt", ".xml", ".yml", ".yaml"}
FORBIDDEN = ("/Users/", "/Volumes/", "Documents/Codex", "swift_reference.swift", "TapZap2.swift", "TapZap2.cpp")


def fail(message):
    raise SystemExit("FAIL: " + message)


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    rows = manifest.get("files", [])
    listed = {row.get("path") for row in rows}
    if len(listed) != len(rows) or None in listed:
        fail("source manifest contains duplicate or empty paths")
    actual = {path.relative_to(ROOT).as_posix() for path in (ROOT / "src").rglob("*")
              if path.is_file() and "build" not in path.relative_to(ROOT).parts
              and "dist" not in path.relative_to(ROOT).parts
              and "evidence" not in path.relative_to(ROOT).parts
              and "__pycache__" not in path.relative_to(ROOT).parts
              and path.suffix != ".pyc"}
    if actual != listed:
        fail(f"source manifest mismatch: missing={sorted(actual-listed)} extra={sorted(listed-actual)}")
    for row in rows:
        relative = Path(row["path"])
        if relative.is_absolute() or ".." in relative.parts:
            fail("unsafe manifest path: " + row["path"])
        path = ROOT / relative
        payload = path.read_bytes()
        digest = hashlib.sha256(payload).hexdigest()
        if digest != row.get("sha256") or len(payload) != row.get("bytes"):
            fail("public file hash/size mismatch: " + row["path"])
        if not re.fullmatch(r"[0-9a-f]{64}", row.get("source_sha256", "")):
            fail("invalid preservation hash: " + row["path"])
    public_trees = (ROOT / "src", ROOT / "docs", ROOT / ".github", ROOT / "THIRD-PARTY-NOTICES")
    public_files = [path for tree in public_trees for path in tree.rglob("*") if path.is_file()]
    public_files += [ROOT / name for name in ("README.md", "CONTRIBUTING.md", "CHANGELOG.md", "SECURITY.md")
                     if (ROOT / name).is_file()]
    for path in public_files:
        if path.suffix not in TEXT_SUFFIXES:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        for token in FORBIDDEN:
            if token in text:
                fail(f"private/proprietary reference {token!r} in {path.relative_to(ROOT)}")
    notices = (ROOT / "THIRD-PARTY-NOTICES/README.md").read_text(encoding="utf-8")
    if ("OFL-1.1" not in notices or "wlr-gamma-control" not in notices
            or "No runtime system libraries are bundled" not in notices):
        fail("third-party notice register is incomplete")
    license_text = (ROOT / "LICENSE").read_text(encoding="utf-8")
    if "Version 3, 29 June 2007" not in license_text:
        fail("GPL-3.0-or-later license text is missing")
    print(f"PASS: {len(rows)} imported source/asset files verified; no private-path patterns found")


if __name__ == "__main__":
    main()
