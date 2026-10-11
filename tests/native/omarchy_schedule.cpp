// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../src/omarchy-hyprland/src/linux/schedule.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace tapzap;
static double date(int month, int day, int hour, int minute, int second = 0) {
    std::tm t{};
    t.tm_year = 126;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    t.tm_isdst = -1;
    return std::mktime(&t);
}
int main() {
    setenv("TZ", "UTC", 1);
    tzset();
    DailySchedule config;
    assert(config.valid());
    config.minutes[1] = config.minutes[0];
    assert(!config.valid());
    config.selected[1] = false;
    assert(config.valid());
    config = DailySchedule{};
    ScheduleEngine s(config);
    const double start = date(10, 11, 18, 0);
    s.arm(start - 1, .2, false);
    assert(!s.tick(start - 1).changed);
    auto out = s.tick(start);
    assert(out.changed && out.started && std::abs(out.intensity - .2) < 1e-9);
    out = s.tick(start + 30);
    assert(std::abs(out.intensity - (.2 + DailySchedule::targets[1]) * .5) < 1e-9);
    out = s.tick(start + 60);
    assert(std::abs(out.intensity - DailySchedule::targets[1]) < 1e-9 && !s.transitioning());
    s.manual(start + 61, .8);
    assert(s.holding());
    assert(!s.tick(start + 300).changed);
    s.arm(start + 10, 0, false); // Explicit OFF must skip the entire current start minute.
    assert(s.nextStart() == date(10, 11, 22, 0));
    assert(!s.tick(start + 20).started);
    s.arm(start, 0, true);
    assert(s.tick(start).started);
    s.manual(start + 15, .4);
    s.recover(start + 90, .4); // A manual hold survives wake.
    assert(s.holding() && !s.tick(start + 91).changed);
    s.arm(start - 1, .3, false);
    out = s.tick(start + 300); // Missed start fades from actual output on wake.
    assert(out.started && std::abs(out.intensity - .3) < 1e-9);
    assert(std::abs(s.tick(start + 360).intensity - DailySchedule::targets[1]) < 1e-9);
    s.arm(start - 1, .2, false);
    s.tick(start);
    const auto beforeWake = s.tick(start + 15).intensity;
    s.recover(start + 120, beforeWake);
    assert(s.transitioning());
    assert(std::abs(s.tick(start + 120).intensity - beforeWake) < 1e-9);
    assert(std::abs(s.tick(start + 180).intensity - DailySchedule::targets[1]) < 1e-9);
    s.disable();
    assert(!s.tick(start + 600).changed);
    config.minutes = {18 * 60, 18 * 60 + 1, 18 * 60 + 2};
    s = ScheduleEngine(config);
    s.arm(start - 1, .1, false);
    s.tick(start);
    s.tick(start + 45);
    out = s.tick(start + 60);
    assert(out.started);
    out = s.tick(start + 120);
    assert(out.started);
    assert(std::abs(s.tick(start + 180).intensity - 1) < 1e-9);
    s.manual(start + 181, .7);
    s.recover(start - 300, .7);
    assert(s.holding());
    setenv("TZ", "America/New_York", 1);
    tzset();
    config = DailySchedule{};
    config.minutes = {150, 180, 1320};
    auto events = config.events(date(3, 8, 0, 0)); // Spring gap 02:30 becomes first valid 03:00.
    int count = 0;
    for (const auto &e : events)
        if (e.at == date(3, 8, 3, 0)) {
            ++count;
            assert(e.phase == 1);
        }
    assert(count == 1); // Collapsed starts run once; later wall-clock entry wins.
    config.minutes = {90, 1080, 1320};
    events = config.events(date(11, 1, 0, 0));
    double first = 0;
    for (const auto &e : events) {
        std::time_t t = e.at;
        std::tm local{};
        localtime_r(&t, &local);
        if (local.tm_mon == 10 && local.tm_mday == 1 && e.phase == 0) {
            first = e.at;
            assert(local.tm_isdst == 1);
        }
    }
    assert(first > 0); // Repeated 01:30 uses the first occurrence only.
    std::puts(
        "PASS: schedule validation, easing, holds, OFF, recovery, adjacent starts, DST gap/fold");
}
