#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Load the hidden native panel with installed Omarchy modules and local assets."""

import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
shell = Path(os.environ.get("OMARCHY_PATH", "/usr/share/omarchy")) / "shell"
if not (shell / "Ui/Panel.qml").is_file():
    print("SKIP: native QML check requires the installed Omarchy shell")
    raise SystemExit(0)

# PanelWindow requires a real window backend. The panel remains closed: this
# check never maps a surface or takes keyboard focus. Isolate its IPC and D-Bus.
wayland = Path(os.environ.get("WAYLAND_DISPLAY", ""))
if not str(os.environ.get("WAYLAND_DISPLAY", "")):
    print("SKIP: native QML check requires WAYLAND_DISPLAY")
    raise SystemExit(0)
if not wayland.is_absolute():
    wayland = Path(os.environ.get("XDG_RUNTIME_DIR", "")) / wayland
if not wayland.is_socket():
    raise SystemExit("Native QML check: Wayland display is unavailable")

with tempfile.TemporaryDirectory(prefix="tapzap-qml-test-") as temporary:
    root = Path(temporary)
    for name in ("Ui", "Commons"):
        (root / name).symlink_to(shell / name)
    shutil.copytree(ROOT / "plugin", root / "Plugin")
    shutil.copytree(ROOT / "src/linux/assets", root / "Plugin/assets")
    runtime = root / "runtime"
    runtime.mkdir(mode=0o700)
    (root / "shell.qml").write_text("""import QtQuick
import Quickshell
import "Plugin" as TapZap
ShellRoot {
    TapZap.Panel { id: panel }
    Timer {
        interval: 100; running: true
        onTriggered: {
            var component=Qt.createComponent("Plugin/Service.qml")
            if(component.status===Component.Error) {console.error(component.errorString()); Qt.exit(1); return}
            var state=panel.snapshot
            state.engine=true; state.ok=true
            panel.notice="INVALID TIMES"
            panel.applyState(JSON.stringify(state),false)
            if(panel.notice!=="INVALID TIMES") {console.error("Status erased validation"); Qt.exit(1); return}
            panel.applyState(JSON.stringify(state),true)
            if(panel.notice!=="") {console.error("Action failed to clear notice"); Qt.exit(1); return}
            var clockComponent=Qt.createComponent("Plugin/TimeEditor.qml")
            if(clockComponent.status!==Component.Ready) {console.error("Clock picker is missing: "+clockComponent.errorString()); Qt.exit(1); return}
            var clock=clockComponent.createObject(null,{minutes:1439})
            clock.adjust("hour",1)
            if(clock.minutes!==59) {console.error("Hour must wrap from 23 to 00"); Qt.exit(1); return}
            clock.adjust("minute",1)
            if(clock.minutes!==0) {console.error("Minute must wrap from 59 to 00"); Qt.exit(1); return}
            clock.adjust("minute",-1)
            if(clock.minutes!==59) {console.error("Minute decrement must wrap"); Qt.exit(1); return}
            clock.minutes=420
            if(clock.commit("hour","24") || clock.minutes!==420) {console.error("Invalid hour changed the draft"); Qt.exit(1); return}
            if(!clock.commit("minute","15") || clock.minutes!==435) {console.error("Typed minute did not enter the draft"); Qt.exit(1); return}
            clock.destroy()
            panel.settingsPage()
            panel.draftMinutes=[435,1080,1320]
            panel.saveTimes()
            if(panel.queue.length!==1 || panel.queue[0].args[2]!=="435") {console.error("Picker draft did not reach Save Times"); Qt.exit(1); return}
            panel.queue=[]
            panel.draftMinutes=[435,435,1320]
            panel.saveTimes()
            if(panel.queue.length || !panel.notice) {console.error("Duplicate selected times were saved"); Qt.exit(1); return}
            panel.draftSelected=[false,false,false]
            panel.saveTimes()
            if(panel.queue.length || !panel.notice) {console.error("An empty schedule was saved"); Qt.exit(1); return}
            console.log("PASS: native QML components, assets and persistent validation notices")
            Qt.quit()
        }
    }
}
""")
    env = dict(
        os.environ,
        QT_QPA_PLATFORM="wayland",
        WAYLAND_DISPLAY=str(wayland),
        XDG_RUNTIME_DIR=str(runtime),
    )
    process = subprocess.Popen(
        ["dbus-run-session", "--", "qs", "-p", str(root), "--no-color"],
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        start_new_session=True,
    )
    try:
        stdout, stderr = process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        stdout, stderr = process.communicate()
        raise AssertionError("Native QML check timed out:\n" + stdout + stderr)
    output = stdout + stderr
    assert process.returncode == 0 and "PASS:" in output, output
    assert "Cannot load font" not in output and "ERROR" not in output, output
    print("PASS: native QML components, assets and persistent validation notices")
