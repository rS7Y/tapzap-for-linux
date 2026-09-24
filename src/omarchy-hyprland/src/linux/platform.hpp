// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Linux/POSIX platform helpers: config persistence (INI), XDG autostart (launch-at-login),
// built-in backlight read (sysfs). Header-only, POSIX (compiles on Linux + macOS dev box).
#pragma once
#include "tapzap_core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdint>
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace tapzap {

inline std::string homeDir() {
    const char* h = std::getenv("HOME");
    return (h && *h) ? std::string(h) : std::string(".");
}

inline void mkdirP(const std::string& path) {
    std::string p;
    for (size_t i = 0; i < path.size(); ++i) {
        p += path[i];
        if (path[i] == '/' && p.size() > 1) ::mkdir(p.c_str(), 0755);
    }
    ::mkdir(path.c_str(), 0755);
}

inline std::string exePath() {
    char buf[4096];
    ssize_t n = ::readlink("/proc/self/exe", buf, sizeof buf - 1);
    if (n <= 0) return "tapzap";
    buf[n] = 0;
    return std::string(buf);
}

// The command a persistent launcher (autostart / .desktop) should run to relaunch us.
// Inside an AppImage, /proc/self/exe points at a per-run /tmp extraction dir that is deleted
// on exit, so we must use $APPIMAGE — the runtime-provided absolute path to the .AppImage
// file the user actually downloaded (stable across reboots). Fall back to the real exe path.
inline std::string launcherPath() {
    if (const char* ai = std::getenv("APPIMAGE")) { if (*ai) return std::string(ai); }
    return exePath();
}

// ---- config (INI) ----
struct Config {
    bool enabled = false;
    double intensity = 1.0;
    double brightness = 0.70;
    bool allowExtremeDim = false;
    bool pwmSafeMode = false;
    bool launchAtLogin = false;
};

inline std::string configDir() {
    const char* x = std::getenv("XDG_CONFIG_HOME");
    std::string base = (x && *x) ? std::string(x) : (homeDir() + "/.config");
    return base + "/TapZapOmarchy";
}
inline std::string configPath() { return configDir() + "/settings.ini"; }

inline Config loadConfig() {
    Config c;
    FILE* f = std::fopen(configPath().c_str(), "r");
    if (!f) return c;
    char line[256];
    while (std::fgets(line, sizeof line, f)) {
        char key[64] = {0}, val[160] = {0};
        if (std::sscanf(line, "%63[^=]=%159[^\n]", key, val) == 2) {
            std::string k(key), v(val);
            while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\t')) v.pop_back();
            const bool truthy = (v == "1" || v == "true");
            if (k == "enabled") c.enabled = truthy;
            else if (k == "intensity") c.intensity = clampd(std::atof(v.c_str()), 0.0, 1.0);
            else if (k == "brightness") c.brightness = clampd(std::atof(v.c_str()), 0.0, 1.0);
            else if (k == "allowExtremeDim") c.allowExtremeDim = truthy;
            else if (k == "pwmSafeMode") c.pwmSafeMode = truthy;
            else if (k == "launchAtLogin") c.launchAtLogin = truthy;
        }
    }
    std::fclose(f);
    return c;
}

inline void saveConfig(const Config& c) {
    mkdirP(configDir());
    FILE* f = std::fopen(configPath().c_str(), "w");
    if (!f) return;
    std::fprintf(f, "enabled=%d\n", c.enabled ? 1 : 0);
    std::fprintf(f, "intensity=%.4f\n", c.intensity);
    std::fprintf(f, "brightness=%.4f\n", c.brightness);
    std::fprintf(f, "allowExtremeDim=%d\n", c.allowExtremeDim ? 1 : 0);
    std::fprintf(f, "pwmSafeMode=%d\n", c.pwmSafeMode ? 1 : 0);
    std::fprintf(f, "launchAtLogin=%d\n", c.launchAtLogin ? 1 : 0);
    std::fclose(f);
}

// ---- launch at login (XDG autostart) ----
inline std::string autostartPath() {
    const char* x = std::getenv("TAPZAP_DESKTOP_CONFIG_HOME");
    if (!x || !*x) x = std::getenv("XDG_CONFIG_HOME");
    std::string base = (x && *x) ? std::string(x) : (homeDir() + "/.config");
    return base + "/autostart/tapzap-omarchy.desktop";
}

inline bool isLaunchAtLogin() { return ::access(autostartPath().c_str(), F_OK) == 0; }

// Desktop Entry Exec quoting has a second backslash-escape layer and percent field codes.
inline std::string desktopQuote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '\n' || c == '\r') return {};
        if (c == '%') { out += "%%"; continue; }
        if (c == '\\') out += "\\\\\\\\";
        else if (c == '\"' || c == '`' || c == '$') { out += "\\\\"; out += c; }
        else out += c;
    }
    return out + "\"";
}
inline bool setLaunchAtLogin(bool enable) {
    const std::string path = autostartPath();
    if (!enable) return ::remove(path.c_str()) == 0 || errno == ENOENT;
    const char* wrapper = std::getenv("TAPZAP_LAUNCHER");
    const std::string quoted = desktopQuote(wrapper && *wrapper ? wrapper : launcherPath());
    if (quoted.empty()) return false;
    const std::string command = wrapper && *wrapper ? quoted + " start-hidden" : quoted + " gui";
    mkdirP(path.substr(0, path.rfind('/')));
    const std::string pending = path + ".tmp." + std::to_string(getpid());
    FILE* f = std::fopen(pending.c_str(), "w");
    if (!f) return false;
    bool ok = std::fprintf(f,
        "[Desktop Entry]\nType=Application\nName=TAP ZAP Omarchy\n"
        "Comment=Screen color temperature + brightness\nExec=%s\n"
        "Terminal=false\nOnlyShowIn=Hyprland;\nX-GNOME-Autostart-enabled=true\n", command.c_str()) > 0;
    if (std::fclose(f) != 0) ok = false;
    if (ok) ok = ::rename(pending.c_str(), path.c_str()) == 0;
    if (!ok) ::remove(pending.c_str());
    return ok;
}

