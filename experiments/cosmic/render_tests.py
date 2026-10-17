#!/usr/bin/env python3
"""Measure the parent X server after COSMIC postprocessing, inside the VM."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import signal
import socket
import subprocess
import time
from PIL import ImageGrab


def wait_for(predicate, seconds=5):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.05)
    raise AssertionError("Timed out waiting for rendered state")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session", type=Path, required=True)
    parser.add_argument("--parent-display", required=True)
    parser.add_argument("--binaries", type=Path, required=True)
    parser.add_argument("--environment", default="Linux VM; nested COSMIC winit on Xvfb/llvmpipe",
                        help="Describe the actual guest and rendering backend in the saved report")
    parser.add_argument("--capture-description", default="Parent X server after compositor postprocessing")
    parser.add_argument("--inverted", action="store_true",
                        help="Exercise COSMIC accessibility inversion through its native protocol")
    args = parser.parse_args()
    args.binaries = args.binaries.resolve()
    output = args.session / "evidence"
    output.mkdir(exist_ok=True)
    session = json.loads((args.session / "session-env.json").read_text())
    env = dict(os.environ, **{k: v for k, v in session.items() if v is not None})
    path = Path(session["XDG_RUNTIME_DIR"]) / f"tapzap-cosmic-{session['WAYLAND_DISPLAY']}.sock"
    env["TAPZAP_COSMIC_SOCKET"] = str(path)
    env["XDG_CONFIG_HOME"] = str(args.session / "tapzap-config")
    children = []
    results = []
    completed = False

    def capture():
        return ImageGrab.grab(xdisplay=args.parent_display).convert("RGB")

    def start_driver():
        proc = subprocess.Popen([str(args.binaries / "tapzap-cosmic-driver")], env=env,
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=(output / f"driver-{len(children)}.log").open("w"), text=True)
        children.append(proc)
        assert reply(proc) == "READY", "Bridge did not initialize"
        return proc

    def reply(proc):
        assert select.select([proc.stdout], [], [], 3)[0], "No bridge reply"
        return proc.stdout.readline().strip()

    def command(proc, text):
        proc.stdin.write(text + "\n"); proc.stdin.flush()
        assert reply(proc) == "OK", text

    def gains(intensity, brightness):
        text = subprocess.check_output([str(args.binaries / "tapzap-cosmic"), "dump-ramp",
                                        str(intensity), str(brightness), "2"], text=True)
        top = re.findall(r"[RGB]=(\d+)", text.strip().splitlines()[-1])
        assert len(top) == 3, "Unexpected TAP ZAP ramp output"
        return [int(v) / 65535 for v in top]

    points = []
    source_colours = [(255, 255, 255), (128, 128, 128), (255, 0, 0),
                      (0, 255, 0), (0, 0, 255), (0, 0, 0)]
    if args.inverted:
        source_colours = [(0, 0, 0), (127, 127, 127), (0, 255, 255),
                          (255, 0, 255), (255, 255, 0), (255, 255, 255)]

    def locate_swatches():
        image = capture()
        y = image.height // 2
        x = 0
        found = []
        for colour in source_colours:
            runs = []
            while x < image.width:
                start = x
                while x < image.width and image.getpixel((x, y)) == colour:
                    x += 1
                if x - start > 30:
                    runs.append((start, x))
                    break
                x = max(x, start + 1)
            if not runs:
                return False
            start, end = runs[0]
            found.append(((start + end) // 2, y))
        points[:] = found
        image.save(output / "neutral.png")
        return True

    def measure(name, rgb, seconds=4, colours=None):
        expected = [[round(c * gain) for c, gain in zip(colour, rgb)] for colour in (colours or source_colours)]
        last = None
        def matched():
            nonlocal last
            image = capture()
            observed = [list(image.getpixel(point)) for point in points]
            error = max(abs(a - b) for actual, want in zip(observed, expected) for a, b in zip(actual, want))
            last = (image, observed, error)
            return error <= 2
        try:
            wait_for(matched, seconds)
        finally:
            if last:
                image, observed, error = last
                image.save(output / f"{name}.png")
                results.append({"case": name, "rgb": rgb, "expected": expected,
                                "observed": observed, "max_channel_error": error, "passed": error <= 2})
                print(json.dumps(results[-1]), flush=True)
        assert last[2] <= 2, name

    try:
        if args.inverted:
            saved_filter = args.session / "state/cosmic-comp/a11y_screen_filter.ron"
            initial_filter_bytes = saved_filter.read_bytes()
        swatches = subprocess.Popen([str(args.binaries / "swatches")], env=env,
                                   stdout=(output / "swatches.log").open("w"), stderr=subprocess.STDOUT)
        children.append(swatches)
        wait_for(locate_swatches)
        driver = start_driver()
        command(driver, "SET 0.3846153846153846 1")
        measure("warm-4000K", gains(0.3846153846153846, 1))
        command(driver, "SET 1 1")
        measure("full-red", [1, 0, 0])
        # PING must keep a static colour alive for longer than the lease.
        time.sleep(2.4)
        measure("heartbeat-keeps-red", [1, 0, 0])
        if args.inverted:
            subprocess.run([str(args.binaries / "a11y-filter"), "0", "0"], env=env, check=True, timeout=10)
            measure("a11y-change-retains-red", [1, 0, 0], colours=[
                (255, 255, 255), (128, 128, 128), (255, 0, 0),
                (0, 255, 0), (0, 0, 255), (0, 0, 0)])
            subprocess.run([str(args.binaries / "a11y-filter"), "1", "0"], env=env, check=True, timeout=10)
            measure("a11y-inverted-again-retains-red", [1, 0, 0])
        with socket.socket(socket.AF_UNIX) as other:
            other.settimeout(2); other.connect(str(path))
            assert other.recv(128) == b"BUSY\n", "Second owner was not rejected"
        results.append({"case": "second-owner-rejected", "passed": True})
        command(driver, "SET 0 0.5")
        measure("dim-50-percent", [0.5, 0.5, 0.5])
        command(driver, "OFF")
        measure("off-restores", [1, 1, 1])
        command(driver, "SET 1 1")
        driver.stdin.write("QUIT\n"); driver.stdin.flush()
        assert driver.wait(timeout=3) == 0
        measure("clean-close-restores", [1, 1, 1])
        driver = start_driver()
        command(driver, "SET 1 1"); measure("before-kill", [1, 0, 0])
        driver.kill(); driver.wait()
        measure("kill-restores", [1, 1, 1])
        driver = start_driver()
        command(driver, "SET 1 1"); measure("before-stop", [1, 0, 0])
        driver.send_signal(signal.SIGSTOP)
        started = time.monotonic()
        measure("heartbeat-expiry-restores", [1, 1, 1])
        results[-1]["elapsed_seconds"] = round(time.monotonic() - started, 3)
        driver.kill(); driver.wait()
        for name, invalid in [("invalid-value-restores", b"SET NaN 1 1\n"),
                              ("oversized-input-restores", b"x" * 1025)]:
            with socket.socket(socket.AF_UNIX) as client:
                client.settimeout(2); client.connect(str(path))
                assert client.recv(128) == b"TAPZAP-COSMIC 1\n"
                client.sendall(b"SET 1 0 0\n"); assert client.recv(128).startswith(b"OK ")
                measure("before-" + name, [1, 0, 0])
                client.sendall(invalid)
                measure(name, [1, 1, 1])
        if args.inverted:
            # Accessibility toggles can reformat the small RON fixture. Compare
            # its state rather than whitespace/trailing commas; RGB is skipped.
            normalize = lambda value: re.sub(rb"[\s,]", b"", value)
            assert normalize(saved_filter.read_bytes()) == normalize(initial_filter_bytes), "Runtime RGB changed saved accessibility settings"
        completed = True
    finally:
        for proc in children:
            if proc.poll() is None:
                proc.kill()
            proc.wait()
        report = {"environment": args.environment,
                  "capture": args.capture_description,
                  "sample_points": points, "tolerance_per_channel": 2, "results": results,
                  "completed": completed,
                  "physical_hardware_validated": False}
        report["accessibility_inverted"] = args.inverted
        if args.inverted and completed:
            report["saved_accessibility_state_unchanged"] = True
        (output / "results.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
