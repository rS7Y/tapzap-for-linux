// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// One headless CTM owner for the native Omarchy panel. No X11 window or tray process.
#include "service.hpp"
#include "gamma_backend.hpp"
#include "ipc.hpp"
#include "platform.hpp"
#include "pwm.hpp"
#include "schedule.hpp"
#include <csignal>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <ctime>
#include <sys/prctl.h>
#include <sys/wait.h>

using namespace tapzap;
namespace {
volatile sig_atomic_t stopping = 0;
void stop(int) {
    stopping = 1;
}
std::string json(const std::string &text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (c == '\n')
            out += "\\n";
        else if (c == '\r')
            out += "\\r";
        else if (c == '\t')
            out += "\\t";
        else if (c >= 32)
            out += c;
    }
    return out + '"';
}
bool atomicWrite(const std::string &path, const std::string &text) {
    mkdirP(configDir());
    std::string pattern = path + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back(0);
    int fd = mkstemp(name.data());
    if (fd < 0)
        return false;
    size_t offset = 0;
    while (offset < text.size()) {
        const auto n = write(fd, text.data() + offset, text.size() - offset);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        offset += n;
    }
    bool ok = offset == text.size() && fsync(fd) == 0;
    if (close(fd))
        ok = false;
    if (ok)
        ok = rename(name.data(), path.c_str()) == 0;
    if (!ok)
        unlink(name.data());
    return ok;
}
struct SchedulePreferences {
    DailySchedule daily;
    bool enabled = false, review = false;
};
std::string zoneKey() {
    if (const char *zone = std::getenv("TZ"))
        return zone;
    char path[1024];
    const auto n = readlink("/etc/localtime", path, sizeof(path));
    if (n > 0)
        return std::string(path, n);
    struct stat info{};
    if (stat("/etc/localtime", &info) == 0)
        return std::to_string(info.st_mtime);
    return "local";
}
SchedulePreferences loadSchedule() {
    SchedulePreferences prefs;
    std::ifstream file(configDir() + "/schedule.ini");
    if (!file)
        return prefs;
    std::string line;
    int fields = 0;
    while (std::getline(file, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos) {
            prefs.review = true;
            continue;
        }
        const auto key = line.substr(0, split), value = line.substr(split + 1);
        if (key == "zone" || key == "next")
            continue;
        std::istringstream in(value);
        double n = 0;
        std::string extra;
        if (!(in >> n) || (in >> extra) || !std::isfinite(n)) {
            prefs.review = true;
            continue;
        }
        if (key == "enabled") {
            prefs.enabled = n == 1;
            prefs.review |= n != 0 && n != 1;
            fields |= 1;
        } else
            for (int i = 0; i < 3; ++i) {
                if (key == "minute" + std::to_string(i)) {
                    if (n < 0 || n >= 1440 || n != std::floor(n))
                        prefs.review = true;
                    else
                        prefs.daily.minutes[i] = n;
                    fields |= 1 << (i + 1);
                } else if (key == "selected" + std::to_string(i)) {
                    prefs.review |= n != 0 && n != 1;
                    prefs.daily.selected[i] = n == 1;
                    fields |= 1 << (i + 4);
                }
            }
    }
    prefs.review |=
        fields != 127 || !prefs.daily.wellFormed() || (prefs.enabled && !prefs.daily.valid());
    if (prefs.review)
        prefs.enabled = false;
    return prefs;
}
bool readNumber(std::istringstream &in, double &number) {
    std::string rest;
    return bool(in >> number) && std::isfinite(number) && !(in >> rest);
}
} // namespace

int runManagedService(bool resumeOnly) {
    const std::string directory = ipcDirectory();
    if (directory.empty())
        return 2;
    const std::string marker = directory + "/resume";
    struct stat info{};
    if (resumeOnly && (lstat(marker.c_str(), &info) || !S_ISREG(info.st_mode) ||
                       info.st_uid != getuid() || (info.st_mode & 077)))
        return 0;
    const pid_t parent = getpid(), child = fork();
    if (child < 0)
        return 2;
    if (child == 0) {
        // Quickshell hard-kills its direct Process child on component teardown.
        // Keep the filter engine one generation below that child so it receives
        // SIGTERM and can restore hardware brightness and flush preferences.
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (getppid() != parent)
            _exit(0);
        signal(SIGPIPE, SIG_IGN);
        const int result = runService(true);
        _exit(result);
    }
    int result = 0;
    while (waitpid(child, &result, 0) < 0) {
        if (errno != EINTR)
            return 2;
    }
    return WIFEXITED(result) ? WEXITSTATUS(result) : 2;
}

