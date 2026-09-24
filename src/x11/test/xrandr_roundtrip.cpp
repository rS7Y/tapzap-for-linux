// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Linux/X11 hardware-path proof.
// Sets the engine's gamma ramp on a REAL X server via XRRSetCrtcGamma, reads it back with
// XRRGetCrtcGamma, and checks the server stored exactly what we sent. This is the closest
// possible proof of the live tint path without a physical panel to photograph (gamma is
// applied at scanout, so it never appears in a framebuffer screenshot anyway).
//
// Build (Linux): clang++ -std=c++17 -O2 -DHAVE_XRANDR test/xrandr_roundtrip.cpp -o build/xrandr_roundtrip \
//                $(pkg-config --cflags --libs xrandr x11)
#include "../tapzap_core.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace tapzap;

int main(int argc, char** argv) {
    const double intensity  = argc > 1 ? std::atof(argv[1]) : 1.0;
    const double brightness = argc > 2 ? std::atof(argv[2]) : 0.70;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { std::fprintf(stderr, "cannot open X display (is $DISPLAY set?)\n"); return 2; }
    int evb, erb, maj, min;
    if (!XRRQueryExtension(dpy, &evb, &erb) || !XRRQueryVersion(dpy, &maj, &min)) {
        std::fprintf(stderr, "RandR extension unavailable\n"); return 2;
    }
    std::printf("RandR %d.%d on display %s\n", maj, min, XDisplayString(dpy));
    std::printf("applying engine ramp: intensity=%.4f brightness=%.4f\n\n", intensity, brightness);

    XRRScreenResources* res = XRRGetScreenResources(dpy, DefaultRootWindow(dpy));
    if (!res) { std::fprintf(stderr, "XRRGetScreenResources failed\n"); return 2; }

    int tested = 0, exact = 0;
    for (int i = 0; i < res->ncrtc; ++i) {
        const RRCrtc crtc = res->crtcs[i];
        XRRCrtcInfo* ci = XRRGetCrtcInfo(dpy, res, crtc);
        const bool active = ci && ci->mode != None && ci->noutput > 0;
        if (ci) XRRFreeCrtcInfo(ci);
        if (!active) continue;
        const int size = XRRGetCrtcGammaSize(dpy, crtc);
        if (size <= 0) continue;
        ++tested;
        std::printf("CRTC %lu: gamma ramp size = %d\n", static_cast<unsigned long>(crtc), size);

        const GammaRamp ramp = buildRamp(intensity, brightness, size);
        XRRCrtcGamma* g = XRRAllocGamma(size);
        for (int j = 0; j < size; ++j) { g->red[j] = ramp.r[j]; g->green[j] = ramp.g[j]; g->blue[j] = ramp.b[j]; }
        XRRSetCrtcGamma(dpy, crtc, g);
        XRRFreeGamma(g);
        XSync(dpy, False);

        XRRCrtcGamma* rb = XRRGetCrtcGamma(dpy, crtc);
        if (!rb) { std::printf("  read-back failed\n"); continue; }
        long maxdiff = 0;
        for (int j = 0; j < size; ++j) {
            maxdiff = std::max(maxdiff, std::labs((long)rb->red[j]   - (long)ramp.r[j]));
            maxdiff = std::max(maxdiff, std::labs((long)rb->green[j] - (long)ramp.g[j]));
            maxdiff = std::max(maxdiff, std::labs((long)rb->blue[j]  - (long)ramp.b[j]));
        }
        const int idx[5] = {0, size / 4, size / 2, (3 * size) / 4, size - 1};
        for (int k = 0; k < 5; ++k) {
            const int j = idx[k];
            std::printf("    in[%4d]  sent (R=%5u G=%5u B=%5u)  read-back (R=%5u G=%5u B=%5u)\n",
                        j, ramp.r[j], ramp.g[j], ramp.b[j], rb->red[j], rb->green[j], rb->blue[j]);
        }
        std::printf("  set-vs-readback max |diff| = %ld -> %s\n\n", maxdiff,
                    maxdiff == 0 ? "EXACT (server stored our ramp verbatim)" : "stored with quantization");
        if (maxdiff == 0) ++exact;
        XRRFreeGamma(rb);
    }
    XRRFreeScreenResources(res);
    XCloseDisplay(dpy);
    std::printf("%d active CRTC(s) tested, %d exact round-trip(s)\n", tested, exact);
    return (tested > 0 && exact == tested) ? 0 : 1;
}
