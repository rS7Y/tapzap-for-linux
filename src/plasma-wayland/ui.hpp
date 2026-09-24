// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Entry point for the Tap Zap GUI. Implemented in ui_cairo.cpp (Linux + X11 + Cairo);
// a stub elsewhere. `tapzap gui` runs the live window; `tapzap gui --shot ...` renders
// one UI frame offscreen to a PPM (same draw code as the live window).
#pragma once
int runGui(int argc, char** argv);
