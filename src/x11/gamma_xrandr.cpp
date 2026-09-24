// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// XRandR (X11) gamma backend — the live screen-tint path on Linux/X11.
// This is the direct analogue of CGSetDisplayTransferByTable (macOS) and the legacy
// SetDeviceGammaRamp path (Windows): a per-CRTC uint16 gamma LUT.
//
// Compiled only on Linux with libxrandr-dev + libx11-dev present (build.sh adds -DHAVE_XRANDR).
// It cannot build on this macOS dev box (no X11 headers); the headless backend stands in here.
#if defined(__linux__) && defined(HAVE_XRANDR)
#include "gamma_backend.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/dpms.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

namespace tapzap {

class XRandRBackend : public IGammaBackend {
public:
    ~XRandRBackend() override {
        std::string e;
        restore(e);
        if (dpy_) XCloseDisplay(dpy_);
    }
    std::string name() const override { return "xrandr"; }
    int outputCount() const override { return static_cast<int>(outs_.size()); }

    bool init(std::string& err) override {
        dpy_ = XOpenDisplay(nullptr);
        if (!dpy_) { err = "cannot open X display (is $DISPLAY set?)"; return false; }
        int evb, erb, maj, min;
        hasDpms_ = DPMSQueryExtension(dpy_, &evb, &erb) && DPMSCapable(dpy_);
        if (!XRRQueryExtension(dpy_, &evb, &erb) || !XRRQueryVersion(dpy_, &maj, &min)) {
            err = "RandR extension unavailable"; return false;
        }
        // Crash self-heal: on X11 an applied gamma ramp OUTLIVES the process (unlike macOS,
        // where WindowServer cleans up). If the previous run died while tinted (marker file
        // still present), the screen is stale-tinted right now — capturing it as "baseline"
        // would bake the tint in forever. Detect the marker, ignore the on-screen ramp, and
        // push a neutral identity baseline instead.
        crashDirty_ = (access(markerPath().c_str(), F_OK) == 0);
        if (!enumerate(err)) return false;
        if (crashDirty_) {
            std::string e;
            restore(e);   // pushes the (identity) baselines + clears the marker
            std::fprintf(stderr, "gamma: previous run exited dirty — reset screen to neutral\n");
        }
        crashDirty_ = false;
        return true;
    }

    // Re-enumerate CRTCs + recapture baselines (after a display reconfigure / wake). Restores
    // first so a still-active CRTC's applied filter isn't recaptured as the new baseline.
    bool refresh(std::string& err) override {
        if (!dpy_) { err = "no display"; return false; }
        std::string e;
        restore(e);
        return enumerate(err);
    }

    bool setFilter(double intensity, double brightness, std::string& err) override {
        for (auto& o : outs_) {
            const GammaRamp ramp = buildRamp(intensity, brightness, o.size, &o.baseR, &o.baseG, &o.baseB);
            XRRCrtcGamma* g = XRRAllocGamma(o.size);
            if (!g) { err = "XRRAllocGamma failed"; return false; }
            for (int j = 0; j < o.size; ++j) { g->red[j] = ramp.r[j]; g->green[j] = ramp.g[j]; g->blue[j] = ramp.b[j]; }
            XRRSetCrtcGamma(dpy_, o.crtc, g);
            XRRFreeGamma(g);
        }
        XFlush(dpy_);
        if (!markerSet_) {
            if (FILE* f = std::fopen(markerPath().c_str(), "w")) { std::fclose(f); markerSet_ = true; }
        }
        return true;
    }

    bool maintainFilter(double intensity, double brightness, std::string& err) override {
        if (!dpy_) { err = "no display"; return false; }
        bool woke = false;
        if (hasDpms_) {
            CARD16 level = DPMSModeOn;
            BOOL enabled = False;
            if (DPMSInfo(dpy_, &level, &enabled)) {
                const bool asleep = enabled && level != DPMSModeOn;
                woke = displayAsleep_ && !asleep;
                displayAsleep_ = asleep;
                if (asleep) return true; // Leave the desktop's display power state alone.
            }
        }
        bool changed = false;
        for (const auto& o : outs_) {
            const GammaRamp expected = buildRamp(intensity, brightness, o.size, &o.baseR, &o.baseG, &o.baseB);
            XRRCrtcGamma* current = XRRGetCrtcGamma(dpy_, o.crtc);
            if (!current) { err = "cannot read display gamma"; return false; }
            if (current->size != o.size) changed = true;
            else for (int j = 0; j < o.size; ++j) {
                // Permit one-bit rounding, but detect neutral ramps or a replaced filter.
                if (std::abs(int(current->red[j]) - int(expected.r[j])) > 1 ||
                    std::abs(int(current->green[j]) - int(expected.g[j])) > 1 ||
                    std::abs(int(current->blue[j]) - int(expected.b[j])) > 1) {
                    changed = true; break;
                }
            }
            XRRFreeGamma(current);
        }
        if (woke || changed) {
            std::fprintf(stderr, "gamma: restoring active filter after %s\n", woke ? "display wake" : "gamma reset");
            return setFilter(intensity, brightness, err);
        }
        return true;
    }

