// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Tap Zap GUI — pure Xlib + Cairo (no GTK/Qt). Uses the shared 320x540 layout
// and #0D0D0D body with #1A1A1A header/footer panels, Anybody
// fonts (loaded from assets/ via FreeType), inner-groove faders with mirrored ticks + metallic
// caps, black readout pills, 2-line flicker warning, gradient presets, bolt-glyph ZAP.
// One render() drives both the live X11 window and the offscreen `--shot` PPM mode.
#include "ui.hpp"

#if defined(__linux__) && defined(HAVE_CAIRO)
#include "gamma_backend.hpp"
#include "ipc.hpp"
#include "platform.hpp"
#include "ppm.hpp"
#include "pwm.hpp"
#include "tapzap_core.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/extensions/shape.h>
#include <X11/Xatom.h>
#include <X11/Xresource.h>
#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif
#ifdef HAVE_XRANDR
#include <X11/extensions/Xrandr.h>
#endif
#include <cairo-ft.h>
#include <cairo-xlib.h>
#include <cairo.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <cerrno>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/select.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/wait.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace tapzap;

namespace {

// ---- logical layout (320x540), positions measured from the real v2.25 popover ----
constexpr double W = 320, H = 540;
constexpr double HEADER_H = 56;
constexpr double HPAD = 24;
constexpr double TEMP_CX = 82, BRIGHT_CX = 238;
constexpr double LABEL_BASE = 84;
constexpr double TRACK_TOP = 116, TRACK_BOTTOM = 292;     // trackHeight = 176
constexpr double TRACK_OUTER_W = 14, TRACK_INNER_W = 8;
constexpr double CAP_W = 52, CAP_H = 28;
constexpr double READOUT_CY = 324, READOUT_H = 24, RW_WARM = 84, RW_BRIGHT = 64;
constexpr double DIV_TOP = 110, DIV_BOTTOM = 300;
constexpr double FOOTER_TOP = 356;
constexpr double WARN_Y1 = 380, WARN_Y2 = 394;
constexpr double PRESET_TOP = 420, PRESET_H = 36;
constexpr double ZAP_TOP = 476, ZAP_H = 48;
constexpr double BRIGHT_FLOOR = 0.10;
constexpr double TRACK_H = TRACK_BOTTOM - TRACK_TOP;

const char* PRESET_NAME[3] = {"DAY", "EVENING", "NIGHT"};
const double PRESET_K[3] = {4000.0, 2700.0, 0.0};
const double PRESET_B[3] = {1.0, 0.85, 0.70};

int settingsLinkAt(double x, double y) {
    if (x >= 80 && x <= W - 80 && y >= 474 && y <= 499) return 1;
    if (x >= 110 && x <= W - 110 && y >= 502 && y <= 530) return 2;
    return 0;
}

struct UiState {
    double intensity = 1.0;
    double brightness = 0.70;
    bool enabled = false;
    int view = 0;                 // 0 = main, 1 = settings
    bool allowExtremeDim = false;
    bool launchAtLogin = false;
    bool loginError = false;
    bool pwmSafe = false;         // PWM-safe toggle requested
    int pwmStatus = 0;            // -1 failed, 0 off, 1 activating, 2 verified-on
    bool pwmAvailable = false;    // a controllable display exists -> show toggle, else flicker warning
    int flicker = 1;             // flicker warning level when no PWM toggle: 0 none, 1 mild, 2 severe
    int copiedToast = 0;          // >0 = show "COPIED" on the feedback button
    bool frameless = std::getenv("TAPZAP_FRAMELESS") != nullptr;
    bool closeHovered = false;
    int linkHovered = 0;
    int hardwarePercent = -1;
};

// ---- fonts ----
struct Fonts {
    FT_Library ft = nullptr;
    cairo_font_face_t* anybodyXB = nullptr; // ExtraBold: title, WARMTH/BRIGHTNESS, ZAP
    cairo_font_face_t* anybodyB = nullptr;  // Bold: presets
    cairo_font_face_t* mono = nullptr;      // readouts / warning (system monospaced stand-in)
    bool loaded = false;
};
Fonts g_fonts;

std::string exeDir() {
    char buf[4096];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return ".";
    buf[n] = 0;
    std::string p(buf);
    auto s = p.rfind('/');
    return s == std::string::npos ? "." : p.substr(0, s);
}

cairo_font_face_t* loadFace(const std::string& path) {
    if (!g_fonts.ft) return nullptr;
    FT_Face face;
    if (FT_New_Face(g_fonts.ft, path.c_str(), 0, &face) != 0) return nullptr;
    return cairo_ft_font_face_create_for_ft_face(face, 0);
}

void ensureFonts() {
    if (g_fonts.loaded) return;
    g_fonts.loaded = true;
    if (FT_Init_FreeType(&g_fonts.ft) != 0) g_fonts.ft = nullptr;
    std::string assets = exeDir() + "/../assets";
    if (const char* e = std::getenv("TAPZAP_ASSETS")) assets = e;
    g_fonts.anybodyXB = loadFace(assets + "/Anybody-ExtraBold.ttf");
    g_fonts.anybodyB = loadFace(assets + "/Anybody-Bold.ttf");
    const char* monos[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu/DejaVuSansMono-Bold.ttf"};
    for (const char* m : monos) {
        if (access(m, R_OK) == 0) { g_fonts.mono = loadFace(m); break; }
    }
}

inline void setc(cairo_t* cr, int r, int g, int b, double a = 1.0) { cairo_set_source_rgba(cr, r / 255.0, g / 255.0, b / 255.0, a); }

void useFont(cairo_t* cr, cairo_font_face_t* f, double size) {
    if (f) cairo_set_font_face(cr, f);
    else cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, size);
}

// Copy one full UTF-8 sequence from p into ch (NUL-terminated), return its byte length.
// Per-glyph iteration must never split multi-byte sequences: cairo_show_text on a lone
// continuation byte is invalid UTF-8, which puts the cairo context into a *sticky* error
// state and silently freezes all further drawing (the settings-view "·" footer bug).
static int utf8Next(const char* p, char ch[5]) {
    const unsigned char c = (unsigned char)*p;
    int n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    int i = 0;
    for (; i < n && p[i]; ++i) ch[i] = p[i];
    ch[i] = 0;
    return i;
}

double textWidth(cairo_t* cr, const char* s, double tracking) {
    double w = 0;
    for (const char* p = s; *p; ) {
        char ch[5];
        p += utf8Next(p, ch);
        cairo_text_extents_t te;
        cairo_text_extents(cr, ch, &te);
        w += te.x_advance + tracking;
    }
    return w > 0 ? w - tracking : 0;
}

// draw text; align 0=left(x), 1=center(x); returns end x
void drawText(cairo_t* cr, const char* s, double x, double baseline, double tracking, int align) {
    double tw = textWidth(cr, s, tracking);
    double cx = (align == 1) ? x - tw / 2 : x;
    for (const char* p = s; *p; ) {
        char ch[5];
        p += utf8Next(p, ch);
        cairo_text_extents_t te;
        cairo_text_extents(cr, ch, &te);
        cairo_move_to(cr, cx, baseline);
        cairo_show_text(cr, ch);
        cx += te.x_advance + tracking;
    }
}

void rrect(cairo_t* cr, double x, double y, double w, double h, double r) {
    const double d = M_PI / 180.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r, r, -90 * d, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0, 90 * d);
    cairo_arc(cr, x + r, y + h - r, r, 90 * d, 180 * d);
    cairo_arc(cr, x + r, y + r, r, 180 * d, 270 * d);
    cairo_close_path(cr);
}

void vgrad(cairo_t* cr, double x, double y, double w, double h, double r,
           double r0, double g0, double b0, double a0, double r1, double g1, double b1, double a1) {
    cairo_pattern_t* p = cairo_pattern_create_linear(0, y, 0, y + h);
    cairo_pattern_add_color_stop_rgba(p, 0, r0, g0, b0, a0);
    cairo_pattern_add_color_stop_rgba(p, 1, r1, g1, b1, a1);
    rrect(cr, x, y, w, h, r);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
}

void drawGear(cairo_t* cr, double cx, double cy, double r, int cr_, int cg, int cb, double a) {
    const int teeth = 8;
    const double rin = r * 0.72, rhole = r * 0.40;
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
    cairo_new_path(cr);
    const int steps = teeth * 2;
    for (int i = 0; i <= steps; ++i) {
        double ang = (double)i / steps * 2 * M_PI - M_PI / 2;
        double rr = (i % 2 == 0) ? r : rin;
        double px = cx + rr * std::cos(ang), py = cy + rr * std::sin(ang);
        if (i == 0) cairo_move_to(cr, px, py);
        else cairo_line_to(cr, px, py);
    }
    cairo_close_path(cr);
    cairo_new_sub_path(cr);
    cairo_arc(cr, cx, cy, rhole, 0, 2 * M_PI);
    setc(cr, cr_, cg, cb, a);
    cairo_fill(cr);
    cairo_set_fill_rule(cr, CAIRO_FILL_RULE_WINDING);
}