// ---- built-in backlight (sysfs) ----
inline double readNumFile(const std::string& path, double fallback) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return fallback;
    double v = fallback;
    if (std::fscanf(f, "%lf", &v) != 1) v = fallback;
    std::fclose(f);
    return v;
}

struct HwBrightness { bool available = false; double value = 0.0; };

inline HwBrightness readHardwareBrightness() {
    HwBrightness r;
    DIR* d = ::opendir("/sys/class/backlight");
    if (!d) return r;
    struct dirent* e;
    while ((e = ::readdir(d)) != nullptr) {
        if (e->d_name[0] == '.') continue;
        const std::string base = std::string("/sys/class/backlight/") + e->d_name;
        const double cur = readNumFile(base + "/brightness", -1);
        const double mx = readNumFile(base + "/max_brightness", -1);
        if (cur >= 0 && mx > 0) { r.available = true; r.value = clampd(cur / mx, 0.0, 1.0); break; }
    }
    ::closedir(d);
    return r;
}

// ---- diagnostic support bundle (clipboard) ----
inline std::string firstLineContaining(const std::string& path, const char* key) {
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return "";
    char line[512];
    std::string out;
    while (std::fgets(line, sizeof line, f)) {
        if (std::strstr(line, key)) { out = line; break; }
    }
    std::fclose(f);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

inline std::string deviceFingerprint() {
    std::string mid;
    FILE* f = std::fopen("/etc/machine-id", "r");
    if (f) { char b[64] = {0}; if (std::fgets(b, sizeof b, f)) mid = b; std::fclose(f); }
    uint64_t h = 1469598103934665603ull;
    for (char c : mid) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
    char out[20];
    std::snprintf(out, sizeof out, "%012llx", static_cast<unsigned long long>(h) & 0xffffffffffffull);
    return std::string(out);
}

inline std::string buildSupportBundle(double intensity, double brightness, bool enabled,
                                      bool extremeDim, bool pwmSafe, bool launchAtLogin) {
    struct utsname u;
    std::string kernel;
    if (uname(&u) == 0) kernel = std::string(u.sysname) + " " + u.release + " " + u.machine;
    const std::string distro = firstLineContaining("/etc/os-release", "PRETTY_NAME=");
    const std::string cpu = firstLineContaining("/proc/cpuinfo", "model name");
    const std::string mem = firstLineContaining("/proc/meminfo", "MemTotal");
    const HwBrightness hb = readHardwareBrightness();
    const char* dpyEnv = std::getenv("DISPLAY");
    char buf[2048];
    std::snprintf(buf, sizeof buf,
                  "TAP ZAP - Linux support bundle\n"
                  "==============================\n"
                  "Kernel: %s\n"
                  "Distro: %s\n"
                  "CPU: %s\n"
                  "%s\n"
                  "DISPLAY: %s\n"
                  "HW brightness: %s%.0f%%\n"
                  "--- Filter ---\n"
                  "Enabled: %s\n"
                  "Warmth: %d K (intensity %.3f)\n"
                  "Brightness: %.0f%%\n"
                  "--- Settings ---\n"
                  "Allow extreme dim: %s\n"
                  "PWM-safe mode: %s\n"
                  "Launch at login: %s\n"
                  "Device: %s\n",
                  kernel.c_str(), distro.c_str(), cpu.c_str(), mem.c_str(),
                  dpyEnv ? dpyEnv : "(none)",
                  hb.available ? "" : "n/a ", hb.available ? hb.value * 100 : 0.0,
                  enabled ? "YES" : "NO",
                  static_cast<int>(std::lround(kelvinForIntensity(intensity))), intensity,
                  brightness * 100,
                  extremeDim ? "YES" : "NO", pwmSafe ? "YES" : "NO", launchAtLogin ? "YES" : "NO",
                  deviceFingerprint().c_str());
    return std::string(buf);
}

} // namespace tapzap