    bool restore(std::string&) override {
        if (!dpy_) return true;
        for (auto& o : outs_) {
            XRRCrtcGamma* g = XRRAllocGamma(o.size);
            if (!g) continue;
            for (int j = 0; j < o.size; ++j) {
                g->red[j]   = static_cast<unsigned short>(std::lround(o.baseR[j] * 65535.0));
                g->green[j] = static_cast<unsigned short>(std::lround(o.baseG[j] * 65535.0));
                g->blue[j]  = static_cast<unsigned short>(std::lround(o.baseB[j] * 65535.0));
            }
            XRRSetCrtcGamma(dpy_, o.crtc, g);
            XRRFreeGamma(g);
        }
        XFlush(dpy_);
        ::unlink(markerPath().c_str());
        markerSet_ = false;
        return true;
    }

private:
    // Per-boot marker (XDG_RUNTIME_DIR is tmpfs, cleared on reboot — matching gamma, which
    // also resets on reboot). Exists ⇔ a tint is applied and not yet cleanly restored.
    static std::string markerPath() {
        const char* rt = std::getenv("XDG_RUNTIME_DIR");
        if (rt && *rt) return std::string(rt) + "/tapzap.gamma.dirty";
        return "/tmp/tapzap.gamma.dirty." + std::to_string(getuid());
    }
    bool enumerate(std::string& err) {
        outs_.clear();
        Window root = DefaultRootWindow(dpy_);
        XRRScreenResources* res = XRRGetScreenResources(dpy_, root);
        if (!res) { err = "XRRGetScreenResources failed"; return false; }
        for (int i = 0; i < res->ncrtc; ++i) {
            const RRCrtc crtc = res->crtcs[i];
            XRRCrtcInfo* ci = XRRGetCrtcInfo(dpy_, res, crtc);
            const bool active = ci && ci->mode != None && ci->noutput > 0;
            if (ci) XRRFreeCrtcInfo(ci);
            if (!active) continue;
            const int size = XRRGetCrtcGammaSize(dpy_, crtc);
            if (size <= 0) continue;
            Out o;
            o.crtc = crtc;
            o.size = size;
            o.baseR.resize(size);
            o.baseG.resize(size);
            o.baseB.resize(size);
            XRRCrtcGamma* g = crashDirty_ ? nullptr : XRRGetCrtcGamma(dpy_, crtc);
            for (int j = 0; j < size; ++j) {
                if (g) {
                    o.baseR[j] = g->red[j]   / 65535.0;
                    o.baseG[j] = g->green[j] / 65535.0;
                    o.baseB[j] = g->blue[j]  / 65535.0;
                } else {
                    const double idn = (size > 1) ? static_cast<double>(j) / (size - 1) : 0.0;
                    o.baseR[j] = o.baseG[j] = o.baseB[j] = idn;
                }
            }
            if (g) XRRFreeGamma(g);
            outs_.push_back(std::move(o));
        }
        XRRFreeScreenResources(res);
        if (outs_.empty()) { err = "no active CRTC exposes a gamma ramp"; return false; }
        return true;
    }
    struct Out {
        RRCrtc crtc = 0;
        int size = 0;
        std::vector<double> baseR, baseG, baseB;
    };
    Display* dpy_ = nullptr;
    std::vector<Out> outs_;
    bool hasDpms_ = false;
    bool displayAsleep_ = false;
    bool crashDirty_ = false;  // true during init when the previous run exited without restoring
    bool markerSet_  = false;  // dirty-marker file currently on disk
};

std::unique_ptr<IGammaBackend> makeXRandRBackend() { return std::make_unique<XRandRBackend>(); }

} // namespace tapzap
#endif // __linux__ && HAVE_XRANDR
