#!/usr/bin/env bash
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")"
wayland-scanner server-header src/linux/ctm/protocols/hyprland-ctm-control-v1.xml build/ctm-server.h
cc -std=c11 -O2 -Wall -Wextra -Ibuild $(pkg-config --cflags wayland-server) \
  tests/ctm_server.c build/ctm-protocol.c -o build/test-ctm-server $(pkg-config --libs wayland-server)
dbus-run-session -- "${TEST_PYTHON:-/usr/bin/python3}" tests/integration.py
