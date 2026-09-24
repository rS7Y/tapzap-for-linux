// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// PWM-Safe requests maximum hardware backlight, then verifies the reported setting.
// This does not measure optical flicker. Prior backlight settings are restored on disable.
#pragma once
#include "platform.hpp"
#include "logind_brightness.hpp"
#include <cstdio>
#include <dirent.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <climits>

namespace tapzap {
class PwmSafe {
public:
    explicit PwmSafe(std::string root = "/sys/class/backlight", bool directWrites = true)
        : root_(std::move(root)), directWrites_(directWrites) {}
    ~PwmSafe() { disable(); }
    bool detect() {
        if (enabled_) return available();
        targets_.clear();
        const bool logind = logind_.available();
        DIR* d = ::opendir(root_.c_str());
        if (d) {
            struct dirent* e;
            while ((e = ::readdir(d)) != nullptr) {
                if (e->d_name[0] == '.') continue;
                const std::string base = root_ + "/" + e->d_name;
                const long mx = static_cast<long>(readNumFile(base + "/max_brightness", -1));
                const std::string bp = base + "/brightness";
                if (mx > 0 && mx <= UINT_MAX && ::access(bp.c_str(), R_OK) == 0 &&
                    (logind || (directWrites_ && ::access(bp.c_str(), W_OK) == 0)))
                    targets_.push_back({bp, e->d_name, mx, -1});
            }
            ::closedir(d);
        }
        return available();
    }
    bool available() const { return !targets_.empty(); }
    bool enable() {
        if (!available()) return false;
        if (restorePending_ && !disable()) return false;
        if (enabled_) { reassertIfDrifted(); return verified(); }
        for (auto& t : targets_) {
            t.priorRaw = static_cast<long>(readNumFile(t.path, -1));
            if (t.priorRaw < 0 || t.priorRaw > t.maxRaw) {
                for (auto& target : targets_) target.priorRaw = -1;
                return false;
            }
        }
        enabled_ = true;
        for (auto& t : targets_) {
            if (!write(t, t.maxRaw)) { disable(); return false; }
        }
        if (!verified()) { disable(); return false; }
        return true;
    }
    bool disable() {
        bool restored = true;
        for (auto& t : targets_) {
            if (t.priorRaw < 0) continue;
            if (readNumFile(t.path, -1) == t.priorRaw || write(t, t.priorRaw)) t.priorRaw = -1;
            else restored = false;
        }
        restorePending_ = !restored;
        enabled_ = !restored; // Preserve snapshots when restoration fails so a later attempt can retry.
        return restored;
    }
    bool verified() const {
        if (!enabled_ || targets_.empty()) return false;
        for (const auto& t : targets_)
            if (readNumFile(t.path, -1) != t.maxRaw) return false;
        return true;
    }
    void reassertIfDrifted() {
        if (!enabled_ || restorePending_) return;
        for (auto& t : targets_)
            if (readNumFile(t.path, -1) != t.maxRaw) write(t, t.maxRaw);
    }
    bool enabled() const { return enabled_; }
private:
    struct Target { std::string path, device; long maxRaw, priorRaw; };
    bool write(const Target& t, long value) {
        bool ok = false;
        if (directWrites_ && ::access(t.path.c_str(), W_OK) == 0) {
            FILE* f = std::fopen(t.path.c_str(), "w");
            if (f) { ok = std::fprintf(f, "%ld", value) > 0; if (std::fclose(f) != 0) ok = false; }
        }
        if (!ok) ok = logind_.set(t.device, static_cast<unsigned int>(value));
        ok = ok && readNumFile(t.path, -1) == value;
        if (!ok) std::fprintf(stderr, "[PWM] Could not verify %s brightness %ld\n", t.device.c_str(), value);
        return ok;
    }
    std::string root_;
    bool directWrites_;
    LogindBrightness logind_;
    std::vector<Target> targets_;
    bool enabled_ = false;
    bool restorePending_ = false;
};
} // namespace tapzap
