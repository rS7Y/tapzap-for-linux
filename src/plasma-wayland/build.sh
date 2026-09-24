#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
# Build the Tap Zap Linux engine. Same source compiles on:
#   - Linux + libxrandr-dev + libx11-dev  -> live XRandR gamma backend
#   - any other platform (e.g. this macOS dev box) -> headless backend
set -euo pipefail
cd "$(dirname "$0")"

CXX="${CXX:-clang++}"
FLAGS="-std=c++17 -O2 -Wall -Wextra"
SRC="main.cpp gamma_headless.cpp ui_cairo.cpp"
LIBS=""
HAVE_PC=0
command -v pkg-config >/dev/null 2>&1 && HAVE_PC=1

if [ "$(uname -s)" = "Linux" ]; then
  if [ "$HAVE_PC" = 1 ] && pkg-config --exists dbus-1; then
    FLAGS="$FLAGS -DHAVE_LOGIND $(pkg-config --cflags dbus-1)"
    LIBS="$LIBS $(pkg-config --libs dbus-1)"
    echo "[build] + logind hardware brightness control"
  fi
  if [ "$HAVE_PC" = 1 ] && pkg-config --exists xrandr x11 xext xfixes; then
    SRC="$SRC gamma_xrandr.cpp gamma_plasma.cpp"
    FLAGS="$FLAGS -DHAVE_XRANDR -DHAVE_XFIXES $(pkg-config --cflags xrandr x11 xext xfixes)"
    LIBS="$LIBS $(pkg-config --libs xrandr x11 xext xfixes)"
    echo "[build] + XRandR gamma backend (live screen tint) + click-through overlay (XFixes)"
  else
    echo "[build] libxrandr-dev/libx11-dev not found -> headless gamma only (sudo apt install libxrandr-dev libx11-dev)"
  fi
  if [ "$HAVE_PC" = 1 ] && pkg-config --exists cairo freetype2; then
    FLAGS="$FLAGS -DHAVE_CAIRO $(pkg-config --cflags cairo freetype2)"
    LIBS="$LIBS $(pkg-config --libs cairo freetype2)"
    echo "[build] + Cairo GUI (Anybody fonts via FreeType)"
  else
    echo "[build] libcairo2-dev/libfreetype6-dev not found -> GUI stub only (sudo apt install libcairo2-dev libfreetype6-dev)"
  fi
else
  echo "[build] $(uname -s): headless gamma + GUI stub (XRandR/Cairo are Linux-only)"
fi

mkdir -p build
# shellcheck disable=SC2086
$CXX $FLAGS $SRC -o build/tapzap $LIBS
echo "[build] -> $(pwd)/build/tapzap"

# Optional system-tray helper (separate process; GTK main loop + StatusNotifierItem).
if [ "$(uname -s)" = "Linux" ] && [ "$HAVE_PC" = 1 ] && pkg-config --exists gtk+-3.0 ayatana-appindicator3-0.1; then
  # shellcheck disable=SC2086
  $CXX -std=c++17 -O2 -Wall $(pkg-config --cflags gtk+-3.0 ayatana-appindicator3-0.1) tray.cpp \
    -o build/tapzap-tray $(pkg-config --libs gtk+-3.0 ayatana-appindicator3-0.1)
  echo "[build] -> $(pwd)/build/tapzap-tray (system tray)"
elif [ "$(uname -s)" = "Linux" ]; then
  echo "[build] tray deps not found -> no tray (sudo apt install libgtk-3-dev libayatana-appindicator3-dev)"
fi
