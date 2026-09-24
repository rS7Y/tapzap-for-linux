#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Refresh public hashes while retaining the reviewed source-snapshot hashes."""

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/SOURCE-MANIFEST.json"
ADDITIONAL_PROVENANCE = {
    "src/gnome-wayland/gnome_helper.py": (
        "desktop-tests-20260906/gnome",
        "5ee3a6a44d484d4dd5e928a4073ce321326515291e3ebbf0b09eb0ec7b2edf63",
    ),
    "src/plasma-wayland/make_profile.py": (
        "desktop-tests-20260906/plasma",
        "5847fcc6152f6b6bba84f6b6e07514cd7f9c13367787f0b40428e652442c4df0",
    ),
    "src/plasma-wayland/plasma_helper.py": (
        "desktop-tests-20260906/plasma",
        "fab17eeaa44d940dcb614439ba0c008ae18fa11d021790e4e22b17ec6d9c35ac",
    ),
}
PUBLIC_AUTHORED = {"src/omarchy-hyprland/README.md": "public-authored-2026-09-24"}


def main():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records = {row["path"]: row for row in manifest["files"]}
    source_files = sorted(path for path in (ROOT / "src").rglob("*")
                          if path.is_file() and "build" not in path.relative_to(ROOT).parts
                          and "dist" not in path.relative_to(ROOT).parts
                          and "evidence" not in path.relative_to(ROOT).parts)
    discovered = {path.relative_to(ROOT).as_posix() for path in source_files}
    unmapped = discovered - records.keys() - ADDITIONAL_PROVENANCE.keys() - PUBLIC_AUTHORED.keys()
    if unmapped:
        raise SystemExit("Unmapped source files: " + ", ".join(sorted(unmapped)))
    missing = records.keys() - discovered
    if missing:
        raise SystemExit("Manifest files are missing: " + ", ".join(sorted(missing)))
    for path in source_files:
        relative = path.relative_to(ROOT).as_posix()
        row = records.get(relative)
        if row is None:
            if relative in ADDITIONAL_PROVENANCE:
                snapshot, source_sha256 = ADDITIONAL_PROVENANCE[relative]
            else:
                snapshot = PUBLIC_AUTHORED[relative]
                source_sha256 = hashlib.sha256(path.read_bytes()).hexdigest()
            row = {"path": relative, "source_snapshot": snapshot, "source_sha256": source_sha256}
            records[relative] = row
        payload = path.read_bytes()
        row["bytes"] = len(payload)
        row["sha256"] = hashlib.sha256(payload).hexdigest()
    manifest["files"] = [records[path] for path in sorted(records)]
    manifest["derivation"] = [
        "source_sha256 identifies the reviewed preservation snapshot; sha256 and bytes identify the public file",
        "project-owned files received GPL-3.0-or-later SPDX headers after owner approval",
        "machine-specific proprietary-platform comments were removed without changing runtime behavior",
        "third-party protocol definitions and font files retain their original bytes and licenses",
    ]
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Updated {len(records)} source-manifest records")


if __name__ == "__main__":
    main()
