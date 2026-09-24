// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TAPZAP_COLOR_H
#define TAPZAP_COLOR_H
#include <math.h>

/* Tap Zap Linux color-temperature transform.
 * The tail below 2000 is a blend to red, not a physical blackbody temperature. */
static inline double clamp(double v, double lo, double hi) {
    return fmin(hi, fmax(lo, v));
}
static inline void cct_rgb(double t, double rgb[3]) {
    t = clamp(t, 2000, 25000);
    double x = t <= 4000
        ? -0.2661239e9 / (t*t*t) - 0.2343580e6 / (t*t) + 0.8776956e3 / t + 0.179910
        : -3.0258469e9 / (t*t*t) + 2.1070379e6 / (t*t) + 0.2226347e3 / t + 0.240390;
    double y = t <= 2222
        ? -1.1063814*x*x*x - 1.34811020*x*x + 2.18555832*x - 0.20219683
        : t <= 4000
        ? -0.9549476*x*x*x - 1.37418593*x*x + 2.09137015*x - 0.16748867
        : 3.0817580*x*x*x - 5.87338670*x*x + 3.75112997*x - 0.37001483;
    double X = x/y, Z = (1-x-y)/y;
    rgb[0] = clamp(3.2406*X - 1.5372 - 0.4986*Z, 0, 4);
    rgb[1] = clamp(-0.9689*X + 1.8758 + 0.0415*Z, 0, 4);
    rgb[2] = clamp(0.0557*X - 0.2040 + 1.0570*Z, 0, 4);
}
static inline void tapzap_rgb(double intensity, double brightness, double rgb[3]) {
    double kelvin = 6500 * (1 - intensity), neutral[3];
    cct_rgb(6500, neutral);
    cct_rgb(fmax(2000, kelvin), rgb);
    double p = clamp(1 - kelvin/2000, 0, 1);
    double eased = p*p*(3-2*p);
    for (int c = 0; c < 3; c++) {
        double base = clamp(rgb[c]/neutral[c], 0, 1);
        rgb[c] = brightness * (base + ((c == 0 ? 1 : 0) - base)*eased);
    }
}
#endif