void drawBolt(cairo_t* cr, double cx, double cy, double h, int cr_, int cg, int cb) {
    double w = h * 0.6, x = cx - w / 2, y = cy - h / 2;
    const double pts[][2] = {{0.52, 0.0}, {0.12, 0.56}, {0.42, 0.56}, {0.30, 1.0}, {0.90, 0.40}, {0.56, 0.40}};
    cairo_new_path(cr);
    for (int i = 0; i < 6; ++i) {
        double px = x + pts[i][0] * w, py = y + pts[i][1] * h;
        if (i == 0) cairo_move_to(cr, px, py);
        else cairo_line_to(cr, px, py);
    }
    cairo_close_path(cr);
    setc(cr, cr_, cg, cb);
    cairo_fill(cr);
}

double capCenterY(double value) { return TRACK_BOTTOM - value * TRACK_H; }

void drawFader(cairo_t* cr, double cx, double value, bool warm, bool enabled) {
    // outer track
    setc(cr, 0, 0, 0, 0.6);
    rrect(cr, cx - TRACK_OUTER_W / 2, TRACK_TOP - 5, TRACK_OUTER_W, TRACK_H + 10, 4);
    cairo_fill(cr);
    setc(cr, 0, 0, 0, 0.8);
    cairo_set_line_width(cr, 2);
    rrect(cr, cx - TRACK_OUTER_W / 2, TRACK_TOP - 5, TRACK_OUTER_W, TRACK_H + 10, 4);
    cairo_stroke(cr);
    // inner groove
    setc(cr, 31, 31, 31);
    rrect(cr, cx - TRACK_INNER_W / 2, TRACK_TOP, TRACK_INNER_W, TRACK_H, 2);
    cairo_fill(cr);
    // fill from bottom
    double fillTop = TRACK_BOTTOM - value * TRACK_H;
    if (enabled) { if (warm) setc(cr, 255, 0, 0, 0.8); else setc(cr, 255, 255, 255, 0.8); }
    else setc(cr, 255, 255, 255, 0.1);
    rrect(cr, cx - TRACK_INNER_W / 2, fillTop, TRACK_INNER_W, TRACK_BOTTOM - fillTop, 2);
    cairo_fill(cr);
    // ticks (11 rows, mirrored). major at i=0,5,10
    for (int i = 0; i <= 10; ++i) {
        double y = TRACK_BOTTOM - i * (TRACK_H / 10.0);
        bool major = (i % 5 == 0);
        double tw = major ? 20 : 10, th = major ? 2 : 1;
        if (major) setc(cr, 255, 255, 255, 0.4);
        else setc(cr, 255, 255, 255, 0.15);
        cairo_rectangle(cr, cx - 13 - tw, y - th / 2, tw, th); // left
        cairo_fill(cr);
        cairo_rectangle(cr, cx + 13, y - th / 2, tw, th); // right
        cairo_fill(cr);
    }
    // cap (metallic 4-stop gradient) — center tracks fill top, nudged up 6
    double cy = capCenterY(value) - 6;
    double capTop = cy - CAP_H / 2, capX = cx - CAP_W / 2;
    // shadow
    setc(cr, 0, 0, 0, 0.5);
    rrect(cr, capX, capTop + 3, CAP_W, CAP_H, 5);
    cairo_fill(cr);
    cairo_pattern_t* p = cairo_pattern_create_linear(0, capTop, 0, capTop + CAP_H);
    cairo_pattern_add_color_stop_rgb(p, 0.0, 0.35, 0.35, 0.35);
    cairo_pattern_add_color_stop_rgb(p, 0.3, 0.22, 0.22, 0.22);
    cairo_pattern_add_color_stop_rgb(p, 0.7, 0.18, 0.18, 0.18);
    cairo_pattern_add_color_stop_rgb(p, 1.0, 0.25, 0.25, 0.25);
    rrect(cr, capX, capTop, CAP_W, CAP_H, 5);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
    // grip
    double ga = enabled ? 0.9 : 0.3;
    if (warm) setc(cr, 255, 0, 0, ga); else setc(cr, 255, 255, 255, ga);
    rrect(cr, cx - 12, cy - 1.5, 24, 3, 1.5);
    cairo_fill(cr);
}

void drawToggle(cairo_t* cr, double x, double y, double w, double h, bool on) {
    const double r = h / 2;
    if (on) setc(cr, 0, 214, 124);
    else setc(cr, 64, 64, 64);
    rrect(cr, x, y, w, h, r);
    cairo_fill(cr);
    setc(cr, 240, 240, 240);
    cairo_arc(cr, on ? (x + w - r) : (x + r), y + h / 2, r - 3, 0, 2 * M_PI);
    cairo_fill(cr);
}

// PWM-safe status row (shown in the footer when a controllable display exists)
void drawPwmRow(cairo_t* cr, const UiState& st) {
    const double x = HPAD, w = W - 2 * HPAD, y = WARN_Y1 - 14, h = 30;
    int dr, dg, db;
    const char* state;
    if (st.pwmStatus == 2) { dr = 0; dg = 214; db = 124; state = "ON"; }
    else if (st.pwmStatus == -1) { dr = 255; dg = 175; db = 70; state = "RETRY"; }
    else if (st.pwmStatus == 1) { dr = 255; dg = 210; db = 0; state = "..."; }
    else { dr = 120; dg = 120; db = 120; state = "OFF"; }
    setc(cr, 0, 0, 0, 0.45);
    rrect(cr, x, y, w, h, 2);
    cairo_fill(cr);
    setc(cr, dr, dg, db, st.pwmStatus ? 0.45 : 0.18);
    cairo_set_line_width(cr, 1);
    rrect(cr, x, y, w, h, 2);
    cairo_stroke(cr);
    setc(cr, dr, dg, db);
    cairo_arc(cr, x + 13, y + h / 2, 4, 0, 2 * M_PI);
    cairo_fill(cr);
    useFont(cr, g_fonts.mono, 10);
    setc(cr, 255, 255, 255, st.pwmSafe ? 0.95 : 0.5);
    drawText(cr, "PWM-SAFE MODE", x + 26, y + h / 2 + 4, 0.8, 0);
    const double sw = textWidth(cr, state, 0.8);
    setc(cr, dr, dg, db);
    drawText(cr, state, x + w - 12 - sw, y + h / 2 + 4, 0.8, 0);
}

void drawWindowChrome(cairo_t* cr, const UiState& st) {
    if (!st.frameless) return;
    setc(cr, 255, 255, 255, 0.12);
    cairo_set_line_width(cr, 1);
    rrect(cr, 0.5, 0.5, W - 1, H - 1, 11.5);
    cairo_stroke(cr);
    if (st.closeHovered) {
        setc(cr, 255, 70, 70, 0.15);
        rrect(cr, W - 42, 10, 32, 36, 6);
        cairo_fill(cr);
    }
    setc(cr, 255, 255, 255, st.closeHovered ? 0.95 : 0.45);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, W - 31, 23); cairo_line_to(cr, W - 21, 33);
    cairo_move_to(cr, W - 21, 23); cairo_line_to(cr, W - 31, 33);
    cairo_stroke(cr);
}

