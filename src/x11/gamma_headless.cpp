// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Headless gamma backend + platform backend selector.
// The headless backend builds the exact ramps the real backend would push, but keeps them
// in memory (no display server). It lets the full daemon path run + be tested on any OS,
// including this macOS dev box where XRandR does not exist.
#include "gamma_backend.hpp"

namespace tapzap {

class HeadlessBackend : public IGammaBackend {
public:
    std::string name() const override { return "headless"; }
    int outputCount() const override { return 1; }
    bool init(std::string&) override { return true; }
    bool setFilter(double intensity, double brightness, std::string&) override {
        last_ = buildRamp(intensity, brightness, kSize);
        active_ = true;
        return true;
    }
    bool restore(std::string&) override { active_ = false; return true; }
    const GammaRamp& lastRamp() const { return last_; }
    bool active() const { return active_; }
private:
    static constexpr int kSize = 256;
    GammaRamp last_{kSize};
    bool active_ = false;
};

#if defined(__linux__) && defined(HAVE_XRANDR)
std::unique_ptr<IGammaBackend> makeXRandRBackend();
#endif

std::unique_ptr<IGammaBackend> makeDefaultBackend() {
#if defined(__linux__) && defined(HAVE_XRANDR)
    return makeXRandRBackend();
#else
    return std::make_unique<HeadlessBackend>();
#endif
}

} // namespace tapzap
