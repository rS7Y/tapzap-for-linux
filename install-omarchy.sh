#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
command -v python3 >/dev/null || { echo 'Python 3 is required.' >&2; exit 1; }
# Bootstrap code is fixed to the reviewed 0.3.0 release source. Its installer
# selects the latest published package and verifies that archive separately.
installer_commit=837b5a1fb4d86c0f84c6783711a82c952c763d81
installer_sha256=c8006d3df40317ed521387d3fb35a4ec265470cd87432b3c27146a414b353b9e
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
  "https://raw.githubusercontent.com/rS7Y/tapzap-for-linux/$installer_commit/src/omarchy-hyprland/install.py" \
  --output "$temporary/install.py"
python3 - "$temporary/install.py" "$installer_sha256" <<'PY'
import hashlib
from pathlib import Path
import sys

if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest() != sys.argv[2]:
    sys.exit("Installer checksum mismatch; refusing to run downloaded code.")
PY
python3 "$temporary/install.py" --download "$@"