void renderSettings(cairo_t* cr, const UiState& st) {
    setc(cr, 13, 13, 13);
    cairo_paint(cr);
    // header
    setc(cr, 26, 26, 26);
    cairo_rectangle(cr, 0, 0, W, HEADER_H);
    cairo_fill(cr);
    setc(cr, 255, 255, 255, 0.05);
    cairo_rectangle(cr, 0, 0, W, 1);
    cairo_fill(cr);
    setc(cr, 0, 0, 0, 0.8);
    cairo_rectangle(cr, 0, HEADER_H - 1, W, 1);
    cairo_fill(cr);
    useFont(cr, g_fonts.anybodyXB, 11);
    setc(cr, 255, 255, 255, 0.3);
    drawText(cr, "SETTINGS", W / 2 + 8, 33, 1.5, 1);
    useFont(cr, g_fonts.mono, 11);
    setc(cr, 230, 230, 230);
    drawText(cr, "BACK", HPAD + 16, 32, 0.5, 0);
    cairo_set_line_width(cr, 1.5);
    cairo_move_to(cr, HPAD + 6, 23); cairo_line_to(cr, HPAD, 28); cairo_line_to(cr, HPAD + 6, 33);
    cairo_stroke(cr);
    // toggle rows
    auto row = [&](const char* label, double y, bool on) {
        useFont(cr, g_fonts.mono, 11); // mono: full charset (Anybody TTFs are glyph-subsetted)
        setc(cr, 230, 230, 230);
        drawText(cr, label, HPAD, y + 5, 0.5, 0);
        drawToggle(cr, W - HPAD - 46, y - 9, 46, 24, on);
        setc(cr, 255, 255, 255, 0.06);
        cairo_rectangle(cr, HPAD, y + 22, W - 2 * HPAD, 1);
        cairo_fill(cr);
    };
    row("LAUNCH AT LOGIN", 104, st.launchAtLogin);
    row("ALLOW EXTREME DIM", 156, st.allowExtremeDim);
    if (st.loginError) {
        useFont(cr, g_fonts.mono, 10); setc(cr, 255, 175, 70);
        drawText(cr, "Could not save login setting. Retry.", HPAD, 208, 0, 0);
    }
    // COPY FEEDBACK button
    const double bx = HPAD, bw = W - 2 * HPAD;
    vgrad(cr, bx, 300, bw, 44, 4, 1, 1, 1, 0.12, 1, 1, 1, 0.08);
    useFont(cr, g_fonts.mono, 11);
    if (st.copiedToast > 0) setc(cr, 0, 214, 124);
    else setc(cr, 220, 220, 220);
    drawText(cr, st.copiedToast > 0 ? "COPIED TO CLIPBOARD" : (st.copiedToast < 0 ? "COPY FAILED - TRY AGAIN" : "COPY FEEDBACK FOR SUPPORT"), W / 2, 300 + 27, 0.5, 1);
    // QUIT button
    vgrad(cr, bx, 356, bw, 44, 6, 0.4, 0, 0, 1, 0.25, 0, 0, 1);
    useFont(cr, g_fonts.mono, 13);
    setc(cr, 255, 230, 230);
    drawText(cr, "QUIT", W / 2, 356 + 28, 2.0, 1);
    // Quiet text links at the bottom, with separate click targets.
    useFont(cr, g_fonts.mono, 14);
    setc(cr, 255, 255, 255, st.linkHovered == 1 ? 1.0 : 0.9);
    drawText(cr, "Made by Rusty", W / 2, 490, 0, 1);
    useFont(cr, g_fonts.mono, 10);
    const int websiteTone = st.linkHovered == 2 ? 190 : 102;
    setc(cr, websiteTone, websiteTone, websiteTone);
    drawText(cr, "tapzap.app", W / 2, 516, 0, 1);
}

void render(cairo_t* cr, const UiState& st) {
    ensureFonts();
    if (st.view == 1) { renderSettings(cr, st); drawWindowChrome(cr, st); return; }
    // body
    setc(cr, 13, 13, 13);
    cairo_paint(cr);

    // ---- header panel ----
    setc(cr, 26, 26, 26);
    cairo_rectangle(cr, 0, 0, W, HEADER_H);
    cairo_fill(cr);
    setc(cr, 255, 255, 255, 0.05);
    cairo_rectangle(cr, 0, 0, W, 1);
    cairo_fill(cr);
    setc(cr, 0, 0, 0, 0.8);
    cairo_rectangle(cr, 0, HEADER_H - 1, W, 1);
    cairo_fill(cr);
    useFont(cr, g_fonts.anybodyXB, 11);
    setc(cr, 255, 255, 255, 0.3);
    drawText(cr, "TAP ZAP", HPAD, 33, 1.5, 0);
    drawGear(cr, st.frameless ? W - 70 : W - HPAD - 6, 28, 8, 255, 255, 255, 0.45);

    // ---- fader labels ----
    useFont(cr, g_fonts.anybodyXB, 14);
    setc(cr, 255, 0, 0);
    drawText(cr, "WARMTH", TEMP_CX, LABEL_BASE, 0, 1);
    setc(cr, 255, 255, 255);
    drawText(cr, "BRIGHTNESS", BRIGHT_CX, LABEL_BASE, 0, 1);

    // ---- center divider ----
    setc(cr, 0, 0, 0);
    cairo_rectangle(cr, W / 2 - 1, DIV_TOP, 2, DIV_BOTTOM - DIV_TOP);
    cairo_fill(cr);
    setc(cr, 255, 255, 255, 0.05);
    cairo_rectangle(cr, W / 2 + 1, DIV_TOP, 1, DIV_BOTTOM - DIV_TOP);
    cairo_fill(cr);

    // ---- faders ----
    drawFader(cr, TEMP_CX, st.intensity, true, st.enabled);
    drawFader(cr, BRIGHT_CX, st.brightness, false, st.enabled);

    // ---- readout pills ----
    int kelvin = (int)std::lround(kelvinForIntensity(st.intensity));
    char kb[32];
    if (kelvin >= 1000) std::snprintf(kb, sizeof kb, "%d,%03dK", kelvin / 1000, kelvin % 1000);
    else std::snprintf(kb, sizeof kb, "%dK", kelvin);
    char bb[16];
    std::snprintf(bb, sizeof bb, "%d%%", (int)(st.brightness * 100));
    // warm pill
    setc(cr, 0, 0, 0);
    rrect(cr, TEMP_CX - RW_WARM / 2, READOUT_CY - READOUT_H / 2, RW_WARM, READOUT_H, 4);
    cairo_fill(cr);
    setc(cr, 255, 255, 255, 0.1);
    cairo_set_line_width(cr, 1);
    rrect(cr, TEMP_CX - RW_WARM / 2, READOUT_CY - READOUT_H / 2, RW_WARM, READOUT_H, 4);
    cairo_stroke(cr);
    useFont(cr, g_fonts.mono, 12);
    setc(cr, 255, 0, 0);
    drawText(cr, kb, TEMP_CX, READOUT_CY + 4, 0, 1);
    // bright pill
    setc(cr, 0, 0, 0);
    rrect(cr, BRIGHT_CX - RW_BRIGHT / 2, READOUT_CY - READOUT_H / 2, RW_BRIGHT, READOUT_H, 4);
    cairo_fill(cr);
    setc(cr, 255, 255, 255, 0.1);
    rrect(cr, BRIGHT_CX - RW_BRIGHT / 2, READOUT_CY - READOUT_H / 2, RW_BRIGHT, READOUT_H, 4);
    cairo_stroke(cr);
    setc(cr, 255, 255, 255);
    drawText(cr, bb, BRIGHT_CX, READOUT_CY + 4, 0, 1);

    // ---- footer panel ----
    setc(cr, 26, 26, 26);
    cairo_rectangle(cr, 0, FOOTER_TOP, W, H - FOOTER_TOP);
    cairo_fill(cr);
    setc(cr, 0, 0, 0, 0.5);
    cairo_rectangle(cr, 0, FOOTER_TOP, W, 1);
    cairo_fill(cr);

    // PWM-safe row OR flicker warning (smart hybrid, like macOS PwmSafeStatusRow)
    if (st.pwmAvailable) {
        drawPwmRow(cr, st);
    } else if (st.flicker != 0) {
        setc(cr, 0, 0, 0, 0.3);
        rrect(cr, HPAD, WARN_Y1 - 15, W - 2 * HPAD, 42, 4);
        cairo_fill(cr);
        useFont(cr, g_fonts.mono, 10);
        setc(cr, 255, 204, 0);
        char label[64];
        if (st.hardwarePercent >= 0)
            std::snprintf(label, sizeof label, "DISPLAY BRIGHTNESS  %d%%", st.hardwarePercent);
        else std::snprintf(label, sizeof label, "CHECK DISPLAY BRIGHTNESS");
        drawText(cr, label, W / 2, WARN_Y1, 0.3, 1);
        setc(cr, 230, 230, 230, 0.8);
        drawText(cr, "RAISE TO 100%, THEN DIM HERE", W / 2, WARN_Y2 + 2, 0, 1);
    }

    // presets
    double pw = (W - 2 * HPAD - 2 * 12) / 3.0;
    for (int i = 0; i < 3; ++i) {
        double px = HPAD + i * (pw + 12);
        bool active = (std::fabs(st.intensity - intensityForKelvin(PRESET_K[i])) < 0.02);
        if (active) vgrad(cr, px, PRESET_TOP, pw, PRESET_H, 4, 1, 1, 1, 0.25, 1, 1, 1, 0.20);
        else vgrad(cr, px, PRESET_TOP, pw, PRESET_H, 4, 1, 1, 1, 0.12, 1, 1, 1, 0.08);
        cairo_pattern_t* bp = cairo_pattern_create_linear(0, PRESET_TOP, 0, PRESET_TOP + PRESET_H);
        cairo_pattern_add_color_stop_rgba(bp, 0, 1, 1, 1, 0.1);
        cairo_pattern_add_color_stop_rgba(bp, 1, 0, 0, 0, 0.5);
        rrect(cr, px, PRESET_TOP, pw, PRESET_H, 4);
        cairo_set_source(cr, bp);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        cairo_pattern_destroy(bp);
        useFont(cr, g_fonts.anybodyB, 10);
        if (active) setc(cr, 255, 255, 255);
        else setc(cr, 142, 142, 147);
        drawText(cr, PRESET_NAME[i], px + pw / 2, PRESET_TOP + PRESET_H / 2 + 3.5, 0, 1);
    }

    // ZAP
    double zw = W - 2 * HPAD;
    if (st.enabled) vgrad(cr, HPAD, ZAP_TOP, zw, ZAP_H, 6, 0.8, 0, 0, 1, 0.6, 0, 0, 1);
    else vgrad(cr, HPAD, ZAP_TOP, zw, ZAP_H, 6, 1, 1, 1, 0.15, 1, 1, 1, 0.10);
    cairo_pattern_t* zb = cairo_pattern_create_linear(0, ZAP_TOP, 0, ZAP_TOP + ZAP_H);
    cairo_pattern_add_color_stop_rgba(zb, 0, 1, 1, 1, st.enabled ? 0.3 : 0.1);
    cairo_pattern_add_color_stop_rgba(zb, 1, 0, 0, 0, 0.5);
    rrect(cr, HPAD, ZAP_TOP, zw, ZAP_H, 6);
    cairo_set_source(cr, zb);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    cairo_pattern_destroy(zb);
    int fg = st.enabled ? 255 : 142;
    int fgg = st.enabled ? 255 : 142, fgb = st.enabled ? 255 : 147;
    useFont(cr, g_fonts.anybodyXB, 13);
    double tw = textWidth(cr, "ZAP", 1.0);
    double groupW = 18 + 8 + tw;
    double gx = W / 2 - groupW / 2;
    drawBolt(cr, gx + 9, ZAP_TOP + ZAP_H / 2, 16, fg, fgg, fgb);
    setc(cr, fg, fgg, fgb);
    drawText(cr, "ZAP", gx + 18 + 8, ZAP_TOP + ZAP_H / 2 + 4.5, 1.0, 0);
    drawWindowChrome(cr, st);
}

