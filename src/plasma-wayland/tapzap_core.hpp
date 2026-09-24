// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Tap Zap (Linux) — color engine core.
//
// Tap Zap Linux color-temperature curve and gamma-ramp construction.
// Pure double arithmetic, zero dependencies. This is the irreplaceable heart of the app:
// the CCT -> xy -> linear-sRGB chromaticity mapping plus the smoothstep red-tail to true 0K red.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tapzap {

struct RGB { double r, g, b; };

inline double clampd(double v, double lo, double hi) { return std::min(std::max(v, lo), hi); }

inline double mixd(double a, double b, double progress) {
    const double t = clampd(progress, 0.0, 1.0);
    return a + ((b - a) * t);
}

inline double smoothstep(double value) {
    const double t = clampd(value, 0.0, 1.0);
    return t * t * (3.0 - (2.0 * t));
}

constexpr double kMaximumKelvin     = 6500.0;
constexpr double kRedTailStartKelvin = 2000.0;

// CCT -> CIE xy (Kim et al. planckian-locus approximation). Verbatim coefficients from the Swift source.
inline void chromaticity(double kelvin, double& x, double& y) {
    const double t = clampd(kelvin, 1667.0, 25000.0);
    if (t <= 4000.0) {
        x = (-0.2661239e9 / std::pow(t, 3.0)) - (0.2343580e6 / std::pow(t, 2.0)) + (0.8776956e3 / t) + 0.179910;
    } else {
        x = (-3.0258469e9 / std::pow(t, 3.0)) + (2.1070379e6 / std::pow(t, 2.0)) + (0.2226347e3 / t) + 0.240390;
    }
    if (t <= 2222.0) {
        y = (-1.1063814 * std::pow(x, 3.0)) - (1.34811020 * std::pow(x, 2.0)) + (2.18555832 * x) - 0.20219683;
    } else if (t <= 4000.0) {
        y = (-0.9549476 * std::pow(x, 3.0)) - (1.37418593 * std::pow(x, 2.0)) + (2.09137015 * x) - 0.16748867;
    } else {
        y = (3.0817580 * std::pow(x, 3.0)) - (5.87338670 * std::pow(x, 2.0)) + (3.75112997 * x) - 0.37001483;
    }
}

// xy -> XYZ -> linear sRGB (D65). Kelvin clamped to [2000, 25000] before chromaticity.
inline RGB cctRGB(double kelvin) {
    const double chromaticKelvin = clampd(kelvin, kRedTailStartKelvin, 25000.0);
    double x, y;
    chromaticity(chromaticKelvin, x, y);
    const double X = x / y;
    const double Y = 1.0;
    const double Z = (1.0 - x - y) / y;
    const double red   = (3.2406 * X) + (-1.5372 * Y) + (-0.4986 * Z);
    const double green = (-0.9689 * X) + (1.8758 * Y) + (0.0415 * Z);
    const double blue  = (0.0557 * X) + (-0.2040 * Y) + (1.0570 * Z);
    return { clampd(red, 0.0, 4.0), clampd(green, 0.0, 4.0), clampd(blue, 0.0, 4.0) };
}

// 6500K white point, computed once (matches `private static let neutralRGB`).
inline const RGB& neutralRGB() {
    static const RGB n = cctRGB(kMaximumKelvin);
    return n;
}

inline RGB normalizedRGBFromCCT(double kelvin) {
    const RGB rgb = cctRGB(kelvin);
    const RGB& n = neutralRGB();
    return { clampd(rgb.r / n.r, 0.0, 1.0), clampd(rgb.g / n.g, 0.0, 1.0), clampd(rgb.b / n.b, 0.0, 1.0) };
}

inline double kelvinForIntensity(double intensity) {
    return kMaximumKelvin * (1.0 - clampd(intensity, 0.0, 1.0));
}

inline double intensityForKelvin(double kelvin) {
    return 1.0 - (clampd(kelvin, 0.0, kMaximumKelvin) / kMaximumKelvin);
}

// Top-level curve: warmth slider intensity (0..1) -> per-channel multiplicative factors.
// Verbatim port of ColorTemperatureCurve.normalizedRGB(forIntensity:).
inline RGB normalizedRGB(double intensity) {
    const double displayKelvin = kelvinForIntensity(intensity);
    if (displayKelvin <= kRedTailStartKelvin) {
        const RGB base = normalizedRGBFromCCT(kRedTailStartKelvin);
        const double progress = 1.0 - (displayKelvin / kRedTailStartKelvin);
        const double eased = smoothstep(progress);
        return { mixd(base.r, 1.0, eased), mixd(base.g, 0.0, eased), mixd(base.b, 0.0, eased) };
    }
    return normalizedRGBFromCCT(displayKelvin);
}

// ---- Gamma ramp construction (ColorFilterManager.applyFilter model) ----
// newRamp[c][i] = baseline[c][i] * factor[c] * brightness, clamped to [0,1].
// On a freshly-captured display the baseline is the identity ramp (i/(size-1)).
// XRandR / CGSetDisplayTransferByTable consume this as the per-CRTC LUT.

struct GammaRamp {
    std::vector<uint16_t> r, g, b;
    explicit GammaRamp(int size) : r(size, 0), g(size, 0), b(size, 0) {}
    int size() const { return static_cast<int>(r.size()); }
};

inline GammaRamp buildRamp(double intensity, double brightness, int size,
                           const std::vector<double>* baseR = nullptr,
                           const std::vector<double>* baseG = nullptr,
                           const std::vector<double>* baseB = nullptr) {
    const RGB f = normalizedRGB(intensity);
    const double br = clampd(brightness, 0.0, 1.0);
    GammaRamp ramp(size);
    for (int i = 0; i < size; ++i) {
        const double idn = (size > 1) ? static_cast<double>(i) / static_cast<double>(size - 1) : 0.0;
        const double bR = baseR ? (*baseR)[i] : idn;
        const double bG = baseG ? (*baseG)[i] : idn;
        const double bB = baseB ? (*baseB)[i] : idn;
        ramp.r[i] = static_cast<uint16_t>(std::lround(clampd(bR * f.r * br, 0.0, 1.0) * 65535.0));
        ramp.g[i] = static_cast<uint16_t>(std::lround(clampd(bG * f.g * br, 0.0, 1.0) * 65535.0));
        ramp.b[i] = static_cast<uint16_t>(std::lround(clampd(bB * f.b * br, 0.0, 1.0) * 65535.0));
    }
    return ramp;
}

// 8-bit LUT — exactly what the GPU/display applies to each framebuffer value.
// Used to faithfully simulate "what the screen looks like" on a captured image.
struct Lut8 { uint8_t r[256], g[256], b[256]; };

inline Lut8 buildLut8(double intensity, double brightness) {
    const RGB f = normalizedRGB(intensity);
    const double br = clampd(brightness, 0.0, 1.0);
    Lut8 lut;
    for (int i = 0; i < 256; ++i) {
        const double idn = static_cast<double>(i) / 255.0;
        lut.r[i] = static_cast<uint8_t>(std::lround(clampd(idn * f.r * br, 0.0, 1.0) * 255.0));
        lut.g[i] = static_cast<uint8_t>(std::lround(clampd(idn * f.g * br, 0.0, 1.0) * 255.0));
        lut.b[i] = static_cast<uint8_t>(std::lround(clampd(idn * f.b * br, 0.0, 1.0) * 255.0));
    }
    return lut;
}

} // namespace tapzap
