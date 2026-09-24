// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Tap Zap (Linux) — CLI harness around the color engine + gamma backend.
//
// Subcommands:
//   selftest                                  verify the curve + ramp invariants (exit 1 on failure)
//   csv [N]                                    emit N curve samples (for diffing vs the macOS source)
//   dump-ramp <intensity> <bright> [size]      print the uint16 LUT pushed to the GPU
//   gradient <out.ppm> [w h]                   render the warmth slider as an image
//   swatches <out.ppm>                         render OFF / DAY / EVENING / NIGHT tints
//   apply <intensity> <bright> <in.ppm> <out>  faithfully simulate the filter over an image
//   run  <intensity> <bright>                  drive the live gamma backend (real tint on Linux/X11)
#include "gamma_backend.hpp"
#include "ppm.hpp"
#include "tapzap_core.hpp"
#include "ipc.hpp"
#include "ui.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace tapzap;

static int rgb8(double v) { return static_cast<int>(std::lround(clampd(v, 0.0, 1.0) * 255.0)); }

static void printRow(const char* label, double intensity, double brightness) {
    const double k = kelvinForIntensity(intensity);
    const RGB f = normalizedRGB(intensity);
    std::printf("  %-8s i=%.4f  K=%6.0f  factor=(%.4f, %.4f, %.4f)  8-bit=(%3d,%3d,%3d)  bright=%.2f\n",
                label, intensity, k, f.r, f.g, f.b, rgb8(f.r), rgb8(f.g), rgb8(f.b), brightness);
}

