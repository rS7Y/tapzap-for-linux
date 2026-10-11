#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
command -v python3 >/dev/null || { echo 'Python 3 is required.' >&2; exit 1; }
temporary=$(mktemp -d)
trap 'rm -rf "$temporary"' EXIT
curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
  https://raw.githubusercontent.com/rS7Y/tapzap-for-linux/main/src/omarchy-hyprland/install.py \
  --output "$temporary/install.py"
python3 "$temporary/install.py" --download "$@"
