#!/usr/bin/env python3
"""Smoke-test the real GUI and its missing-compositor failure in the guest."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import time
from PIL import ImageGrab


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--parent-display", required=True)
    parser.add_argument("--ui-transport", default="XWayland inside nested patched COSMIC")
    args = parser.parse_args()
    args.binary = args.binary.resolve()
    session = json.loads((args.session / "session-env.json").read_text())
    env = dict(os.environ, **{k: v for k, v in session.items() if v is not None})
    env["XDG_CONFIG_HOME"] = str(args.session / "gui-smoke-config")
    env["TAPZAP_ASSETS"] = str(args.assets.resolve() / "assets")
    env["TAPZAP_DEBUG_RENDER"] = "1"
    output = args.session / "evidence"
    output.mkdir(exist_ok=True)
    results = {"ui_transport": args.ui_transport, "completed": False}
    with (output / "gui.log").open("w") as log:
        process = subprocess.Popen([str(args.binary), "gui", "1", "1", "0"], env=env,
                                   cwd=args.assets, stdout=log, stderr=log)
    try:
        started = time.monotonic()
        deadline = started + 30
        while True:
            assert process.poll() is None, "GUI failed to launch; inspect gui.log"
            try:
                with socket.socket(socket.AF_UNIX) as ipc:
                    ipc.connect(session["XDG_RUNTIME_DIR"] + "/tapzap.sock")
                break
            except OSError:
                assert time.monotonic() < deadline, "GUI control socket did not become ready"
                time.sleep(0.05)
        while "[redraw]" not in (output / "gui.log").read_text():
            assert process.poll() is None, "GUI failed before its first paint; inspect gui.log"
            assert time.monotonic() < deadline, "GUI did not paint before the startup deadline"
            time.sleep(0.05)
        results["startup_seconds"] = round(time.monotonic() - started, 3)
        time.sleep(0.2)
        ImageGrab.grab(xdisplay=args.parent_display).save(output / "gui-off.png")
        for command, picture in [("on", "gui-on-red.png"), ("off", "gui-off-restored.png")]:
            subprocess.run([str(args.binary), command], env=env, check=True)
            deadline = time.monotonic() + 4
            while True:
                screenshot = ImageGrab.grab(xdisplay=args.parent_display).convert("RGB")
                # Exclude the virtual cursor at the edge; measure composed content.
                area = screenshot.crop((48, 48, screenshot.width - 48, screenshot.height - 48))
                channels = [channel[1] for channel in area.getextrema()]
                maxima = channels[1:]
                matched = (channels[0] > 32 and max(maxima) <= 2) if command == "on" else min(maxima) > 32
                if matched or time.monotonic() >= deadline:
                    break
                time.sleep(0.05)
            screenshot.save(output / picture)
            results[command + "_max_red"] = channels[0]
            results[command + "_max_green_blue"] = maxima
            assert matched, f"GUI {command} did not reach its expected rendered colour: {maxima}"
            results[command + "_ipc"] = True
        # The existing CLI exposes on/off/toggle/show; quit is a GUI IPC command.
        with socket.socket(socket.AF_UNIX) as ipc:
            ipc.connect(session["XDG_RUNTIME_DIR"] + "/tapzap.sock")
            ipc.sendall(b"quit")
        assert process.wait(timeout=3) == 0
        results["clean_gui_quit_exit_code"] = 0
        bad_env = dict(env, TAPZAP_COSMIC_SOCKET=str(args.session / "missing.sock"))
        failed = subprocess.run([str(args.binary), "gui"], env=bad_env, cwd=args.assets,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=3)
        assert failed.returncode == 2, failed.stderr
        assert "bridge unavailable" in failed.stderr, failed.stderr
        results["missing_socket_exit_code"] = failed.returncode
        results["missing_socket_message"] = failed.stderr.strip()
        results["completed"] = True
        print(json.dumps(results, indent=2))
    finally:
        if process.poll() is None:
            process.kill()
        process.wait()
        (output / "gui-smoke.json").write_text(json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
