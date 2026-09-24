// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Gamma backend interface. Implementations:
//   - XRandR  (Linux/X11)        -> gamma_xrandr.cpp     [live screen tint]
//   - wlroots (Linux/Wayland)    -> gamma_wayland.cpp    [planned, see README]
//   - headless                   -> gamma_headless.cpp   [testing / non-Linux]
// The rest of the app is written against this interface, so the platform-specific
// gamma code is the only thing that changes between display servers.
#pragma once
#include "tapzap_core.hpp"
#include <memory>
#include <string>

namespace tapzap {

struct IGammaBackend {
    virtual ~IGammaBackend() = default;
    // Open the display server connection and capture each output's baseline ramp.
    virtual bool init(std::string& err) = 0;
    // Apply intensity (0..1 warmth) + brightness (0..1) across every active output.
    virtual bool setFilter(double intensity, double brightness, std::string& err) = 0;
    // Restore every output to its captured baseline.
    virtual bool restore(std::string& err) = 0;
    // Re-enumerate outputs and recapture baselines (call after a display reconfigure / wake).
    virtual bool refresh(std::string& err) { (void)err; return true; }
    // While enabled, repair display-server resets without replacing the original baseline.
    virtual bool maintainFilter(double intensity, double brightness, std::string& err) {
        (void)intensity; (void)brightness; (void)err; return true;
    }
    virtual std::string name() const = 0;
    virtual int outputCount() const = 0;
    virtual double displayScale() const { return 1.0; }
};

// Returns the best backend for the current platform/session (XRandR on Linux/X11, else headless).
std::unique_ptr<IGammaBackend> makeDefaultBackend();

} // namespace tapzap
