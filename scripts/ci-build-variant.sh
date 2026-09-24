#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

variant="${1:?usage: ci-build-variant.sh <x11|gnome-wayland|plasma-wayland|sway|omarchy-hyprland>}"
root="$(cd "$(dirname "$0")/.." && pwd)"
build_log="$(mktemp)"
trap 'rm -f "$build_log"' EXIT

case "$variant" in
  x11) markers=("XRandR gamma backend" "Cairo GUI") ;;
  gnome-wayland|sway) markers=("native wlr Wayland gamma control" "XRandR gamma backend" "Cairo GUI") ;;
  plasma-wayland) markers=("XRandR gamma backend" "Cairo GUI") ;;
  omarchy-hyprland)
    cd "$root/src/$variant"
    bash package.sh
    exit 0
    ;;
  *) echo "Unknown Linux variant: $variant" >&2; exit 2 ;;
esac

cd "$root/src/$variant"
CXX=g++ bash build.sh 2>&1 | tee "$build_log"
for marker in "${markers[@]}"; do
  if ! grep -Fq "$marker" "$build_log"; then
    echo "Required native build feature was omitted: $marker" >&2
    exit 1
  fi
done
test -x build/tapzap-tray || { echo "Required tray helper was not built" >&2; exit 1; }
./build/tapzap selftest
