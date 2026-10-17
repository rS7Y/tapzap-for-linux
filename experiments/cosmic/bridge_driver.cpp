// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamma_backend.hpp"
#include <cstdio>
#include <iostream>
#include <string>
#include <sys/select.h>

int main() {
    auto backend = tapzap::makeDefaultBackend();
    std::string error;
    if (!backend->init(error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cout << "READY" << std::endl;
    bool active = false;
    double intensity = 0, brightness = 1;
    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        timeval timeout{0, 500000};
        int ready = select(1, &fds, nullptr, nullptr, &timeout);
        if (ready < 0)
            break;
        if (ready) {
            std::string line;
            if (!std::getline(std::cin, line) || line == "QUIT")
                break;
            bool ok = false;
            if (line == "OFF") {
                ok = backend->restore(error);
                active = false;
            } else if (std::sscanf(line.c_str(), "SET %lf %lf", &intensity, &brightness) == 2) {
                ok = backend->setFilter(intensity, brightness, error);
                active = ok;
            }
            std::cout << (ok ? "OK" : "ERROR " + error) << std::endl;
        } else if (active && !backend->maintainFilter(intensity, brightness, error)) {
            std::cout << "ERROR " << error << std::endl;
            return 2;
        }
    }
    return 0;
}
