#!/usr/bin/env python3
"""Exercise the real C++ backend against the real Rust controller."""
import argparse
import os
from pathlib import Path
import select
import signal
import subprocess
import tempfile
import time
import unittest

HERE = Path(__file__).resolve().parent
CONFIG = None


def wait_for(predicate, seconds=3):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("Timed out waiting for controller state")


class BridgeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix="tz-cosmic-build-")
        cls.server = Path(cls.build.name) / "server"
        cls.driver = Path(cls.build.name) / "driver"
        subprocess.run([CONFIG.rustc, "--edition=2021", str(HERE / "control_server.rs"),
                        "-o", str(cls.server)], check=True)
        subprocess.run([CONFIG.cxx, "-std=c++17", "-Wall", "-Wextra", "-I", str(CONFIG.include),
                        str(HERE / "bridge_driver.cpp"), str(HERE / "gamma_cosmic.cpp"),
                        "-o", str(cls.driver)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tz-rgb-", dir="/tmp")
        self.path = Path(self.temp.name)
        self.sock = self.path / "control.sock"
        self.state = self.path / "rgb"
        self.render_pause = self.path / "render-paused"
        self.server_proc = subprocess.Popen([str(self.server), str(self.sock), str(self.state),
                                             str(self.render_pause)])
        wait_for(self.state.exists)
        env = dict(os.environ, TAPZAP_COSMIC_SOCKET=str(self.sock))
        self.client = subprocess.Popen([str(self.driver)], env=env, stdin=subprocess.PIPE,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

    def tearDown(self):
        for proc in (self.client, self.server_proc):
            if proc.poll() is None:
                proc.kill()
            proc.wait()
        self.client.stdin.close()
        self.client.stdout.close()
        self.client.stderr.close()
        self.temp.cleanup()

    def reply(self):
        self.assertTrue(select.select([self.client.stdout], [], [], 3)[0], "No bridge reply")
        return self.client.stdout.readline().strip()

    def command(self, text):
        self.client.stdin.write(text + "\n")
        self.client.stdin.flush()
        self.assertEqual(self.reply(), "OK")

    def assert_rgb(self, expected):
        def equal():
            try:
                values = [float(v) for v in self.state.read_text().split()]
                return len(values) == 3 and all(abs(a - b) < 1e-5 for a, b in zip(values, expected))
            except (ValueError, FileNotFoundError):
                return False
        wait_for(equal)

    def test_colour_off_and_clean_close(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 0.5")
        self.assert_rgb([0.5, 0, 0])
        self.command("OFF")
        self.assert_rgb([1, 1, 1])
        self.command("SET 0 0.5")
        self.assert_rgb([0.5, 0.5, 0.5])
        self.client.stdin.write("QUIT\n"); self.client.stdin.flush()
        self.assertEqual(self.client.wait(timeout=3), 0)
        self.assert_rgb([1, 1, 1])

    def test_killed_client_restores(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 1")
        self.assert_rgb([1, 0, 0])
        self.client.kill(); self.client.wait()
        self.assert_rgb([1, 1, 1])

    def test_stopped_client_expires(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 1")
        self.assert_rgb([1, 0, 0])
        self.client.send_signal(signal.SIGSTOP)
        self.assert_rgb([1, 1, 1])

    def test_startup_accepts_slow_compositor_greeting(self):
        # Model the cold-start compositor stall observed in the Pop!_OS VM.
        self.client.kill(); self.client.wait()
        for stream in (self.client.stdin, self.client.stdout, self.client.stderr):
            stream.close()
        self.server_proc.send_signal(signal.SIGSTOP)
        self.client = subprocess.Popen([str(self.driver)],
            env=dict(os.environ, TAPZAP_COSMIC_SOCKET=str(self.sock)),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        time.sleep(1.25)
        self.server_proc.send_signal(signal.SIGCONT)
        self.assertEqual(self.reply(), "READY")

    def test_command_timeout_remains_short_after_greeting(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 1")
        self.server_proc.send_signal(signal.SIGSTOP)
        started = time.monotonic()
        self.client.stdin.write("SET 0 1\n"); self.client.stdin.flush()
        self.assertTrue(self.reply().startswith("ERROR "))
        self.assertLess(time.monotonic() - started, 2.5)
        self.server_proc.send_signal(signal.SIGCONT)
        self.assert_rgb([1, 1, 1])

    def test_render_stall_does_not_lose_filter_or_heartbeat(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 1")
        self.assert_rgb([1, 0, 0])
        self.render_pause.touch()
        time.sleep(0.05)
        # This SET and >2 s of idle maintenance must survive a blocked renderer.
        self.command("SET 0 0.5")
        time.sleep(2.3)
        self.assertIsNone(self.client.poll(), "Heartbeat failed while renderer was busy")
        self.render_pause.unlink()
        self.assert_rgb([0.5, 0.5, 0.5])

    def test_off_during_render_stall_cancels_pending_filter(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 0 0.5")
        self.assert_rgb([0.5, 0.5, 0.5])
        self.render_pause.touch()
        time.sleep(0.05)
        self.command("SET 1 1")
        self.command("OFF")
        self.render_pause.unlink()
        self.assert_rgb([1, 1, 1])

    def test_expired_owner_cannot_reapply_after_render_stall(self):
        self.assertEqual(self.reply(), "READY")
        self.command("SET 1 1")
        self.assert_rgb([1, 0, 0])
        self.render_pause.touch()
        self.client.send_signal(signal.SIGSTOP)
        time.sleep(2.3)
        self.render_pause.unlink()
        self.assert_rgb([1, 1, 1])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--include", type=Path, required=True)
    parser.add_argument("--rustc", default="rustc")
    parser.add_argument("--cxx", default="c++")
    CONFIG, rest = parser.parse_known_args()
    unittest.main(argv=[__file__] + rest)
