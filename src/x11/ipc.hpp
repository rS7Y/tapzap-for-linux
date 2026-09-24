// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Single-instance control IPC over a Unix domain socket — parity with the macOS tapzap:// URL
// scheme (on/off/toggle/show). A running GUI listens; `tapzap on|off|toggle|show` sends a command.
#pragma once
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace tapzap {

inline std::string ipcSocketPath() {
    const char* r = std::getenv("XDG_RUNTIME_DIR");
    const std::string base = (r && *r) ? std::string(r) : std::string("/tmp");
    return base + "/tapzap.sock";
}

// Client: connect to a running instance and send a command. Returns true if delivered.
inline bool ipcSend(const std::string& cmd) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return false;
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    const std::string p = ipcSocketPath();
    std::strncpy(addr.sun_path, p.c_str(), sizeof(addr.sun_path) - 1);
    bool ok = false;
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof addr) == 0)
        ok = ::write(fd, cmd.c_str(), cmd.size()) == static_cast<ssize_t>(cmd.size());
    ::close(fd);
    return ok;
}

// Server: create the listening socket (non-blocking). Returns fd or -1.
inline int ipcListen() {
    const std::string p = ipcSocketPath();
    ::unlink(p.c_str());
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, p.c_str(), sizeof(addr.sun_path) - 1);
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof addr) != 0) { ::close(fd); return -1; }
    if (::listen(fd, 4) != 0) { ::close(fd); return -1; }
    return fd;
}

// Server: accept one connection and read the command string.
inline std::string ipcAccept(int listenfd) {
    const int c = ::accept(listenfd, nullptr, nullptr);
    if (c < 0) return "";
    char buf[64] = {0};
    const ssize_t n = ::read(c, buf, sizeof(buf) - 1);
    ::close(c);
    if (n <= 0) return "";
    std::string s(buf, static_cast<size_t>(n));
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

inline void ipcClose(int listenfd) {
    if (listenfd >= 0) ::close(listenfd);
    ::unlink(ipcSocketPath().c_str());
}

} // namespace tapzap
