#!/usr/bin/env python3
"""Start an isolated nested COSMIC session inside the test guest."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import time

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compositor", type=Path, required=True)
    parser.add_argument("--directory", type=Path, required=True)
    parser.add_argument("--display", default=":91")
    parser.add_argument("--inverted", action="store_true",
                        help="Seed isolated COSMIC accessibility inversion for startup testing")
    args = parser.parse_args()
    if not re.fullmatch(r":[0-9]{1,4}", args.display):
        parser.error("Use a local X display such as :91")
    args.directory = args.directory.resolve()
    args.directory.mkdir(mode=0o700, parents=True, exist_ok=False)
    runtime = args.directory / "runtime"
    runtime.mkdir(mode=0o700)
    if args.inverted:
        state = args.directory / "state" / "cosmic-comp"
        state.mkdir(parents=True)
        (state / "a11y_screen_filter.ron").write_text("(inverted: true)\n")
    env = dict(os.environ, DISPLAY=args.display, XDG_RUNTIME_DIR=str(runtime),
               XDG_CONFIG_HOME=str(args.directory / "config"),
               XDG_STATE_HOME=str(args.directory / "state"), LIBGL_ALWAYS_SOFTWARE="1",
               COSMIC_BACKEND="winit", RUST_LOG="warn", TAPZAP_COSMIC_ENABLE="1")
    with (args.directory / "xvfb.log").open("w") as log:
        xserver = subprocess.Popen(["Xvfb", args.display, "-screen", "0", "1280x800x24",
                                    "-nolisten", "tcp"], stdout=log, stderr=log, start_new_session=True)
    time.sleep(0.5)
    if xserver.poll() is not None:
        raise RuntimeError("Xvfb failed; inspect xvfb.log (the display may already be in use)")
    with (args.directory / "cosmic.log").open("w") as log:
        compositor = subprocess.Popen(["dbus-run-session", "--", str(args.compositor.resolve()),
                                       "python3", str(HERE / "session_child.py"),
                                       str(args.directory / "session-env.json")], env=env,
                                      stdout=log, stderr=log, start_new_session=True)
    (args.directory / "pids.json").write_text(json.dumps({"xvfb": xserver.pid, "cosmic": compositor.pid}))
    for _ in range(100):
        if (args.directory / "session-env.json").exists():
            print((args.directory / "session-env.json").read_text())
            return
        if compositor.poll() is not None:
            break
        time.sleep(0.1)
    raise RuntimeError("COSMIC did not become ready; inspect cosmic.log")


if __name__ == "__main__":
    main()
