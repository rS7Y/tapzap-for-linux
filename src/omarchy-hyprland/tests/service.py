#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Headless engine against an isolated CTM compositor; never touches the desktop."""

import json
import os
from pathlib import Path
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def wait(check):
    deadline = time.monotonic() + 7
    while time.monotonic() < deadline:
        try:
            if check():
                return
        except (OSError, ValueError):
            pass
        time.sleep(0.03)
    raise AssertionError("condition timed out")


with tempfile.TemporaryDirectory(prefix="tapzap-service-test-") as temporary:
    tmp = Path(temporary)
    runtime = tmp / "runtime"
    runtime.mkdir(mode=0o700)
    env = dict(
        os.environ,
        XDG_RUNTIME_DIR=str(runtime),
        XDG_CONFIG_HOME=str(tmp / "config"),
        WAYLAND_DISPLAY="tapzap-service-test",
        TAPZAP_HYPRLAND_HELPER=str(ROOT / "build/tapzap-ctm"),
    )
    log = (tmp / "process.log").open("w")
    server = subprocess.Popen(
        [ROOT / "build/test-ctm-server", env["WAYLAND_DISPLAY"], tmp / "matrix.json"],
        env=env,
        stdout=log,
        stderr=log,
    )
    engine = None

    def query(command="status"):
        with socket.socket(socket.AF_UNIX) as client:
            client.settimeout(6)
            client.connect(str(runtime / "tapzap-omarchy/control.sock"))
            client.sendall((command + "\n").encode())
            data = b""
            while not data.endswith(b"\n"):
                chunk = client.recv(4096)
                if not chunk:
                    raise OSError("closed")
                data += chunk
            return json.loads(data)

    def matrix():
        return json.loads((tmp / "matrix.json").read_text())

    try:
        wait(lambda: (runtime / env["WAYLAND_DISPLAY"]).exists())
        engine = subprocess.Popen(
            [ROOT / "build/tapzap", "daemon"], env=env, stdout=log, stderr=log
        )
        wait(lambda: query()["engine"])
        assert not query()["enabled"]
        for invalid in (
            "set warmth nan",
            "set brightness inf",
            "set warmth 1.1",
            "set brightness .09",
            "set login 2",
            "preset bad",
            "schedule save 420 1 420 1 1320 1 extra",
            "on extra",
        ):
            assert not query(invalid)["ok"], invalid
        assert not query()["enabled"]
        assert query("schedule save 420 1 420 1 1320 1")["ok"]  # Disabled draft is allowed.
        assert not query("schedule enabled 1")["ok"]
        assert query("schedule save 420 1 1080 1 1320 1")["ok"]
        assert query("schedule enabled 1")["schedule"]["enabled"]
        assert query("preset night")["enabled"]
        wait(lambda: matrix()["active"])
        # The protocol transports each coefficient as Wayland 24.8 fixed point.
        assert matrix()["matrix"] == [179 / 256, 0, 0, 0, 0, 0, 0, 0, 0]
        held = query("set warmth .6")["schedule"]
        assert held["holding"]
        unchanged = query("schedule save 420 1 1080 1 1320 1")["schedule"]
        assert unchanged["holding"] and unchanged["next"] == held["next"]
        assert query("set brightness .5")["schedule"]["holding"]
        assert not query("off")["enabled"]
        wait(lambda: not matrix()["active"])
        assert query()["schedule"]["waiting"]
        assert query("set extreme 1")["ok"]
        assert query("set brightness 0")["ok"]
        assert query("set extreme 0")["brightness"] == 0.1
        assert query("set login 1")["login"]
        assert query("set login 0")["ok"]
        settings = tmp / "config/TapZapOmarchy/settings.ini"
        wait(lambda: settings.is_file())
        saved = settings.read_bytes()
        settings.unlink()
        settings.mkdir()
        assert query("set brightness .4")["ok"]
        wait(lambda: "Could not save settings" in query()["error"])
        settings.rmdir()
        settings.write_bytes(saved)
        wait(lambda: not query()["error"])
        assert query("preset day")["ok"]
        engine.terminate()
        engine.wait(timeout=7)
        wait(lambda: not matrix()["active"])
        engine = subprocess.Popen(
            [ROOT / "build/tapzap", "daemon"], env=env, stdout=log, stderr=log
        )
        wait(lambda: query()["engine"])
        state = query()
        assert not state["enabled"] and state["schedule"]["enabled"]
        assert state["schedule"]["next"] > time.time()
        assert query("quit")["ok"]
        engine.wait(timeout=7)
        print("PASS: native engine validation, presets, hold, OFF, persistence and SIGTERM restore")
    finally:
        if engine and engine.poll() is None:
            engine.terminate()
            engine.wait(timeout=7)
        server.terminate()
        server.wait(timeout=7)
        log.close()
