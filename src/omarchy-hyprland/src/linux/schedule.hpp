// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Daily local-time scheduling, independent of the UI and display backend.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <vector>

namespace tapzap {
struct DailySchedule {
    std::array<int, 3> minutes{420, 1080, 1320};
    std::array<bool, 3> selected{true, true, true};
    static constexpr std::array<double, 3> targets{1.0 - 4000.0 / 6500.0, 1.0 - 2700.0 / 6500.0,
                                                   1.0};
    struct Event {
        double at;
        int phase;
    };
    bool wellFormed() const {
        return std::all_of(minutes.begin(), minutes.end(),
                           [](int m) { return m >= 0 && m < 1440; });
    }
    bool valid() const {
        if (!wellFormed())
            return false;
        int count = 0;
        for (int i = 0; i < 3; ++i)
            if (selected[i]) {
                ++count;
                for (int j = 0; j < i; ++j)
                    if (selected[j] && minutes[i] == minutes[j])
                        return false;
            }
        return count > 0;
    }
    // Choose the first occurrence of an ambiguous time. For a spring gap,
    // choose the first valid wall-clock minute after the requested minute.
    static double eventTime(std::tm day, int minute) {
        day.tm_hour = minute / 60;
        day.tm_min = minute % 60;
        day.tm_sec = 0;
        const auto same = [&](const std::tm &t) {
            return t.tm_year == day.tm_year && t.tm_mon == day.tm_mon && t.tm_mday == day.tm_mday;
        };
        double earliest = 0;
        for (int dst : {0, 1}) {
            std::tm candidate = day;
            candidate.tm_isdst = dst;
            const auto at = std::mktime(&candidate);
            std::tm local{};
            localtime_r(&at, &local);
            if (same(local) && local.tm_hour * 60 + local.tm_min == minute &&
                (!earliest || at < earliest))
                earliest = at;
        }
        if (earliest)
            return earliest;
        day.tm_isdst = -1;
        const auto approximate = std::mktime(&day);
        // mktime normalizes nonexistent 02:30 to 03:30 on many platforms.
        // Search the transition neighborhood so this matches next-valid-time semantics.
        int bestMinute = 1440;
        for (std::time_t at = approximate - 10800; at <= approximate + 10800; at += 60) {
            std::tm local{};
            localtime_r(&at, &local);
            const int m = local.tm_hour * 60 + local.tm_min;
            if (same(local) && m >= minute &&
                (m < bestMinute || (m == bestMinute && at < earliest))) {
                bestMinute = m;
                earliest = at;
            }
        }
        return earliest;
    }
    std::vector<Event> events(double now) const {
        std::vector<Event> result;
        if (!valid())
            return result;
        const std::time_t at = now;
        std::tm local{};
        localtime_r(&at, &local);
        for (int offset = -2; offset <= 3; ++offset) {
            std::tm day = local;
            day.tm_mday += offset;
            day.tm_hour = 12;
            day.tm_min = day.tm_sec = 0;
            day.tm_isdst = -1;
            const auto noon = std::mktime(&day);
            localtime_r(&noon, &day);
            for (int i = 0; i < 3; ++i)
                if (selected[i]) {
                    const double event = eventTime(day, minutes[i]);
                    if (event)
                        result.push_back({event, i});
                }
        }
        std::sort(result.begin(), result.end(), [&](const Event &a, const Event &b) {
            return a.at == b.at ? minutes[a.phase] < minutes[b.phase] : a.at < b.at;
        });
        std::vector<Event> distinct;
        for (const auto &e : result) {
            if (!distinct.empty() && distinct.back().at == e.at)
                distinct.back() = e;
            else
                distinct.push_back(e);
        }
        return distinct;
    }
    double next(double now, bool includeCurrentMinute = false) const {
        const double earliest = includeCurrentMinute ? std::floor(now / 60) * 60 : now;
        for (const auto &e : events(now))
            if (includeCurrentMinute ? e.at >= earliest : e.at > earliest)
                return e.at;
        return 0;
    }
};

class ScheduleEngine {
    DailySchedule config_;
    bool enabled_ = false, holding_ = false, fading_ = false, waiting_ = false;
    double next_ = 0, value_ = 0, from_ = 0, to_ = 0, start_ = 0, last_ = 0;

  public:
    struct Output {
        bool changed = false, started = false;
        double intensity = 0, progress = 0;
    };
    explicit ScheduleEngine(DailySchedule config = {}) : config_(config) {}
    bool enabled() const {
        return enabled_;
    }
    bool holding() const {
        return holding_;
    }
    bool waiting() const {
        return waiting_;
    }
    bool transitioning() const {
        return fading_;
    }
    double nextStart() const {
        return next_;
    }
    void configure(const DailySchedule &value) {
        config_ = value;
    }
    void disable() {
        enabled_ = holding_ = fading_ = waiting_ = false;
        next_ = 0;
    }
    void arm(double now, double current, bool includeCurrentMinute = true) {
        enabled_ = config_.valid();
        value_ = current;
        last_ = now;
        next_ = enabled_ ? config_.next(now, includeCurrentMinute) : 0;
        holding_ = fading_ = false;
        waiting_ = enabled_;
    }
    void manual(double now, double current) {
        if (!enabled_)
            return;
        value_ = current;
        next_ = config_.next(now);
        last_ = now;
        holding_ = true;
        waiting_ = fading_ = false;
    }
    void recover(double now, double current, bool rebase = false) {
        value_ = current;
        if (!enabled_)
            return;
        if (rebase || now < last_ - 2) {
            next_ = config_.next(now);
            fading_ = false;
        } else if (fading_) {
            // Resume an interrupted fade from the last actual output, without
            // jumping ahead or leaving the filter stranded until tomorrow.
            from_ = current;
            start_ = now;
        }
        last_ = now;
        // A pending future start / manual hold must not be replayed on wake.
    }
    Output tick(double now) {
        Output out;
        out.intensity = value_;
        if (!enabled_ || !next_)
            return out;
        if (now < last_ - 2)
            recover(now, value_, true);
        if (now >= next_) {
            DailySchedule::Event latest{next_, -1};
            for (const auto &e : config_.events(now))
                if (e.at >= next_ && e.at <= now)
                    latest = e;
            if (latest.phase >= 0) {
                // Long gaps recover from the last actual output over a full minute.
                from_ = value_;
                to_ = DailySchedule::targets[latest.phase];
                start_ = now - latest.at > 2 ? now : latest.at;
                fading_ = true;
                holding_ = waiting_ = false;
                out.started = true;
            }
            next_ = config_.next(now);
        }
        if (fading_) {
            const double t = std::clamp((now - start_) / 60.0, 0.0, 1.0);
            const double eased = t * t * t * (t * (t * 6 - 15) + 10);
            value_ = from_ + (to_ - from_) * eased;
            out.changed = true;
            out.intensity = value_;
            out.progress = eased;
            if (t >= 1)
                fading_ = false;
        }
        last_ = now;
        return out;
    }
};
} // namespace tapzap
