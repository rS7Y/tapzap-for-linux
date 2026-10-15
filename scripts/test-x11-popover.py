# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Live X11 regression: close to tray, then reopen without losing the GUI.

Run on a disposable X11 desktop with python3-xlib and the native tray helper:
    python3 scripts/test-x11-popover.py src/x11/build/tapzap
This uses private config/runtime directories and starts with the filter OFF.
"""

import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time

from Xlib import X, display, protocol


def main():
    binary = Path(sys.argv[1]).resolve()
    dpy = display.Display()
    root = dpy.screen().root
    with tempfile.TemporaryDirectory(prefix="tapzap-popover-") as tmp:
        env = dict(os.environ, XDG_RUNTIME_DIR=tmp, XDG_CONFIG_HOME=tmp)
        with tempfile.TemporaryFile(mode="w+") as log:
            proc = subprocess.Popen(
                [str(binary), "gui"],
                env=env,
                stdout=log,
                stderr=log,
                start_new_session=True,
            )

            def wait_for(predicate):
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    assert proc.poll() is None, "GUI exited while reopening the popover"
                    if predicate():
                        return
                    time.sleep(0.05)
                raise AssertionError("popover state did not settle within 5 seconds")

            def command(text):
                with socket.socket(socket.AF_UNIX) as connection:
                    connection.connect(str(Path(tmp) / "tapzap.sock"))
                    connection.sendall(text.encode())

            try:
                wait_for(lambda: (Path(tmp) / "tapzap.sock").exists())
                windows = [w for w in root.query_tree().children if w.get_wm_name() == "Tap Zap"]
                assert len(windows) == 1, "run without another TAP ZAP GUI on this display"
                win = windows[0]
                wait_for(lambda: win.get_attributes().map_state == X.IsViewable)
                # Let the tray helper start before asking the GUI to hide to it.
                time.sleep(0.5)
                for _ in range(10):
                    win.send_event(
                        protocol.event.ClientMessage(
                            window=win,
                            client_type=dpy.intern_atom("WM_PROTOCOLS"),
                            data=(
                                32,
                                [dpy.intern_atom("WM_DELETE_WINDOW"), X.CurrentTime, 0, 0, 0],
                            ),
                        )
                    )
                    dpy.sync()
                    wait_for(lambda: win.get_attributes().map_state == X.IsUnmapped)
                    command("show")
                    wait_for(lambda: win.get_attributes().map_state == X.IsViewable)
                    wait_for(lambda: dpy.get_input_focus().focus == win)
                    time.sleep(0.1)
                    assert proc.poll() is None, "GUI exited after mapping the popover"
                command("quit")
                assert proc.wait(timeout=5) == 0
                print("PASS: 10 close-to-tray/reopen cycles; clean Quit")
            except BaseException:
                log.seek(0)
                sys.stderr.write(log.read())
                raise
            finally:
                # Includes a tray child left behind by an Xlib error in a broken build.
                try:
                    os.killpg(proc.pid, signal.SIGTERM)
                except ProcessLookupError:
                    pass
                proc.wait(timeout=5)
    dpy.close()


if __name__ == "__main__":
    main()
