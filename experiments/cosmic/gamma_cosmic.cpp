// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamma_backend.hpp"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace tapzap {
class CosmicRGB final : public IGammaBackend {
  public:
    ~CosmicRGB() override {
        std::string ignored;
        restore(ignored);
    }
    bool init(std::string &err) override {
        if (!connectSocket(err))
            return false;
        disconnect();
        return true;
    }
    bool setFilter(double intensity, double brightness, std::string &err) override {
        if (!std::isfinite(intensity) || !std::isfinite(brightness) || intensity < 0 ||
            intensity > 1 || brightness < 0 || brightness > 1) {
            err = "Warmth and brightness must be finite values within [0, 1]";
            return false;
        }
        if (fd_ < 0 && !connectSocket(err))
            return false;
        const auto colour = normalizedRGB(intensity);
        std::array<double, 3> rgb{colour.r * brightness, colour.g * brightness,
                                  colour.b * brightness};
        std::ostringstream command;
        command << std::setprecision(9) << "SET " << rgb[0] << ' ' << rgb[1] << ' ' << rgb[2]
                << '\n';
        if (!exchange(command.str(), rgb, err))
            return false;
        rgb_ = rgb;
        return true;
    }
    bool maintainFilter(double, double, std::string &err) override {
        return exchange("PING\n", rgb_, err);
    }
    bool restore(std::string &err) override {
        if (fd_ < 0)
            return true;
        const bool ok = exchange("OFF\n", {1, 1, 1}, err);
        disconnect();
        return ok;
    }
    std::string name() const override {
        return "COSMIC RGB compositor prototype";
    }
    // A single global composed-desktop transform, not a physical monitor count.
    int outputCount() const override {
        return 1;
    }

  private:
    int fd_ = -1;
    std::array<double, 3> rgb_{1, 1, 1};

    void disconnect() {
        if (fd_ >= 0)
            ::close(fd_);
        fd_ = -1;
    }

    bool fail(std::string &err, const std::string &message) {
        err = message;
        disconnect();
        return false;
    }

    bool readLine(std::string &line, std::string &err) {
        line.clear();
        for (int i = 0; i < 128; ++i) {
            char byte;
            if (::recv(fd_, &byte, 1, 0) != 1)
                return fail(err, "Patched COSMIC disconnected or its reply timed out");
            if (byte == '\n')
                return true;
            line += byte;
        }
        return fail(err, "Invalid COSMIC reply length");
    }

    bool connectSocket(std::string &err) {
        std::string path;
        if (const char *overridePath = std::getenv("TAPZAP_COSMIC_SOCKET")) {
            path = overridePath;
        } else {
            const char *runtime = std::getenv("XDG_RUNTIME_DIR");
            const char *display = std::getenv("WAYLAND_DISPLAY");
            if (!runtime || !display)
                return fail(err, "Select the patched COSMIC session first");
            path = std::string(runtime) + "/tapzap-cosmic-" + display + ".sock";
        }
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path.size() >= sizeof(address.sun_path))
            return fail(err, "COSMIC socket path is too long");
        struct stat metadata{};
        if (::lstat(path.c_str(), &metadata) != 0 || !S_ISSOCK(metadata.st_mode) ||
            metadata.st_uid != ::geteuid() || (metadata.st_mode & 077) != 0)
            return fail(err,
                        "Private patched-COSMIC socket is unavailable or has unsafe permissions");
        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0)
            return fail(err, "Cannot allocate COSMIC control socket");
        ::fcntl(fd_, F_SETFD, FD_CLOEXEC);
        timeval timeout{1, 0};
        // The compositor can pause during cold shader/XWayland startup. Give
        // only its initial greeting more time; active commands stay at 1 s.
        timeval startupTimeout{3, 0};
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &startupTimeout, sizeof(startupTimeout));
        ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#ifdef SO_NOSIGPIPE
        int enabled = 1;
        ::setsockopt(fd_, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
        if (::connect(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0)
            return fail(err, "Cannot connect to the patched COSMIC compositor");
        std::string greeting;
        if (!readLine(greeting, err))
            return false;
        if (greeting != "TAPZAP-COSMIC 1")
            return fail(err, "COSMIC RGB channel is busy or incompatible");
        ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        return true;
    }

    bool exchange(const std::string &command, const std::array<double, 3> &expected,
                  std::string &err) {
        if (fd_ < 0)
            return fail(err, "COSMIC RGB connection has expired");
#ifdef MSG_NOSIGNAL
        constexpr int flags = MSG_NOSIGNAL;
#else
        constexpr int flags = 0;
#endif
        size_t sent = 0;
        while (sent < command.size()) {
            auto count = ::send(fd_, command.data() + sent, command.size() - sent, flags);
            if (count <= 0)
                return fail(err, "Cannot send COSMIC RGB command");
            sent += static_cast<size_t>(count);
        }
        std::string reply;
        if (!readLine(reply, err))
            return false;
        std::istringstream input(reply);
        std::string tag, extra;
        std::array<double, 3> actual{};
        if (!(input >> tag >> actual[0] >> actual[1] >> actual[2]) || tag != "OK" ||
            (input >> extra))
            return fail(err, "Invalid COSMIC acknowledgement");
        for (int channel = 0; channel < 3; ++channel)
            if (!std::isfinite(actual[channel]) ||
                std::fabs(actual[channel] - expected[channel]) > 1e-5)
                return fail(err, "COSMIC acknowledged different RGB gains");
        return true;
    }
};
std::unique_ptr<IGammaBackend> makeDefaultBackend() {
    return std::make_unique<CosmicRGB>();
}
} // namespace tapzap