int runService(bool waitForOwner) {
    auto lock = std::make_unique<InstanceLock>();
    bool acquired = lock->acquire();
    // A replacement plugin can load before the old engine finishes flushing
    // settings and restoring backlights. Wait for its ownership lock to close.
    for (int attempt = 0; !acquired && waitForOwner && attempt < 80; ++attempt) {
        usleep(100000);
        lock = std::make_unique<InstanceLock>();
        acquired = lock->acquire();
    }
    if (!acquired)
        return ipcSend("status") ? 0 : 2;
    const int listener = ipcListen();
    if (listener < 0)
        return 2;
    const std::string resumeMarker = ipcDirectory() + "/resume";
    int marker = open(resumeMarker.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (marker >= 0)
        close(marker);
    struct sigaction action{};
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    Config config = loadConfig();
    // Like the current Mac app, startup never replays the last manual ON state.
    config.enabled = false;
    if (!std::isfinite(config.intensity))
        config.intensity = 1;
    if (!std::isfinite(config.brightness))
        config.brightness = .7;
    config.brightness = std::clamp(config.brightness, config.allowExtremeDim ? 0.0 : .1, 1.0);
    config.launchAtLogin = isLaunchAtLogin();
    SchedulePreferences preferences = loadSchedule();
    ScheduleEngine schedule(preferences.daily);
    auto wallNow = [] {
        return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    };
    double now = wallNow();
    std::string zone = zoneKey();
    if (preferences.enabled)
        schedule.arm(now, 0, false);
    // A restart starts OFF and watches a future event. Wake within this process
    // catches a missed event smoothly; neither path replays a previous manual preset.
    std::unique_ptr<IGammaBackend> backend;
    PwmSafe pwm;
    bool available = pwm.detect();
    std::string filterError, pwmError, storageError;
    bool dirty = false, activationFade = false;
    double appliedBrightness = config.brightness, fadeBrightness = 1;
    double lastWall = now;
    auto lastSteady = std::chrono::steady_clock::now();
    auto nextMaintenance = lastSteady;
    auto nextSave = lastSteady;
    auto save = [&] {
        std::ostringstream ini;
        ini << std::setprecision(17) << "enabled=" << config.enabled
            << "\nintensity=" << config.intensity << "\nbrightness=" << config.brightness
            << "\nallowExtremeDim=" << config.allowExtremeDim
            << "\npwmSafeMode=" << config.pwmSafeMode << "\nlaunchAtLogin=" << config.launchAtLogin
            << '\n';
        std::ostringstream times;
        times << std::setprecision(17) << "enabled=" << preferences.enabled
              << "\nnext=" << schedule.nextStart() << "\nzone=" << zone << '\n';
        for (int i = 0; i < 3; ++i)
            times << "minute" << i << '=' << preferences.daily.minutes[i] << "\nselected" << i
                  << '=' << preferences.daily.selected[i] << '\n';
        const bool ok = atomicWrite(configPath(), ini.str()) &&
                        atomicWrite(configDir() + "/schedule.ini", times.str());
        dirty = !ok;
        storageError =
            ok ? "" : "Could not save settings. Check the configuration directory permissions.";
        return ok;
    };
    auto changed = [&] {
        dirty = true;
        nextSave = std::chrono::steady_clock::now() + std::chrono::milliseconds(350);
    };
    auto apply = [&](double brightness) {
        appliedBrightness = brightness;
        if (!config.enabled) {
            if (backend) {
                std::string error;
                if (!backend->restore(error))
                    filterError = error;
                backend.reset();
            }
            return filterError.empty();
        }
        if (config.pwmSafeMode && available && !pwm.enabled())
            pwmError = pwm.enable() ? "" : "Could not verify maximum hardware brightness";
        if (!backend) {
            auto candidate = makeDefaultBackend();
            std::string error;
            if (!candidate->init(error)) {
                filterError = error;
                config.enabled = false;
                return false;
            }
            backend = std::move(candidate);
        }
        std::string error;
        if (!backend->setFilter(config.intensity, brightness, error)) {
            filterError = error;
            config.enabled = false;
            backend.reset();
            return false;
        }
        filterError.clear();
        return true;
    };
    auto setOn = [&](bool on) {
        activationFade = false;
        config.enabled = on;
        filterError.clear();
        if (preferences.enabled)
            schedule.arm(now, on ? config.intensity : 0, false);
        if (on && config.pwmSafeMode && available && !pwm.enabled())
            pwmError = pwm.enable() ? "" : "Could not verify maximum hardware brightness";
        const bool ok = apply(config.brightness);
        changed();
        return ok;
    };
    auto status = [&](bool ok, const std::string &error) {
        const auto hardware = readHardwareBrightness();
        std::ostringstream out;
        out << std::setprecision(10) << "{\"ok\":" << (ok ? "true" : "false")
            << ",\"engine\":true,\"enabled\":" << (config.enabled ? "true" : "false")
            << ",\"intensity\":" << config.intensity << ",\"brightness\":" << config.brightness
            << ",\"extreme\":" << (config.allowExtremeDim ? "true" : "false")
            << ",\"login\":" << (config.launchAtLogin ? "true" : "false")
            << ",\"pwmAvailable\":" << (available ? "true" : "false")
            << ",\"pwmRequested\":" << (config.pwmSafeMode ? "true" : "false")
            << ",\"pwmVerified\":" << (pwm.verified() ? "true" : "false") << ",\"hardwarePercent\":"
            << (hardware.available ? int(std::lround(hardware.value * 100)) : -1)
            << ",\"backend\":\"hyprland-ctm-v2\",\"outputs\":"
            << (backend ? backend->outputCount() : 0) << ",\"error\":"
            << json(!error.empty()         ? error
                    : !filterError.empty() ? filterError
                                           : storageError)
            << ",\"pwmError\":" << json(pwmError)
            << ",\"schedule\":{\"enabled\":" << (preferences.enabled ? "true" : "false")
            << ",\"review\":" << (preferences.review ? "true" : "false")
            << ",\"holding\":" << (schedule.holding() ? "true" : "false")
            << ",\"waiting\":" << (schedule.waiting() ? "true" : "false")
            << ",\"transitioning\":" << (schedule.transitioning() ? "true" : "false")
            << ",\"next\":" << schedule.nextStart() << ",\"minutes\":[";
        for (int i = 0; i < 3; ++i)
            out << (i ? "," : "") << preferences.daily.minutes[i];
        out << "],\"selected\":[";
        for (int i = 0; i < 3; ++i)
            out << (i ? "," : "") << (preferences.daily.selected[i] ? "true" : "false");
        out << "]}}";
        return out.str();
    };
    while (!stopping) {
        const auto steady = std::chrono::steady_clock::now();
        now = wallNow();
        const double elapsed = std::chrono::duration<double>(steady - lastSteady).count();
        const std::string currentZone = zoneKey();
        if (std::abs((now - lastWall) - elapsed) > 2 || elapsed > 2 || currentZone != zone) {
            schedule.recover(now, config.enabled ? config.intensity : 0, currentZone != zone);
            fadeBrightness = appliedBrightness;
            if (!schedule.transitioning())
                activationFade = false;
            zone = currentZone;
        }
        lastSteady = steady;
        lastWall = now;
        const auto out = schedule.tick(now);
        if (out.changed) {
            if (out.started) {
                activationFade = !config.enabled;
                fadeBrightness = activationFade ? 1 : config.brightness;
                config.enabled = true;
                changed();
            }
            config.intensity = out.intensity;
            const double brightness =
                activationFade
                    ? fadeBrightness + (config.brightness - fadeBrightness) * out.progress
                    : config.brightness;
            if (!apply(brightness)) {
                if (preferences.enabled)
                    schedule.arm(now, 0, false);
                else
                    schedule.disable();
                activationFade = false;
                changed();
            }
            if (!schedule.transitioning()) {
                activationFade = false;
                changed();
            }
        }
        if (steady >= nextMaintenance) {
            if (config.enabled && backend) {
                std::string error;
                if (!backend->maintainFilter(config.intensity, appliedBrightness, error)) {
                    if (backend->outputCount() == 0 &&
                        error == "No active Hyprland filter output") {
                        filterError = error;
                    } else {
                        filterError = error;
                        config.enabled = false;
                        backend.reset();
                        if (preferences.enabled)
                            schedule.arm(now, 0, false);
                        else
                            schedule.disable();
                        changed();
                    }
                } else
                    filterError.clear();
            }
            if (pwm.enabled())
                pwm.reassertIfDrifted();
            nextMaintenance = steady + std::chrono::seconds(2);
        }
        if (dirty && steady >= nextSave)
            save();
        pollfd fd{listener, POLLIN, 0};
        const int ready = poll(&fd, 1, schedule.transitioning() ? 34 : 250);
        if (ready <= 0)
            continue;
        auto request = ipcAccept(listener);
        if (request.fd < 0)
            continue;
        std::istringstream input(request.command);
        std::string verb;
        input >> verb;
        bool ok = true;
        std::string error;
        std::string extra;
        if ((verb == "status" || verb == "quit" || verb == "on" || verb == "off" ||
             verb == "toggle") &&
            input >> extra) {
            ok = false;
            error = "Unexpected command arguments";
        } else if (verb == "status") {
        } else if (verb == "quit") {
            unlink(resumeMarker.c_str());
            stopping = 1;
        } else if (verb == "on" || verb == "off" || verb == "toggle")
            ok = setOn(verb == "on" || (verb == "toggle" && !config.enabled));
        else if (verb == "set") {
            std::string field;
            double value;
            input >> field;
            if (!readNumber(input, value)) {
                ok = false;
                error = "Expected a finite numeric value";
            } else if (field == "warmth" && value >= 0 && value <= 1) {
                config.intensity = value;
                config.enabled = true;
                activationFade = false;
                schedule.manual(now, value);
                ok = apply(config.brightness);
                changed();
            } else if (field == "brightness" && value >= (config.allowExtremeDim ? 0 : .1) &&
                       value <= 1) {
                config.brightness = value;
                activationFade = false;
                ok = apply(value);
                changed();
            } else if ((field == "extreme" || field == "login" || field == "pwm") &&
                       (value == 0 || value == 1)) {
                if (field == "extreme") {
                    config.allowExtremeDim = value;
                    config.brightness = std::max(config.brightness, value ? 0.0 : .1);
                    ok = apply(config.brightness);
                }
                if (field == "login") {
                    ok = setLaunchAtLogin(value);
                    if (ok)
                        config.launchAtLogin = value;
                    else
                        error = "Could not save launch at login";
                }
                if (field == "pwm") {
                    ok = value ? (available && pwm.enable()) : pwm.disable();
                    if (ok) {
                        config.pwmSafeMode = value;
                        pwmError.clear();
                    } else {
                        error = "Could not verify hardware brightness or restore it";
                        pwmError = error;
                    }
                }
                changed();
            } else {
                ok = false;
                error = "Unknown setting or value outside its allowed range";
            }
        } else if (verb == "preset") {
            std::string name, extra;
            input >> name;
            const int index =
                name == "day" ? 0 : (name == "evening" ? 1 : (name == "night" ? 2 : -1));
            if (index < 0 || input >> extra) {
                ok = false;
                error = "Choose day, evening, or night";
            } else {
                config.intensity = DailySchedule::targets[index];
                config.brightness = std::array<double, 3>{1, .85, .7}[index];
                config.enabled = true;
                activationFade = false;
                schedule.manual(now, config.intensity);
                ok = apply(config.brightness);
                changed();
            }
        } else if (verb == "schedule") {
            std::string command;
            input >> command;
            if (command == "enabled") {
                double value;
                if (!readNumber(input, value) || (value != 0 && value != 1) ||
                    (value == 1 && (preferences.review || !preferences.daily.valid()))) {
                    ok = false;
                    error = "Review schedule: select at least one preset with unique valid times";
                } else if (preferences.enabled != bool(value)) {
                    preferences.enabled = value;
                    if (value)
                        schedule.arm(now, config.enabled ? config.intensity : 0, true);
                    else
                        schedule.disable();
                    changed();
                }
            } else if (command == "save") {
                DailySchedule candidate;
                std::string extra;
                for (int i = 0; i < 3; ++i) {
                    int selected = -1;
                    if (!(input >> candidate.minutes[i] >> selected) ||
                        (selected != 0 && selected != 1))
                        ok = false;
                    candidate.selected[i] = selected == 1;
                }
                if (input >> extra)
                    ok = false;
                if (!candidate.wellFormed() || (preferences.enabled && !candidate.valid()))
                    ok = false;
                if (!ok)
                    error = "Choose valid, unique start times for the selected presets";
                else if (preferences.review || candidate.minutes != preferences.daily.minutes ||
                         candidate.selected != preferences.daily.selected) {
                    preferences.daily = candidate;
                    preferences.review = false;
                    schedule.configure(candidate);
                    if (preferences.enabled)
                        schedule.arm(now, config.enabled ? config.intensity : 0, true);
                    changed();
                }
            } else {
                ok = false;
                error = "Unknown schedule command";
            }
        } else {
            ok = false;
            error = "Unknown command";
        }
        if (dirty && (verb == "quit" || verb == "schedule")) {
            if (!save()) {
                ok = false;
                error = "Could not save settings";
            }
        }
        ipcReply(request, status(ok, error));
    }
    config.enabled = false;
    std::string error;
    if (backend)
        backend->restore(error);
    backend.reset();
    const bool restored = pwm.disable();
    save();
    close(listener);
    unlink(ipcSocketPath().c_str());
    return restored ? 0 : 2;
}
