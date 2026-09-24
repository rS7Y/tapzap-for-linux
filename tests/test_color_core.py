# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[1]
VARIANTS = (
    ("src/x11", "tapzap_core.hpp"),
    ("src/gnome-wayland", "tapzap_core.hpp"),
    ("src/plasma-wayland", "tapzap_core.hpp"),
    ("src/sway", "tapzap_core.hpp"),
    ("src/omarchy-hyprland", "src/linux/tapzap_core.hpp"),
)

CORE_CONTRACT = r"""
#include "tapzap_core.hpp"
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iostream>

int main() {
    using tapzap::GammaRamp;
    using tapzap::RGB;
    using tapzap::buildRamp;
    using tapzap::normalizedRGB;

    const RGB neutral = normalizedRGB(0.0);
    const RGB red = normalizedRGB(1.0);
    if (std::abs(neutral.r - 1.0) > 1e-12 ||
        std::abs(neutral.g - 1.0) > 1e-12 ||
        std::abs(neutral.b - 1.0) > 1e-12) return 1;
    if (std::abs(red.r - 1.0) > 1e-12 || red.g != 0.0 || red.b != 0.0) return 2;

    if (std::abs(normalizedRGB(-1.0).b - neutral.b) > 1e-12 ||
        std::abs(normalizedRGB(2.0).g - red.g) > 1e-12) return 3;

    double previousBlue = 1.0;
    for (int step = 0; step <= 1000; ++step) {
        const RGB value = normalizedRGB(step / 1000.0);
        if (value.r < 0.0 || value.r > 1.0 ||
            value.g < 0.0 || value.g > 1.0 ||
            value.b < 0.0 || value.b > 1.0) return 4;
        if (value.b > previousBlue + 1e-12) return 5;
        previousBlue = value.b;
    }

    const GammaRamp identity = buildRamp(0.0, 1.0, 17);
    const GammaRamp dimmed = buildRamp(0.0, 0.5, 17);
    const GammaRamp allRed = buildRamp(1.0, 1.0, 17);
    if (identity.size() != 17 || identity.r.front() != 0 ||
        identity.g.front() != 0 || identity.b.front() != 0 ||
        identity.r.back() != 65535 || identity.g.back() != 65535 ||
        identity.b.back() != 65535) return 6;
    if (dimmed.r.back() != 32768 || dimmed.g.back() != 32768 ||
        dimmed.b.back() != 32768) return 7;
    if (allRed.r.back() != 65535 || allRed.g.back() != 0 ||
        allRed.b.back() != 0) return 8;
    for (const GammaRamp* ramp : {&identity, &dimmed, &allRed}) {
        for (std::size_t i = 0; i < ramp->r.size(); ++i) {
            if (ramp->r[i] > ramp->r.back() ||
                ramp->g[i] > ramp->g.back() ||
                ramp->b[i] > ramp->b.back()) return 9;
        }
    }
    return 0;
}
"""


class LinuxColorCoreTests(unittest.TestCase):
    def test_each_variant_exposes_expected_linux_core_contract(self):
        compiler = shutil.which("c++") or shutil.which("clang++")
        self.assertIsNotNone(compiler, "a C++17 compiler is required for source tests")

        for variant, header in VARIANTS:
            with self.subTest(variant=variant):
                source_root = REPO / variant
                core_header = source_root / header
                self.assertTrue(core_header.is_file(), f"missing {core_header.relative_to(REPO)}")

                with tempfile.TemporaryDirectory(prefix="tapzap-core-test-") as temp_dir:
                    temp = Path(temp_dir)
                    fixture = temp / "core_contract.cpp"
                    executable = temp / "core_contract"
                    fixture.write_text(CORE_CONTRACT, encoding="utf-8")
                    subprocess.run(
                        [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                         "-I", str(core_header.parent), str(fixture), "-o", str(executable)],
                        check=True,
                        capture_output=True,
                        text=True,
                    )
                    subprocess.run([str(executable)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