static int cmd_selftest() {
    std::printf("Tap Zap - color engine selftest (faithful port of macOS ColorTemperatureCurve)\n\n");
    bool ok = true;
    auto check = [&](bool c, const char* m) { std::printf("  [%s] %s\n", c ? "PASS" : "FAIL", m); if (!c) ok = false; };

    const RGB n = normalizedRGB(0.0);
    check(std::fabs(n.r - 1.0) < 1e-9 && std::fabs(n.g - 1.0) < 1e-9 && std::fabs(n.b - 1.0) < 1e-9,
          "intensity 0.0 -> neutral white (1,1,1)");

    const RGB red = normalizedRGB(1.0);
    check(std::fabs(red.r - 1.0) < 1e-12 && red.g == 0.0 && red.b == 0.0,
          "intensity 1.0 -> true 0K red (1,0,0): zero green, zero blue");

    check(std::fabs(kelvinForIntensity(0.0) - 6500.0) < 1e-9, "intensity 0.0 maps to 6500K");
    check(std::fabs(kelvinForIntensity(1.0) - 0.0) < 1e-9, "intensity 1.0 maps to 0K");

    const double dayI = intensityForKelvin(4000.0);
    const double eveI = intensityForKelvin(2700.0);
    check(std::fabs(kelvinForIntensity(dayI) - 4000.0) < 1e-6, "DAY preset round-trips to 4000K");
    check(std::fabs(kelvinForIntensity(eveI) - 2700.0) < 1e-6, "EVENING preset round-trips to 2700K");

    double prevB = 2.0;
    bool mono = true, inrange = true;
    for (int i = 0; i <= 1000; ++i) {
        const RGB c = normalizedRGB(i / 1000.0);
        if (c.r < -1e-12 || c.r > 1 + 1e-12 || c.g < -1e-12 || c.g > 1 + 1e-12 || c.b < -1e-12 || c.b > 1 + 1e-12) inrange = false;
        if (c.b > prevB + 1e-6) mono = false;
        prevB = c.b;
    }
    check(inrange, "all RGB factors stay within [0,1] across the full slider");
    check(mono, "blue channel never rises as warmth increases (monotonic cut)");

    const GammaRamp r0 = buildRamp(0.0, 1.0, 256);
    check(r0.r[255] == 65535 && r0.g[255] == 65535 && r0.b[255] == 65535,
          "OFF ramp top entry = (65535,65535,65535) - identity");
    const GammaRamp r1 = buildRamp(1.0, 1.0, 256);
    check(r1.r[255] == 65535 && r1.g[255] == 0 && r1.b[255] == 0,
          "NIGHT ramp top entry = (65535,0,0) - green+blue fully cut");

    std::printf("\n  Curve samples (warmth slider, factors = screen tint of pure white):\n");
    printRow("OFF", 0.0, 1.0);
    printRow("DAY", dayI, 1.0);
    printRow("EVENING", eveI, 0.85);
    printRow("(0.80)", 0.80, 1.0);
    printRow("NIGHT", 1.0, 0.70);

    std::printf("\nSELFTEST: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int cmd_csv(int n) {
    if (n < 2) n = 1001;
    for (int i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(n - 1);
        const RGB c = normalizedRGB(t);
        std::printf("%d,%.10f,%.4f,%.17g,%.17g,%.17g\n", i, t, kelvinForIntensity(t), c.r, c.g, c.b);
    }
    return 0;
}

static int cmd_dump_ramp(double intensity, double brightness, int size) {
    if (size < 2) size = 256;
    const GammaRamp r = buildRamp(intensity, brightness, size);
    std::printf("# gamma LUT  intensity=%.4f brightness=%.4f size=%d (uint16, fed to XRandR/CoreGraphics)\n",
                intensity, brightness, size);
    const int idx[5] = {0, size / 4, size / 2, (3 * size) / 4, size - 1};
    for (int j = 0; j < 5; ++j) {
        const int i = idx[j];
        std::printf("  in[%4d]  ->  R=%5u  G=%5u  B=%5u\n", i, r.r[i], r.g[i], r.b[i]);
    }
    return 0;
}

static int cmd_gradient(const char* out, int w, int h) {
    if (w < 2) w = 1100;
    if (h < 1) h = 240;
    Image img; img.w = w; img.h = h; img.px.resize(static_cast<size_t>(w) * h * 3);
    for (int x = 0; x < w; ++x) {
        const double t = static_cast<double>(x) / static_cast<double>(w - 1);
        const RGB c = normalizedRGB(t);
        const uint8_t R = static_cast<uint8_t>(rgb8(c.r)), G = static_cast<uint8_t>(rgb8(c.g)), B = static_cast<uint8_t>(rgb8(c.b));
        for (int y = 0; y < h; ++y) {
            const size_t p = (static_cast<size_t>(y) * w + x) * 3;
            img.px[p] = R; img.px[p + 1] = G; img.px[p + 2] = B;
        }
    }
    if (!writePPM(out, img)) { std::fprintf(stderr, "write failed\n"); return 1; }
    std::printf("wrote %s (%dx%d): warmth slider 0.0 (left, 6500K white) -> 1.0 (right, 0K red)\n", out, w, h);
    return 0;
}

static int cmd_swatches(const char* out) {
    struct P { double i, b; };
    const double dayI = intensityForKelvin(4000.0);
    const double eveI = intensityForKelvin(2700.0);
    const P presets[4] = {{0.0, 1.0}, {dayI, 1.0}, {eveI, 0.85}, {1.0, 0.70}};
    const int sw = 260, h = 260, gap = 12, n = 4;
    const int w = sw * n + gap * (n - 1);
    Image img; img.w = w; img.h = h; img.px.assign(static_cast<size_t>(w) * h * 3, 13); // #0D0D0D bg
    for (int k = 0; k < n; ++k) {
        const RGB c = normalizedRGB(presets[k].i);
        const uint8_t R = static_cast<uint8_t>(rgb8(c.r * presets[k].b));
        const uint8_t G = static_cast<uint8_t>(rgb8(c.g * presets[k].b));
        const uint8_t B = static_cast<uint8_t>(rgb8(c.b * presets[k].b));
        const int x0 = k * (sw + gap);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < sw; ++x) {
                const size_t p = (static_cast<size_t>(y) * w + (x0 + x)) * 3;
                img.px[p] = R; img.px[p + 1] = G; img.px[p + 2] = B;
            }
    }
    if (!writePPM(out, img)) { std::fprintf(stderr, "write failed\n"); return 1; }
    std::printf("wrote %s: OFF | DAY 4000K | EVENING 2700K | NIGHT 0K (each at its preset brightness)\n", out);
    return 0;
}

