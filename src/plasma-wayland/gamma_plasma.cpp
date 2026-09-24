// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamma_backend.hpp"
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tapzap {
class PlasmaBackend final : public IGammaBackend {
    int socket_ = -1;
    pid_t child_ = -1;
    int outputs_ = 0;
    bool failed_ = false;
    bool receive(std::string& err) {
        std::string response;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (response.size() < 4096) {
            auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            if (left <= 0) break;
            pollfd fd{socket_, POLLIN, 0};
            int ready = poll(&fd, 1, static_cast<int>(left));
            if (ready < 0 && errno == EINTR) continue;
            if (ready <= 0) break;
            char c;
            if (recv(socket_, &c, 1, 0) != 1) break;
            if (c == '\n') {
                if (response == "OK" || response.rfind("OK ", 0) == 0) {
                    if (response.size() > 3) outputs_ = std::atoi(response.c_str() + 3);
                    return true;
                }
                err = response;
                return false;
            }
            response += c;
        }
        failed_ = true;
        err = "Plasma color helper disconnected or timed out";
        // EOF causes the helper/independent guard to restore the original profiles.
        if (socket_ >= 0) { close(socket_); socket_ = -1; }
        return false;
    }
    bool request(const std::string& line, std::string& err) {
        if (failed_ || socket_ < 0) { err = "Restart TAP ZAP to reconnect its Plasma color helper"; return false; }
        const auto message = line + "\n";
        size_t sent = 0;
        while (sent < message.size()) {
            auto n = send(socket_, message.data() + sent, message.size() - sent, MSG_NOSIGNAL);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { failed_ = true; err = "Plasma color helper is unavailable"; return false; }
            sent += n;
        }
        return receive(err);
    }
public:
    ~PlasmaBackend() override {
        std::string err;
        if (socket_ >= 0) { request("quit", err); close(socket_); socket_ = -1; }
        if (child_ > 0) {
            for (int i = 0; i < 30; ++i) {
                if (waitpid(child_, nullptr, WNOHANG) != 0) return;
                usleep(100000);
            }
            kill(child_, SIGTERM); // Independent guard still owns the restoration journal.
            waitpid(child_, nullptr, 0);
        }
    }
    bool init(std::string& err) override {
        const char* script = std::getenv("TAPZAP_PLASMA_HELPER");
        if (!script || script[0] != '/' || access(script, R_OK)) { err = "Plasma helper is missing"; return false; }
        int pair[2];
        if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair)) { err = std::strerror(errno); return false; }
        child_ = fork();
        if (child_ == 0) {
            dup2(pair[1], STDIN_FILENO); dup2(pair[1], STDOUT_FILENO);
            close(pair[0]); close(pair[1]);
            unsetenv("LD_LIBRARY_PATH");
            execl("/usr/bin/python3", "python3", "-u", script, static_cast<char*>(nullptr));
            _exit(127);
        }
        close(pair[1]);
        if (child_ < 0) { close(pair[0]); err = "Cannot start Plasma helper"; return false; }
        socket_ = pair[0];
        return receive(err);
    }
    bool setFilter(double intensity, double brightness, std::string& err) override {
        const auto f = normalizedRGB(intensity);
        const double b = clampd(brightness, 0, 1);
        char requestLine[160];
        std::snprintf(requestLine, sizeof(requestLine), "set %.17g %.17g %.17g", f.r*b, f.g*b, f.b*b);
        return request(requestLine, err);
    }
    bool restore(std::string& err) override { return request("off", err); }
    bool maintainFilter(double, double, std::string& err) override { return request("check", err); }
    std::string name() const override { return "plasma-wayland-icc-proof"; }
    int outputCount() const override { return outputs_; }
};
std::unique_ptr<IGammaBackend> makePlasmaBackend() { return std::make_unique<PlasmaBackend>(); }
}
