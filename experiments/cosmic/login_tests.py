#!/usr/bin/env python3
"""Validate an already autostarted GUI; never launch or retry the application.

The VM-only login wrapper records start-*.json and gui.log under
DIRECTORY/boots/BOOT_ID. See validation/pop-os/REPLAY.md for setup.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import time
from PIL import ImageGrab


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--parent-display", required=True)
    parser.add_argument("--cursor-corner", action="store_true",
                        help="Sample the stationary QEMU cursor at the top-left corner")
    args = parser.parse_args()
    boot = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    output = args.directory / "boots" / boot
    output.mkdir(parents=True, exist_ok=True)
    report = {"boot_id": boot, "completed": False, "app_launched_by_test": False}
    deadline = time.monotonic() + 180

    def wait(predicate):
        while True:
            result = predicate()
            if result:
                return result
            assert time.monotonic() < deadline, "Autostart did not reach the expected state"
            time.sleep(0.1)

    def capture(name, red, alive=True):
        def matched():
            log = (output / "gui.log").read_text()
            assert "Filter error:" not in log and "gamma recovery:" not in log, log
            if alive:
                os.kill(start["pid"], 0)
            image = ImageGrab.grab(xdisplay=args.parent_display).convert("RGB")
            area = image.crop((48, 48, image.width - 48, image.height - 48))
            maxima = [extent[1] for extent in area.getextrema()]
            good = maxima[0] > 32 and max(maxima[1:]) <= 2 if red else min(maxima[1:]) > 32
            if good:
                image.save(output / (name + ".png"))
                report[name] = {"max_rgb": maxima, "passed": True}
            return good
        wait(matched)

    def ipc(command):
        with socket.socket(socket.AF_UNIX) as connection:
            connection.settimeout(2)
            connection.connect(start["environment"]["XDG_RUNTIME_DIR"] + "/tapzap.sock")
            connection.sendall(command.encode())

    try:
        wait(lambda: list(output.glob("start-*.json")))
        starts = list(output.glob("start-*.json"))
        assert len(starts) == 1, "Expected one launch by the desktop autostart service"
        start = json.loads(starts[0].read_text())
        report["start"] = start
        wait(lambda: (output / "gui.log").exists() and "[redraw]" in (output / "gui.log").read_text())
        # No ON command precedes this sample: it proves saved-state autostart.
        capture("autostart-red", True)
        time.sleep(3)
        capture("autostart-heartbeat", True)
        ipc("off")
        capture("off-restored", False)
        if args.cursor_corner:
            # In this headless VM the pointer stays at (0, 0). These white
            # glyph pixels are identified from OFF, independently of ON.
            from PIL import Image
            with Image.open(output / "off-restored.png") as off, Image.open(output / "autostart-red.png") as on:
                points = [(x, y) for y in range(24) for x in range(16)
                          if off.convert("RGB").getpixel((x, y)) == (255, 255, 255)]
                assert len(points) >= 3, "Stationary white cursor glyph was not captured"
                observed = [list(on.convert("RGB").getpixel(point)) for point in points]
                assert all(max(abs(a-b) for a, b in zip(rgb, [178, 0, 0])) <= 2 for rgb in observed), observed
                report["cursor"] = {"sample_points": points, "expected_on": [178, 0, 0],
                                    "observed_on": observed, "passed": True}
        # Leave saved state ON for the next reboot, then verify quit restoration.
        ipc("on")
        capture("on-again", True)
        ipc("quit")
        wait(lambda: not Path(f"/proc/{start['pid']}").exists())
        capture("quit-restored", False, alive=False)
        report["completed"] = True
    finally:
        (output / "login-results.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