// ---- offscreen render to PPM ----
int shot(const char* path, double intensity, double brightness, bool enabled, int scale) {
    const int pxW = (int)W * scale, pxH = (int)H * scale;
    cairo_surface_t* s = cairo_image_surface_create(CAIRO_FORMAT_RGB24, pxW, pxH);
    cairo_t* cr = cairo_create(s);
    cairo_scale(cr, scale, scale);
    UiState st;
    st.intensity = intensity;
    st.brightness = brightness;
    st.enabled = enabled;
    render(cr, st);
    cairo_surface_flush(s);
    unsigned char* data = cairo_image_surface_get_data(s);
    const int stride = cairo_image_surface_get_stride(s);
    Image img;
    img.w = pxW;
    img.h = pxH;
    img.px.resize((size_t)pxW * pxH * 3);
    for (int y = 0; y < pxH; ++y) {
        const uint32_t* row = (const uint32_t*)(data + (size_t)y * stride);
        for (int x = 0; x < pxW; ++x) {
            const uint32_t v = row[x];
            const size_t o = ((size_t)y * pxW + x) * 3;
            img.px[o] = (v >> 16) & 0xff;
            img.px[o + 1] = (v >> 8) & 0xff;
            img.px[o + 2] = v & 0xff;
        }
    }
    const bool ok = writePPM(path, img);
    cairo_destroy(cr);
    cairo_surface_destroy(s);
    std::printf("UI shot -> %s (%dx%d, i=%.4f b=%.2f %s)\n", path, pxW, pxH, intensity, brightness, enabled ? "ON" : "OFF");
    return ok ? 0 : 1;
}

// ---- overlay film (TAPZAP_OVERLAY=1) ----
// A fullscreen, click-through, alpha-composited window the app raises when the filter is on.
// Unlike gamma (applied at scanout, invisible to screen capture/VNC), this lives in the
// framebuffer, so it shows over remote displays. It's a tint film, not true 0K-red gamma.
struct Overlay {
    Display* dpy = nullptr;
    Window win = 0;
    cairo_surface_t* surf = nullptr;
    cairo_t* cr = nullptr;
    Atom opacityAtom = 0;
    int w = 0, h = 0;
    bool ok = false;
    bool mapped = false;
    Window appWin = 0;                    // the Tap Zap UI window — punched out of the film
    int holeX = -1, holeY = -1, holeW = -1, holeH = -1;  // last-applied hole (skip redundant reshapes)

    void init(Display* d, Window app = 0) {
        dpy = d;
        appWin = app;
        const int scr = DefaultScreen(d);
        w = DisplayWidth(d, scr);
        h = DisplayHeight(d, scr);
        Visual* vis = DefaultVisual(d, scr);       // default (e.g. 24-bit) visual — no ARGB needed
        const int depth = DefaultDepth(d, scr);
        XSetWindowAttributes attr;
        attr.override_redirect = True;
        attr.background_pixel = BlackPixel(d, scr);
        attr.border_pixel = 0;
        win = XCreateWindow(d, RootWindow(d, scr), 0, 0, w, h, 0, depth, InputOutput, vis,
                            CWOverrideRedirect | CWBackPixel | CWBorderPixel, &attr);
        // Input passthrough: give the window an EMPTY input region so every click falls through
        // to the desktop/app beneath. Prefer XFixes (honored by ~all modern servers incl. VNC);
        // fall back to the older XShape input-shape for servers without XFixes.
        setInputPassthrough(d);
        opacityAtom = XInternAtom(d, "_NET_WM_WINDOW_OPACITY", False);
        surf = cairo_xlib_surface_create(d, win, vis, w, h);
        cr = cairo_create(surf);
        ok = true;
    }
    void setInputPassthrough(Display* d) {
#ifdef HAVE_XFIXES
        int evb, erb;
        if (XFixesQueryExtension(d, &evb, &erb)) {
            XserverRegion empty = XFixesCreateRegion(d, nullptr, 0);
            XFixesSetWindowShapeRegion(d, win, ShapeInput, 0, 0, empty);
            XFixesDestroyRegion(d, empty);
            return;
        }
#endif
        XShapeCombineRectangles(d, win, ShapeInput, 0, 0, nullptr, 0, ShapeSet, Unsorted);
    }
    // Cut a rectangular hole in the film exactly over the app window (+ its WM title bar), so the
    // UI renders crisp and stays fully clickable regardless of whether the server honours input-
    // shape passthrough. The desktop everywhere else keeps the tint. Tracks the window if it moves.
    void punchHole() {
        if (!appWin) return;
        Window root, child; int ax = 0, ay = 0; unsigned aw = 0, ah = 0, bw = 0, ad = 0;
        if (!XGetGeometry(dpy, appWin, &root, &ax, &ay, &aw, &ah, &bw, &ad)) return;
        XTranslateCoordinates(dpy, appWin, root, 0, 0, &ax, &ay, &child); // client-area origin on root
        const int mx = 10, mtop = 40, mbot = 12; // pad for the WM frame/title bar + shadow
        int hx = ax - mx, hy = ay - mtop, hw = (int)aw + 2 * mx, hh = (int)ah + mtop + mbot;
        if (hx == holeX && hy == holeY && hw == holeW && hh == holeH) return; // unchanged
        holeX = hx; holeY = hy; holeW = hw; holeH = hh;
        XRectangle full = {0, 0, (unsigned short)w, (unsigned short)h};
        XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, &full, 1, ShapeSet, Unsorted);
        XRectangle hole = {(short)hx, (short)hy, (unsigned short)hw, (unsigned short)hh};
        XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, &hole, 1, ShapeSubtract, Unsorted);
    }
    void update(double intensity, double brightness, bool enabled) {
        if (!ok) return;
        if (!enabled) {
            if (mapped) { XUnmapWindow(dpy, win); XFlush(dpy); mapped = false; }
            return;
        }
        // Opacity is a compositor property (translucency) — set once up front.
        const double a = clampd(0.18 + intensity * 0.60, 0.0, 0.85); // film strength
        const unsigned long op = (unsigned long)(a * 4294967295.0);
        XChangeProperty(dpy, win, opacityAtom, XA_CARDINAL, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(const_cast<unsigned long*>(&op)), 1);
        XMapRaised(dpy, win);
        punchHole();      // keep the app window clear of the film (after raise, so it's not re-covered)
        // Paint the warmth fill AFTER map+reshape: a bounding-shape change makes the server blank an
        // override-redirect window (ShapeNotify→expose with no handler), which would leave the black
        // background showing. Painting last guarantees the red fill is the final content.
        const RGB f = normalizedRGB(intensity);
        const double bf = clampd(0.35 + 0.65 * brightness, 0.0, 1.0);
        cairo_set_source_rgb(cr, f.r * bf, f.g * bf, f.b * bf);
        cairo_paint(cr);
        cairo_surface_flush(surf);
        XFlush(dpy);
        mapped = true;
    }
    void destroy() {
        if (cr) cairo_destroy(cr);
        if (surf) cairo_surface_destroy(surf);
        if (win) XDestroyWindow(dpy, win);
    }
};

