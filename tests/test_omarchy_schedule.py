# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Deterministic local-calendar and manual-override tests; no display access."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OmarchyScheduleTests(unittest.TestCase):
    def test_daily_schedule_and_recovery(self):
        compiler = shutil.which("c++") or shutil.which("g++")
        self.assertIsNotNone(compiler, "A C++17 compiler is required")
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / "schedule-test"
            build = subprocess.run(
                [
                    compiler,
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    str(ROOT / "tests/native/omarchy_schedule.cpp"),
                    "-o",
                    str(output),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([output], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PASS", result.stdout)
