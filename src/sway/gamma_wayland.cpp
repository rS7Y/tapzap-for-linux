// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamma_backend.hpp"
#include "build/wlr-gamma-control-client.h"
#include <wayland-client.h>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <map>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

namespace tapzap {
class WaylandGamma final : public IGammaBackend {
    struct Output {
        WaylandGamma* owner;
        wl_output* output = nullptr;
        zwlr_gamma_control_v1* control = nullptr;
        uint32_t size = 0;
        bool failed = false;
    };
    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    zwlr_gamma_control_manager_v1* manager_ = nullptr;
    std::map<uint32_t, std::unique_ptr<Output>> outputs_;
    bool active_ = false, changed_ = false;

    static void geometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t) {}
    static void mode(void* data, wl_output*, uint32_t flags, int32_t, int32_t, int32_t) {
        if (flags & WL_OUTPUT_MODE_CURRENT) static_cast<Output*>(data)->owner->changed_ = true;
    }
    static void done(void*, wl_output*) {}
    static void scale(void*, wl_output*, int32_t) {}
    static void name(void*, wl_output*, const char*) {}
    static void description(void*, wl_output*, const char*) {}
    static constexpr wl_output_listener outputListener_{geometry, mode, done, scale, name, description};
    static void global(void* data, wl_registry* registry, uint32_t id, const char* interface, uint32_t version) {
        auto& self = *static_cast<WaylandGamma*>(data);
        if (std::strcmp(interface, zwlr_gamma_control_manager_v1_interface.name) == 0 && !self.manager_) {
            self.manager_ = static_cast<zwlr_gamma_control_manager_v1*>(wl_registry_bind(
                registry, id, &zwlr_gamma_control_manager_v1_interface, 1));
        } else if (std::strcmp(interface, wl_output_interface.name) == 0) {
            auto o = std::make_unique<Output>();
            o->owner = &self;
            o->output = static_cast<wl_output*>(wl_registry_bind(registry, id, &wl_output_interface, std::min(version, 4u)));
            wl_output_add_listener(o->output, &outputListener_, o.get());
            self.outputs_.emplace(id, std::move(o));
            self.changed_ = true;
        }
    }
    static void removed(void* data, wl_registry*, uint32_t id) {
        auto& self = *static_cast<WaylandGamma*>(data);
        auto it = self.outputs_.find(id);
        if (it == self.outputs_.end()) return;
        if (it->second->control) zwlr_gamma_control_v1_destroy(it->second->control);
        wl_output_destroy(it->second->output);
        self.outputs_.erase(it);
        self.changed_ = true;
    }
    static constexpr wl_registry_listener registryListener_{global, removed};
    static void gammaSize(void* data, zwlr_gamma_control_v1*, uint32_t size) {
        auto& o = *static_cast<Output*>(data);
        o.size = size;
        if (!size || size > 1048576) o.failed = true;
    }
    static void failed(void* data, zwlr_gamma_control_v1* control) {
        auto& o = *static_cast<Output*>(data);
        o.failed = true;
        o.control = nullptr;
        zwlr_gamma_control_v1_destroy(control);
    }
    static constexpr zwlr_gamma_control_v1_listener gammaListener_{gammaSize, failed};

    bool sync(std::string& err) {
        if (!display_) { err = "Wayland display is disconnected"; return false; }
        bool complete = false;
        auto cb = wl_display_sync(display_);
        static const wl_callback_listener listener = {
            [](void* data, wl_callback*, uint32_t) { *static_cast<bool*>(data) = true; }
        };
        wl_callback_add_listener(cb, &listener, &complete);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        bool ok = true;
        while (!complete) {
            if (wl_display_dispatch_pending(display_) < 0) { ok = false; break; }
            if (complete) break;
            if (wl_display_prepare_read(display_) != 0) continue;
            const int flushed = wl_display_flush(display_);
            if (flushed < 0 && errno != EAGAIN) {
                wl_display_cancel_read(display_); ok = false; break;
            }
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) { wl_display_cancel_read(display_); ok = false; break; }
            pollfd fd{wl_display_get_fd(display_), static_cast<short>(POLLIN | (flushed < 0 ? POLLOUT : 0)), 0};
            const int ready = poll(&fd, 1, static_cast<int>(left));
            if (ready < 0 && errno == EINTR) { wl_display_cancel_read(display_); continue; }
            if (ready <= 0 || fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                wl_display_cancel_read(display_); ok = false; break;
            }
            if (fd.revents & POLLIN) {
                if (wl_display_read_events(display_) < 0) { ok = false; break; }
            } else wl_display_cancel_read(display_);
        }
        wl_callback_destroy(cb);
        if (!ok) err = "Wayland compositor disconnected or did not respond";
        return ok;
    }
    void releaseControls() {
        for (auto& [id, o] : outputs_) {
            if (o->control) zwlr_gamma_control_v1_destroy(o->control);
            o->control = nullptr; o->size = 0; o->failed = false;
        }
        active_ = false;
        if (display_) wl_display_flush(display_);
    }
    bool reject(std::string& err, const char* reason) {
        err = reason;
        releaseControls();
        std::string ignored;
        sync(ignored);
        return false;
    }
