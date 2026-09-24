// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Minimal binary PPM (P6) I/O — zero dependencies. Used for engine self-rendering and
// for faithful "what the screen looks like" simulation. Real formats (PNG/BMP) are bridged
// at dev time by ImageMagick/PIL; the engine itself stays dependency-free.
#pragma once
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace tapzap {

struct Image { int w = 0, h = 0; std::vector<uint8_t> px; }; // interleaved RGB8

inline bool readPPM(const std::string& path, Image& img) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char magic[3] = {0};
    if (std::fscanf(f, "%2s", magic) != 1 || std::string(magic) != "P6") { std::fclose(f); return false; }
    auto skipws = [&]() {
        int c;
        while ((c = std::fgetc(f)) != EOF) {
            if (c == '#') { while ((c = std::fgetc(f)) != EOF && c != '\n') {} }
            else if (!std::isspace(c)) { std::ungetc(c, f); break; }
        }
    };
    int w = 0, h = 0, maxv = 0;
    skipws(); if (std::fscanf(f, "%d", &w) != 1) { std::fclose(f); return false; }
    skipws(); if (std::fscanf(f, "%d", &h) != 1) { std::fclose(f); return false; }
    skipws(); if (std::fscanf(f, "%d", &maxv) != 1) { std::fclose(f); return false; }
    std::fgetc(f); // single whitespace separator before binary data
    if (w <= 0 || h <= 0 || maxv != 255) { std::fclose(f); return false; }
    img.w = w; img.h = h;
    img.px.resize(static_cast<size_t>(w) * h * 3);
    const size_t got = std::fread(img.px.data(), 1, img.px.size(), f);
    std::fclose(f);
    return got == img.px.size();
}

inline bool writePPM(const std::string& path, const Image& img) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", img.w, img.h);
    std::fwrite(img.px.data(), 1, img.px.size(), f);
    std::fclose(f);
    return true;
}

} // namespace tapzap