// ---- live window ----
double desktopUiScale(Display* dpy, int screen) {
    // Read the live property, not XResourceManagerString's connection-time cache.
    const Atom resource = XInternAtom(dpy, "RESOURCE_MANAGER", False);
    Atom type = None;
    int format = 0;
    unsigned long count = 0, remaining = 0;
    unsigned char* bytes = nullptr;
    double scale = 1.0;
    if (XGetWindowProperty(dpy, RootWindow(dpy, screen), resource, 0, 65536,
                           False, XA_STRING, &type, &format, &count, &remaining, &bytes) == Success &&
        type == XA_STRING && format == 8 && bytes) {
        XrmInitialize();
        XrmDatabase db = XrmGetStringDatabase(reinterpret_cast<char*>(bytes));
        if (db) {
            char* valueType = nullptr;
            XrmValue value{};
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &valueType, &value) && value.addr) {
                char* end = nullptr;
                const double dpi = std::strtod(value.addr, &end);
                if (end != value.addr && std::isfinite(dpi) && dpi >= 48 && dpi <= 768)
                    scale = dpi / 96.0;
            }
            XrmDestroyDatabase(db);
        }
    }
    if (bytes) XFree(bytes);
    return scale;
}

int liveWindow(double i0, double b0, bool e0) {
    const bool startHidden = std::getenv("TAPZAP_START_HIDDEN") != nullptr;
    if (ipcSend(startHidden ? "status" : "show")) return 0;
    InstanceLock instance;
    if (!instance.acquire()) {
        std::fprintf(stderr, "TAP ZAP is already starting, or the private runtime directory is unavailable.\n");
        return 2;
    }
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) { std::fprintf(stderr, "cannot open X display (is $DISPLAY set?)\n"); return 2; }
    fcntl(ConnectionNumber(dpy), F_SETFD, FD_CLOEXEC);
    const int scr = DefaultScreen(dpy);
    // Layout units are logical pixels; the optional multiplier is applied on top
    // of the desktop's DPI, matching the Windows app's logical-unit scaling.
    double userScale = 1.0;
    if (const char* s = std::getenv("TAPZAP_SCALE")) {
        char* end = nullptr;
        const double v = std::strtod(s, &end);
        if (end != s && *end == '\0' && std::isfinite(v) && v >= 0.5 && v <= 3.0) userScale = v;
    }
    double compositorScale = 1.0;
    auto desiredScale = [&]() {
        const double desktop = std::max(desktopUiScale(dpy, scr), compositorScale);
        // Keep a compact tool window when high desktop scaling is retained at
        // a lower resolution; merely fitting the full screen makes it enormous.
        const double fit = std::min((DisplayWidth(dpy, scr) - 32 * desktop) / W,
                                    DisplayHeight(dpy, scr) * 0.60 / H);
        return std::max(0.5, std::min(std::max(1.0, desktop * userScale), fit));
    };
    double scale = desiredScale();
    int pxW = static_cast<int>(std::lround(W * scale));
    int pxH = static_cast<int>(std::lround(H * scale));
    int popoverX = 0, popoverY = 0;
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, pxW, pxH, 0, 0, BlackPixel(dpy, scr));
    XStoreName(dpy, win, "TAP ZAP — Omarchy trial");
    XClassHint wmClass{const_cast<char*>("tapzap-omarchy"), const_cast<char*>("TapZapOmarchy")};
    XSetClassHint(dpy, win, &wmClass);
    Atom dialogType = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    XChangeProperty(dpy, win, XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False), XA_ATOM, 32,
                    PropModeReplace, reinterpret_cast<unsigned char*>(&dialogType), 1);
    auto updateSizeHints = [&]() {
        XSizeHints* sh = XAllocSizeHints();
        sh->flags = PMinSize | PMaxSize;
        sh->min_width = sh->max_width = pxW;
        sh->min_height = sh->max_height = pxH;
        XSetWMNormalHints(dpy, win, sh);
        XFree(sh);
    };
    updateSizeHints();

    // ---- popover chrome: frameless, tray-anchored, above, off the taskbar — like the macOS
    // menu-bar popover / Windows tray flyout (not a floating titled window). Env TAPZAP_WINDOWED=1
    // keeps the old decorated window for WMs where a frameless popover misbehaves.
    const bool frameless = std::getenv("TAPZAP_FRAMELESS") != nullptr;
    const bool popover = !frameless && std::getenv("TAPZAP_WINDOWED") == nullptr;
    const bool dismissOnFocusLoss = popover || frameless;
    if (popover || frameless) {
        // remove title bar / borders via Motif hints (MWM_HINTS_DECORATIONS = 1<<1, decorations=0)
        struct { unsigned long flags, functions, decorations; long input_mode; unsigned long status; }
            mh = {2UL, 0UL, 0UL, 0L, 0UL};
        Atom motif = XInternAtom(dpy, "_MOTIF_WM_HINTS", False);
        XChangeProperty(dpy, win, motif, motif, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&mh), 5);
    }
    auto updateShape = [&]() {
        if (!frameless) return;
        int eventBase = 0, errorBase = 0;
        if (XShapeQueryExtension(dpy, &eventBase, &errorBase)) {
            std::vector<XRectangle> rows;
            const double radius = 12 * scale;
            for (int y = 0; y < pxH; ++y) {
                const double edge = std::min(y + 0.5, pxH - y - 0.5);
                const double dy = std::max(0.0, radius - edge);
                const int inset = static_cast<int>(std::ceil(radius - std::sqrt(radius * radius - dy * dy)));
                rows.push_back({static_cast<short>(inset), static_cast<short>(y),
                    static_cast<unsigned short>(pxW - 2 * inset), 1});
            }
            XShapeCombineRectangles(dpy, win, ShapeBounding, 0, 0, rows.data(), rows.size(), ShapeSet, YSorted);
        }
    };
    updateShape();
    if (popover) {
        // window-type = utility, and skip taskbar/pager + keep above
        Atom wtype = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom wtUtil = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_UTILITY", False);
        XChangeProperty(dpy, win, wtype, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(&wtUtil), 1);
        Atom wstate = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom states[] = {XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False),
                         XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False),
                         XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False)};
        XChangeProperty(dpy, win, wstate, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<unsigned char*>(states), 3);
        // anchor top-right, just under a typical panel — where the tray icon lives
        const int margin = 12;
        popoverX = DisplayWidth(dpy, scr) - pxW - margin;
        popoverY = 40;
        XMoveWindow(dpy, win, popoverX, popoverY);
    }

    long evmask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | LeaveWindowMask;
    if (dismissOnFocusLoss) evmask |= FocusChangeMask;
    XSelectInput(dpy, win, evmask);
    Atom wmDelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, win, &wmDelete, 1);
    if (!startHidden) XMapWindow(dpy, win);
    if (popover && !startHidden) { XMoveWindow(dpy, win, popoverX, popoverY); XRaiseWindow(dpy, win); }
    Visual* vis = DefaultVisual(dpy, scr);
    cairo_surface_t* xs = cairo_xlib_surface_create(dpy, win, vis, pxW, pxH);
    cairo_surface_t* buf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, pxW, pxH);
    cairo_t* bcr = cairo_create(buf);
    cairo_scale(bcr, scale, scale);
    cairo_t* xcr = cairo_create(xs);

    auto backend = makeDefaultBackend();
    std::string err;
    const bool haveBackend = backend->init(err);
    compositorScale = backend->displayScale();
    if (!haveBackend) std::fprintf(stderr, "gamma backend unavailable: %s (UI runs, no live tint)\n", err.c_str());
    else std::fprintf(stderr, "gamma backend: %s (%d output[s])\n", backend->name().c_str(), backend->outputCount());
    // The Plasma proof must never open an apparently working, unfiltered UI.
    if (!haveBackend) { XDestroyWindow(dpy, win); XCloseDisplay(dpy); return 2; }

    Config cfg = loadConfig();
    UiState st;
    st.allowExtremeDim = cfg.allowExtremeDim;
    st.launchAtLogin = isLaunchAtLogin();
    if (st.launchAtLogin) st.loginError = !setLaunchAtLogin(true);
    st.pwmSafe = cfg.pwmSafeMode;
    auto floorMin = [&]() { return st.allowExtremeDim ? 0.0 : BRIGHT_FLOOR; };
    if (e0) { // explicit forced-on launch (e.g. `tapzap gui 1 0.7 1`)
        st.intensity = clampd(i0, 0.0, 1.0);
        st.brightness = clampd(b0, floorMin(), 1.0);
        st.enabled = true;
    } else { // Normal launch: resume the saved filter state and slider positions.
        st.intensity = clampd(cfg.intensity, 0.0, 1.0);
        st.brightness = clampd(cfg.brightness, floorMin(), 1.0);
        st.enabled = cfg.enabled;
    }
    auto saveCfg = [&]() {
        Config c;
        c.enabled = st.enabled;
        c.intensity = st.intensity;
        c.brightness = st.brightness;
        c.allowExtremeDim = st.allowExtremeDim;
        c.pwmSafeMode = st.pwmSafe;
        c.launchAtLogin = st.launchAtLogin;
        saveConfig(c);
    };

    PwmSafe pwm;
    st.pwmAvailable = pwm.detect();
    if (st.pwmAvailable && st.pwmSafe) { st.pwmStatus = pwm.enable() ? 2 : -1; }
    else { st.pwmSafe = false; st.pwmStatus = 0; }

    const bool useOverlay = std::getenv("TAPZAP_OVERLAY") != nullptr;
    Overlay overlay;
    if (useOverlay) overlay.init(dpy, win);
    std::string lastFilterError;
    auto applyFilter = [&]() {
        lastFilterError.clear();
        if (useOverlay) overlay.update(st.intensity, st.brightness, st.enabled);
        if (!haveBackend) return;
        std::string e;
        const bool applied = st.enabled ? backend->setFilter(st.intensity, st.brightness, e) : backend->restore(e);
        if (!applied) {
            lastFilterError = e;
            std::fprintf(stderr, "Filter error: %s\n", e.c_str());
            st.enabled = false;
        }
    };
    auto refreshFlicker = [&]() {
        // smart hybrid (macOS FlickerWarningLevel): judge by the built-in panel's real hardware
        // brightness when readable, otherwise by the app's brightness slider.
        if (st.pwmAvailable) return; // PWM-safe row is shown instead of the warning
        const HwBrightness hb = readHardwareBrightness();
        st.hardwarePercent = hb.available ? static_cast<int>(std::lround(clampd(hb.value, 0.0, 1.0) * 100)) : -1;
        if (hb.available) st.flicker = hb.value < 0.80 ? 2 : (hb.value < 0.99 ? 1 : 0);
        else st.flicker = st.brightness < 0.99 ? 1 : 0;
    };
    auto redraw = [&]() {
        refreshFlicker();
        render(bcr, st);
        cairo_set_source_surface(xcr, buf, 0, 0);
        cairo_paint(xcr);
        cairo_surface_flush(xs);
        XFlush(dpy);
        if (std::getenv("TAPZAP_DEBUG_RENDER"))
            std::fprintf(stderr, "[redraw] view=%d bcr=%s xcr=%s buf=%s xs=%s\n", st.view,
                         cairo_status_to_string(cairo_status(bcr)),
                         cairo_status_to_string(cairo_status(xcr)),
                         cairo_status_to_string(cairo_surface_status(buf)),
                         cairo_status_to_string(cairo_surface_status(xs)));
    };

    auto updateDesktopScale = [&]() {
        const double next = desiredScale();
        if (std::abs(next - scale) < 0.0001) return;
        scale = next;
        pxW = static_cast<int>(std::lround(W * scale));
        pxH = static_cast<int>(std::lround(H * scale));
        updateSizeHints();
        XResizeWindow(dpy, win, pxW, pxH);
        updateShape();
        cairo_xlib_surface_set_size(xs, pxW, pxH);
        cairo_destroy(bcr);
        cairo_surface_destroy(buf);
        buf = cairo_image_surface_create(CAIRO_FORMAT_RGB24, pxW, pxH);
        bcr = cairo_create(buf);
        cairo_scale(bcr, scale, scale);
        // Keep the window reachable when the screen becomes smaller.
        int x = 0, y = 0;
        Window child = None;
        XTranslateCoordinates(dpy, win, RootWindow(dpy, scr), 0, 0, &x, &y, &child);
        popoverX = std::max(0, std::min(x, DisplayWidth(dpy, scr) - pxW));
        popoverY = std::max(0, std::min(y, DisplayHeight(dpy, scr) - pxH));
        XMoveWindow(dpy, win, popoverX, popoverY);
        std::fprintf(stderr, "UI scale updated: %.3fx, %dx%d pixels\n", scale, pxW, pxH);
        redraw();
    };
    const Atom resourceManager = XInternAtom(dpy, "RESOURCE_MANAGER", False);
    XWindowAttributes rootAttributes{};
    XGetWindowAttributes(dpy, RootWindow(dpy, scr), &rootAttributes);
    XSelectInput(dpy, RootWindow(dpy, scr), rootAttributes.your_event_mask | PropertyChangeMask);
    std::fprintf(stderr, "UI scale: desktop %.3fx, preference %.3fx, window %dx%d pixels\n",
                 desktopUiScale(dpy, scr), userScale, pxW, pxH);

    int dragging = -1;
    bool running = true;
    bool focusSeen = false;   // popover dismiss-on-focus-loss only arms after the first FocusIn
    auto hidePanel = [&]() {
        if (dragging >= 0) saveCfg();
        dragging = -1;
        focusSeen = false;
        st.closeHovered = false;
        st.linkHovered = 0;
        XUnmapWindow(dpy, win);
        XFlush(dpy);
    };
    const Cursor handCursor = XCreateFontCursor(dpy, XC_hand2);
    const Cursor arrowCursor = XCreateFontCursor(dpy, XC_left_ptr);

    // Display-change re-assert: re-apply gamma after a reconfigure/wake (RRScreenChangeNotify).
    int rrEventBase = -1;