static int cmd_apply(double intensity, double brightness, const char* in, const char* out) {
    Image img;
    if (!readPPM(in, img)) { std::fprintf(stderr, "cannot read %s (expect binary P6 PPM, maxval 255)\n", in); return 1; }
    const Lut8 lut = buildLut8(intensity, brightness);
    for (size_t p = 0; p + 2 < img.px.size(); p += 3) {
        img.px[p]     = lut.r[img.px[p]];
        img.px[p + 1] = lut.g[img.px[p + 1]];
        img.px[p + 2] = lut.b[img.px[p + 2]];
    }
    if (!writePPM(out, img)) { std::fprintf(stderr, "write failed\n"); return 1; }
    std::printf("wrote %s: applied filter (i=%.3f, bright=%.2f) to %s through the 8-bit gamma LUT\n",
                out, intensity, brightness, in);
    return 0;
}

static int cmd_run(double intensity, double brightness) {
    std::string err;
    auto be = makeDefaultBackend();
    if (!be->init(err)) { std::fprintf(stderr, "backend init failed: %s\n", err.c_str()); return 1; }
    std::printf("backend: %s  (outputs: %d)\n", be->name().c_str(), be->outputCount());
    if (!be->setFilter(intensity, brightness, err)) { std::fprintf(stderr, "setFilter failed: %s\n", err.c_str()); return 1; }
    std::printf("applied filter i=%.3f bright=%.2f across %d output(s)\n", intensity, brightness, be->outputCount());
    cmd_dump_ramp(intensity, brightness, 256);
    be->restore(err);
    std::printf("restored baseline.\n");
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf(
            "Tap Zap (Linux) - usage:\n"
            "  tapzap selftest\n"
            "  tapzap csv [N]\n"
            "  tapzap dump-ramp <intensity> <brightness> [size]\n"
            "  tapzap gradient <out.ppm> [w h]\n"
            "  tapzap swatches <out.ppm>\n"
            "  tapzap apply <intensity> <brightness> <in.ppm> <out.ppm>\n"
            "  tapzap run <intensity> <brightness>\n"
            "  tapzap gui [intensity brightness enabled]      (Linux/X11 + Cairo)\n"
            "  tapzap on|off|toggle|show                      (control a running instance)\n");
        return 2;
    }
    const std::string c = argv[1];
    if (c == "gui") return runGui(argc, argv);
    if (c == "on" || c == "off" || c == "toggle" || c == "show") {
        if (tapzap::ipcSend(c)) return 0;
        std::fprintf(stderr, "Tap Zap is not running (start it with: tapzap gui)\n");
        return 1;
    }
    if (c == "selftest") return cmd_selftest();
    if (c == "csv") return cmd_csv(argc > 2 ? std::atoi(argv[2]) : 1001);
    if (c == "dump-ramp" && argc >= 4) return cmd_dump_ramp(std::atof(argv[2]), std::atof(argv[3]), argc > 4 ? std::atoi(argv[4]) : 256);
    if (c == "gradient" && argc >= 3) return cmd_gradient(argv[2], argc > 4 ? std::atoi(argv[3]) : 0, argc > 4 ? std::atoi(argv[4]) : 0);
    if (c == "swatches" && argc >= 3) return cmd_swatches(argv[2]);
    if (c == "apply" && argc >= 6) return cmd_apply(std::atof(argv[2]), std::atof(argv[3]), argv[4], argv[5]);
    if (c == "run" && argc >= 4) return cmd_run(std::atof(argv[2]), std::atof(argv[3]));
    std::fprintf(stderr, "unknown or incomplete command: %s\n", argv[1]);
    return 2;
}
