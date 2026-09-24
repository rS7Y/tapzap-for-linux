#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")"
[[ $(uname -s) == Linux ]] || { echo 'Build this trial on Linux.' >&2; exit 1; }
pkg-config --exists wayland-client cairo freetype2 x11 xext xrandr gio-2.0 dbus-1
command -v wayland-scanner >/dev/null
mkdir -p build
wayland-scanner client-header src/linux/ctm/protocols/hyprland-ctm-control-v1.xml build/ctm-client.h
wayland-scanner private-code src/linux/ctm/protocols/hyprland-ctm-control-v1.xml build/ctm-protocol.c
cc -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -Ibuild $(pkg-config --cflags wayland-client) \
  src/linux/ctm/engine.c build/ctm-protocol.c -o build/tapzap-ctm $(pkg-config --libs wayland-client) -lm
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -DHAVE_CAIRO -DHAVE_XRANDR -DHAVE_LOGIND \
  $(pkg-config --cflags cairo freetype2 x11 xext xrandr dbus-1) \
  src/linux/main.cpp src/linux/ui_cairo.cpp src/linux/gamma_hyprland.cpp \
  -o build/tapzap $(pkg-config --libs cairo freetype2 x11 xext xrandr dbus-1)
${CXX:-g++} -std=c++17 -O2 -Wall -Wextra $(pkg-config --cflags gio-2.0) \
  src/linux/tray.cpp -o build/tapzap-tray $(pkg-config --libs gio-2.0)
./build/tapzap selftest