public:
    ~WaylandGamma() override {
        std::string ignored;
        if (display_) restore(ignored);
        for (auto& [id, o] : outputs_) wl_output_destroy(o->output);
        if (manager_) zwlr_gamma_control_manager_v1_destroy(manager_);
        if (registry_) wl_registry_destroy(registry_);
        if (display_) wl_display_disconnect(display_);
    }
    bool init(std::string& err) override {
        display_ = wl_display_connect(nullptr);
        if (!display_) { err = "Cannot connect to the Wayland desktop"; return false; }
        registry_ = wl_display_get_registry(display_);
        wl_registry_add_listener(registry_, &registryListener_, this);
        if (!sync(err) || !sync(err)) return false;
        if (!manager_) { err = "This desktop does not expose wlr gamma control"; return false; }
        if (outputs_.empty()) { err = "No active Wayland outputs"; return false; }
        changed_ = false;
        return true;
    }
    bool setFilter(double intensity, double brightness, std::string& err) override {
        if (!sync(err)) return false;
        if (outputs_.empty()) { err = "No active Wayland outputs"; return false; }
        for (auto& [id, o] : outputs_) {
            if (o->failed) return reject(err, "Gamma control was rejected or transferred to another client");
            if (!o->control) {
                o->control = zwlr_gamma_control_manager_v1_get_gamma_control(manager_, o->output);
                zwlr_gamma_control_v1_add_listener(o->control, &gammaListener_, o.get());
            }
        }
        if (!sync(err)) { releaseControls(); return false; }
        for (auto& [id, o] : outputs_) {
            if (o->failed || !o->control || !o->size)
                return reject(err, "Display gamma control is unavailable or owned by another app");
            auto ramp = buildRamp(intensity, brightness, o->size);
            std::vector<uint16_t> values;
            values.reserve(o->size * 3);
            values.insert(values.end(), ramp.r.begin(), ramp.r.end());
            values.insert(values.end(), ramp.g.begin(), ramp.g.end());
            values.insert(values.end(), ramp.b.begin(), ramp.b.end());
            int fd = memfd_create("tapzap-gamma", MFD_CLOEXEC);
            if (fd < 0) return reject(err, "Cannot allocate gamma table");
            const auto* bytes = reinterpret_cast<const char*>(values.data());
            size_t sent = 0, size = values.size() * sizeof(uint16_t);
            while (sent < size) {
                ssize_t n = write(fd, bytes + sent, size - sent);
                if (n < 0 && errno == EINTR) continue;
                if (n <= 0) break;
                sent += n;
            }
            if (sent != size || lseek(fd, 0, SEEK_SET) < 0) {
                close(fd); return reject(err, "Cannot fill gamma table");
            }
            zwlr_gamma_control_v1_set_gamma(o->control, fd);
            close(fd);
        }
        active_ = true;
        changed_ = false;
        if (!sync(err)) { releaseControls(); return false; }
        for (auto& [id, o] : outputs_)
            if (o->failed) return reject(err, "The compositor could not apply the requested gamma table");
        return true;
    }
    bool restore(std::string& err) override {
        releaseControls();
        return sync(err);
    }
    bool refresh(std::string& err) override { changed_ = true; return sync(err); }
    bool maintainFilter(double intensity, double brightness, std::string& err) override {
        if (!sync(err)) return false;
        for (auto& [id, o] : outputs_)
            if (o->failed) return reject(err, "Wayland gamma control was lost");
        if (active_ && changed_) return setFilter(intensity, brightness, err);
        return true;
    }
    std::string name() const override { return "wayland-wlr-gamma"; }
    int outputCount() const override { return static_cast<int>(outputs_.size()); }
};
std::unique_ptr<IGammaBackend> makeWaylandGammaBackend() { return std::make_unique<WaylandGamma>(); }
}