#ifdef HAVE_XRANDR
    {
        int rrErrBase = 0, maj = 0, min = 0;
        if (XRRQueryExtension(dpy, &rrEventBase, &rrErrBase) && XRRQueryVersion(dpy, &maj, &min))
            XRRSelectInput(dpy, RootWindow(dpy, scr), RRScreenChangeNotifyMask);
        else
            rrEventBase = -1;
    }
#endif

    // Signal self-pipe: restore gamma/backlight on SIGINT/SIGTERM. X calls aren't async-signal-safe,
    // so the handler only nudges the select() loop, which then exits and runs the normal cleanup.
    static int s_sigwr = -1;
    int sigpipe[2] = {-1, -1};
    if (pipe(sigpipe) == 0) {
        s_sigwr = sigpipe[1];
        struct sigaction sa;
        std::memset(&sa, 0, sizeof sa);
        sa.sa_handler = [](int) { if (s_sigwr >= 0) { char c = 1; ssize_t r = ::write(s_sigwr, &c, 1); (void)r; } };
        ::sigaction(SIGINT, &sa, nullptr);
        ::sigaction(SIGTERM, &sa, nullptr);
    }

    const int ipcfd = ipcListen();
    if (ipcfd < 0) { std::fprintf(stderr,"Cannot create private control socket\n"); return 2; }
    const Atom A_CLIPBOARD = XInternAtom(dpy, "CLIPBOARD", False);
    const Atom A_UTF8 = XInternAtom(dpy, "UTF8_STRING", False);
    const Atom A_TARGETS = XInternAtom(dpy, "TARGETS", False);
    std::string clipText; // support-bundle text we own on the clipboard

    // Launch the tray helper (menu-bar presence) if it's installed next to our binary.
    pid_t trayPid = -1;
    {
        const std::string ep = exePath();
        const auto sl = ep.rfind('/');
        const std::string traybin = (sl == std::string::npos ? std::string() : ep.substr(0, sl + 1)) + "tapzap-tray";
        if (::access(traybin.c_str(), X_OK) == 0) {
            const pid_t p = fork();
            if (p == 0) {
                const pid_t parent = getppid();
                prctl(PR_SET_PDEATHSIG, SIGTERM);
                if (getppid() != parent) _exit(1);
                execl(traybin.c_str(), "tapzap-tray", static_cast<char*>(nullptr)); _exit(127);
            }
            if (p > 0) trayPid = p;
        }
    }
    // The application launcher always provides a way to reopen a hidden trial.
    const bool trayRunning = true;
    std::vector<pid_t> linkOpeners;
    auto openLink = [&](int link) {
        const char* url = link == 1 ? "https://x.com/ZE_RUSTY" : "https://tapzap.app";
        const pid_t pid = fork();
        if (pid == 0) {
            // The browser must not inherit the GUI's signal pipe or lifetime.
            if (sigpipe[0] >= 0) close(sigpipe[0]);
            if (sigpipe[1] >= 0) close(sigpipe[1]);
            setsid();
            execlp("xdg-open", "xdg-open", url, static_cast<char*>(nullptr));
            _exit(127);
        }
        if (pid > 0) linkOpeners.push_back(pid);
    };

    auto handleX = [&](XEvent& ev) {
        if (ev.type == PropertyNotify && ev.xproperty.window == RootWindow(dpy, scr) &&
            ev.xproperty.atom == resourceManager) {
            updateDesktopScale();
            return;
        }
        // Coalesce bursts so a high-latency/remote X link doesn't queue one full-frame redraw per event.
        if (ev.type == MotionNotify) {
            XEvent nx;
            // Never pull motion from beyond a pending release/focus event.
            while (XPending(dpy)) {
                XPeekEvent(dpy, &nx);
                if (nx.type != MotionNotify || nx.xmotion.window != win) break;
                XNextEvent(dpy, &ev);
            }
            if (dragging >= 0 && !(ev.xmotion.state & Button1Mask)) {
                saveCfg();
                dragging = -1;
            }
        } else if (ev.type == Expose) {
            XEvent nx;
            while (XCheckTypedWindowEvent(dpy, win, Expose, &nx)) { /* collapse to a single repaint */ }
        }
#ifdef HAVE_XRANDR
        if (rrEventBase >= 0 && ev.type == rrEventBase + RRScreenChangeNotify) {
            XRRUpdateConfiguration(&ev);
            updateDesktopScale();
            std::string e;
            backend->refresh(e); // re-enumerate outputs + recapture baselines
            applyFilter();        // re-assert the filter on the new configuration
            redraw();
            return;
        }
#endif
        if (ev.type == Expose) {
            redraw();
        } else if (ev.type == ClientMessage) {
            if ((Atom)ev.xclient.data.l[0] == wmDelete) {
                if (trayRunning) hidePanel();
                else running = false;
            }
        } else if (ev.type == FocusIn && dismissOnFocusLoss) {
            focusSeen = true;
        } else if (ev.type == FocusOut && dismissOnFocusLoss && trayRunning) {
            // A real focus transfer cancels a drag too. Temporary pointer grabs
            // are distinguished by their mode, not possibly stale drag state.
            if (focusSeen && ev.xfocus.mode == NotifyNormal && ev.xfocus.detail != NotifyInferior)
                hidePanel();
        } else if (ev.type == KeyPress) {
            KeySym ks = XLookupKeysym(&ev.xkey, 0);
            if (ks == XK_Escape && st.view == 1) { st.view = 0; redraw(); }
            else if (ks == XK_Escape && trayRunning) hidePanel();
            else if (ks == XK_q) running = false;
            else if (ks == XK_Escape) running = false;
            else if (ks == XK_space) { st.enabled = !st.enabled; applyFilter(); saveCfg(); redraw(); }
        } else if (ev.type == ButtonPress && ev.xbutton.button == 1) {
            const double lx = ev.xbutton.x / (double)scale, ly = ev.xbutton.y / (double)scale;
            std::fprintf(stderr, "[click] raw=%d,%d scaled=%.0f,%.0f view=%d W=%d HEADER_H=%d\n",
                         ev.xbutton.x, ev.xbutton.y, lx, ly, st.view, (int)W, (int)HEADER_H);
            st.copiedToast = 0; // any click clears the toast
            if (frameless && lx >= W - 46 && ly < HEADER_H) {
                hidePanel();
            } else if (st.view == 1 && ly < HEADER_H && lx < 104) {
                st.view = 0;
            } else if (st.view == 0 && ly < HEADER_H && (frameless ? (lx >= W - 90 && lx < W - 50) : lx > W - HPAD - 20)) {
                st.view = st.view ? 0 : 1;
            } else if (frameless && ly < HEADER_H) {
                // Ask the window manager to move the window using its normal drag behavior.
                XEvent move{};
                move.xclient.type = ClientMessage;
                move.xclient.window = win;
                move.xclient.message_type = XInternAtom(dpy, "_NET_WM_MOVERESIZE", False);
                move.xclient.format = 32;
                move.xclient.data.l[0] = ev.xbutton.x_root;
                move.xclient.data.l[1] = ev.xbutton.y_root;
                move.xclient.data.l[2] = 8; // move
                move.xclient.data.l[3] = Button1;
                move.xclient.data.l[4] = 1; // application source
                XUngrabPointer(dpy, CurrentTime);
                XSendEvent(dpy, RootWindow(dpy, scr), False,
                           SubstructureRedirectMask | SubstructureNotifyMask, &move);
                XFlush(dpy);
            } else if (st.view == 1) {                          // ---- settings view ----
                const int link = settingsLinkAt(lx, ly);
                if (link) {
                    openLink(link);
                } else if (ly >= 104 - 20 && ly <= 104 + 20) {  // LAUNCH AT LOGIN
                    st.loginError = !setLaunchAtLogin(!st.launchAtLogin);
                    st.launchAtLogin = isLaunchAtLogin();
                    saveCfg();
                } else if (ly >= 156 - 20 && ly <= 156 + 20) {  // ALLOW EXTREME DIM
                    st.allowExtremeDim = !st.allowExtremeDim;
                    if (st.brightness < floorMin()) { st.brightness = floorMin(); applyFilter(); }
                    saveCfg();
                } else if (ly >= 300 && ly <= 344) {            // COPY FEEDBACK -> clipboard
                    clipText = buildSupportBundle(st.intensity, st.brightness, st.enabled,
                                                  st.allowExtremeDim, st.pwmSafe, st.launchAtLogin);
                    XSetSelectionOwner(dpy, A_CLIPBOARD, win, CurrentTime); // Ctrl+V / Ctrl+Shift+V
                    XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);  // middle-click / vim "*p
                    st.copiedToast = XGetSelectionOwner(dpy, A_CLIPBOARD) == win ? 1 : -1;
                    std::fprintf(stderr, "[copy] %zu bytes -> CLIPBOARD + PRIMARY\n", clipText.size());
                } else if (ly >= 356 && ly <= 400) {            // QUIT
                    running = false;
                }
            } else {                                            // ---- main view ----
                if (std::fabs(lx - TEMP_CX) < 30 && ly > TRACK_TOP - 14 && ly < TRACK_BOTTOM + 14) dragging = 0;
                else if (std::fabs(lx - BRIGHT_CX) < 30 && ly > TRACK_TOP - 14 && ly < TRACK_BOTTOM + 14) dragging = 1;
                else if (ly >= PRESET_TOP && ly <= PRESET_TOP + PRESET_H) {
                    double pw = (W - 2 * HPAD - 2 * 12) / 3.0;
                    for (int i = 0; i < 3; ++i) {
                        double px = HPAD + i * (pw + 12);
                        if (lx >= px && lx <= px + pw) {
                            st.intensity = intensityForKelvin(PRESET_K[i]);
                            st.brightness = PRESET_B[i];
                            st.enabled = true;
                            applyFilter();
                            saveCfg();
                        }
                    }
                } else if (lx >= HPAD && lx <= W - HPAD && ly >= ZAP_TOP && ly <= ZAP_TOP + ZAP_H) {
                    st.enabled = !st.enabled;
                    applyFilter();
                    saveCfg();
                } else if (st.pwmAvailable && ly >= WARN_Y1 - 14 && ly <= WARN_Y1 + 16) {
                    if (st.pwmStatus != -1) st.pwmSafe = !st.pwmSafe;
                    if (st.pwmSafe) st.pwmStatus = pwm.enable() ? 2 : -1;
                    else st.pwmStatus = pwm.disable() ? 0 : -1;
                    saveCfg();
                }
                if (dragging >= 0) {
                    double v = clampd((TRACK_BOTTOM - ly) / TRACK_H, 0.0, 1.0);
                    if (dragging == 0) st.intensity = v;
                    else st.brightness = std::max(v, floorMin());
                    applyFilter();
                }
            }
            redraw();
        } else if (ev.type == MotionNotify && dragging >= 0) {
            double v = clampd((TRACK_BOTTOM - ev.xmotion.y / (double)scale) / TRACK_H, 0.0, 1.0);
            if (dragging == 0) st.intensity = v;
            else st.brightness = std::max(v, floorMin());
            applyFilter();
            redraw();
        } else if (ev.type == MotionNotify) {
            const double lx = ev.xmotion.x / (double)scale, ly = ev.xmotion.y / (double)scale;
            const bool close = frameless && lx >= W - 46 && ly < HEADER_H;
            const int link = st.view == 1 ? settingsLinkAt(lx, ly) : 0;
            if (close != st.closeHovered || link != st.linkHovered) {
                st.closeHovered = close;
                st.linkHovered = link;
                XDefineCursor(dpy, win, close || link ? handCursor : arrowCursor);
                redraw();
            }
        } else if (ev.type == LeaveNotify) {
            if (st.closeHovered || st.linkHovered) {
                st.closeHovered = false;
                st.linkHovered = 0;
                XDefineCursor(dpy, win, arrowCursor);
                redraw();
            }
        } else if (ev.type == ButtonRelease && ev.xbutton.button == 1) {
            if (dragging >= 0) saveCfg();
            dragging = -1;
        } else if (ev.type == SelectionRequest) {
            // serve the support-bundle text we own on the CLIPBOARD
            const XSelectionRequestEvent& rq = ev.xselectionrequest;
            XSelectionEvent resp;
            std::memset(&resp, 0, sizeof resp);
            resp.type = SelectionNotify;
            resp.display = rq.display;
            resp.requestor = rq.requestor;
            resp.selection = rq.selection;
            resp.target = rq.target;
            resp.time = rq.time;
            resp.property = None;
            if (rq.target == A_TARGETS) {
                Atom targets[] = {A_TARGETS, A_UTF8, XA_STRING};
                XChangeProperty(dpy, rq.requestor, rq.property, XA_ATOM, 32, PropModeReplace,
                                reinterpret_cast<unsigned char*>(targets), 3);
                resp.property = rq.property;
            } else if ((rq.target == A_UTF8 || rq.target == XA_STRING) && !clipText.empty()) {
                XChangeProperty(dpy, rq.requestor, rq.property, rq.target, 8, PropModeReplace,
                                reinterpret_cast<const unsigned char*>(clipText.data()),
                                static_cast<int>(clipText.size()));
                resp.property = rq.property;
            }
            XSendEvent(dpy, rq.requestor, False, 0, reinterpret_cast<XEvent*>(&resp));
            XFlush(dpy);
        }
    };

    updateDesktopScale();
    if (frameless) {
        popoverX = std::max(0, DisplayWidth(dpy, scr) - pxW - static_cast<int>(24 * compositorScale));
        popoverY = static_cast<int>(40 * compositorScale);
        XMoveWindow(dpy, win, popoverX, popoverY);
    }
    applyFilter();
    const int xfd = ConnectionNumber(dpy);
    auto nextMaintenance = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (running) {
        while (XPending(dpy)) {
            XEvent ev;
            XNextEvent(dpy, &ev);
            handleX(ev);
            if (!running) break;
        }
        if (!running) break;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(xfd, &fds);
        int maxfd = xfd;
        if (sigpipe[0] >= 0) { FD_SET(sigpipe[0], &fds); if (sigpipe[0] > maxfd) maxfd = sigpipe[0]; }
        if (ipcfd >= 0) { FD_SET(ipcfd, &fds); if (ipcfd > maxfd) maxfd = ipcfd; }
        struct timeval tv;
        const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
            nextMaintenance - std::chrono::steady_clock::now()).count();
        const auto waitMicros = std::max<decltype(remaining)>(0, remaining);
        tv.tv_sec = waitMicros / 1000000;
        tv.tv_usec = waitMicros % 1000000;
        const int r = ::select(maxfd + 1, &fds, nullptr, nullptr, &tv);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (sigpipe[0] >= 0 && FD_ISSET(sigpipe[0], &fds)) { running = false; break; }
        if (ipcfd >= 0 && FD_ISSET(ipcfd, &fds)) { // on/off/toggle/show from `tapzap <cmd>`
            auto request = ipcAccept(ipcfd);
            const std::string cmd = request.command;
            const bool known = cmd == "status" || cmd == "on" || cmd == "off" || cmd == "toggle" ||
                               cmd == "show" || cmd == "hide" || cmd == "quit";
            if (cmd == "on") { st.enabled = true; applyFilter(); saveCfg(); redraw(); }
            else if (cmd == "off") { st.enabled = false; applyFilter(); saveCfg(); redraw(); }
            else if (cmd == "toggle") { st.enabled = !st.enabled; applyFilter(); saveCfg(); redraw(); }
            else if (cmd == "show") {
                if (dragging >= 0) saveCfg();
                dragging = -1;
                Window focused = None;
                int revert = 0;
                XGetInputFocus(dpy, &focused, &revert);
                focusSeen = (focused == win);
                XMapRaised(dpy, win);
                if (frameless) XMoveWindow(dpy, win, popoverX, popoverY);
                if (popover) XMoveWindow(dpy, win, popoverX, popoverY);
                if (dismissOnFocusLoss) {
                    // Let the compositor grant focus after mapping. Direct
                    // XSetInputFocus can fail with BadMatch on XWayland here.
                    XEvent activate{};
                    activate.xclient.type = ClientMessage;
                    activate.xclient.window = win;
                    activate.xclient.message_type = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
                    activate.xclient.format = 32;
                    activate.xclient.data.l[0] = 1;
                    activate.xclient.data.l[1] = CurrentTime;
                    XSendEvent(dpy, RootWindow(dpy, scr), False,
                               SubstructureRedirectMask | SubstructureNotifyMask, &activate);
                }
                XFlush(dpy); redraw();
            }
            else if (cmd == "hide") hidePanel();
            else if (cmd == "quit") { running = false; }
            XWindowAttributes attrs{}; XGetWindowAttributes(dpy, win, &attrs);
            const bool operationOK = known && ((cmd != "on" && cmd != "toggle" && cmd != "off") || lastFilterError.empty());
            const std::string reply = "{\"ok\":" + std::string(operationOK ? "true" : "false") +
                ",\"enabled\":" + (st.enabled ? "true" : "false") +
                ",\"visible\":" + (attrs.map_state == IsViewable ? "true" : "false") +
                ",\"backend\":" + jsonString(backend->name()) +
                ",\"outputs\":" + std::to_string(backend->outputCount()) +
                ",\"error\":" + jsonString(known ? lastFilterError : "Unknown command") + "}";
            ipcReply(request, reply);
        }
        if (std::chrono::steady_clock::now() >= nextMaintenance) {
            for (auto it = linkOpeners.begin(); it != linkOpeners.end();) {
                const pid_t result = waitpid(*it, nullptr, WNOHANG);
                if (result > 0 || (result < 0 && errno == ECHILD)) it = linkOpeners.erase(it);
                else ++it;
            }
            // A real deadline keeps recovery running even while X/IPC events arrive.
            nextMaintenance = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            if (st.enabled && haveBackend) {
                std::string maintenanceError;
                if (!backend->maintainFilter(st.intensity, st.brightness, maintenanceError)) {
                    lastFilterError = maintenanceError;
                    std::fprintf(stderr, "gamma recovery: %s\n", maintenanceError.c_str());
                    st.enabled = false;
                    redraw();
                }
            }
            const int prev = st.flicker;
            const int prevPercent = st.hardwarePercent;
            refreshFlicker();
            bool needRedraw = (st.flicker != prev || st.hardwarePercent != prevPercent);
            if (st.pwmSafe && st.pwmStatus != -1) {
                pwm.reassertIfDrifted();
                const int ps = pwm.verified() ? 2 : -1;
                if (ps != st.pwmStatus) { st.pwmStatus = ps; needRedraw = true; }
            }
            if (needRedraw && st.view == 0) redraw();
        }
    }
    if (haveBackend) { std::string e; backend->restore(e); }
    if (st.pwmSafe) pwm.disable(); // restore prior backlight on quit (like macOS restoreOnExit)
    if (useOverlay) overlay.destroy();
    if (trayPid > 0) ::kill(trayPid, SIGTERM);
    ipcClose(ipcfd);
    cairo_destroy(xcr);
    cairo_destroy(bcr);
    cairo_surface_destroy(buf);
    cairo_surface_destroy(xs);
    XFreeCursor(dpy, handCursor);
    XFreeCursor(dpy, arrowCursor);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
    return 0;
}

} // namespace

int runGui(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[2], "--shot") == 0) {
        if (argc < 7) { std::fprintf(stderr, "usage: gui --shot <file.ppm> <intensity> <brightness> <0|1> [scale]\n"); return 2; }
        return shot(argv[3], std::atof(argv[4]), std::atof(argv[5]), std::atoi(argv[6]) != 0, argc > 7 ? std::atoi(argv[7]) : 2);
    }
    const double i0 = argc > 2 ? std::atof(argv[2]) : 1.0;
    const double b0 = argc > 3 ? std::atof(argv[3]) : 0.70;
    const bool e0 = argc > 4 ? (std::atoi(argv[4]) != 0) : false;
    return liveWindow(i0, b0, e0);
}

#else
#include <cstdio>
int runGui(int, char**) {
    std::fprintf(stderr, "Tap Zap GUI requires Linux + X11 + Cairo (build on a Linux box with libcairo2-dev + libfreetype6-dev).\n");
    return 2;
}
#endif
