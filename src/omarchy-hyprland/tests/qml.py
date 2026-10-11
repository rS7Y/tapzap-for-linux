#!/usr/bin/env python3
# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Load the native panel offscreen with installed Omarchy modules and local assets."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
shell = Path(os.environ.get("OMARCHY_PATH", "/usr/share/omarchy")) / "shell"
if not (shell / "Ui/Panel.qml").is_file():
    print("SKIP: native QML check requires the installed Omarchy shell")
    raise SystemExit(0)

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
            console.log("PASS: native QML components, assets and persistent validation notices")
            Qt.quit()
        }
    }
}
""")
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", XDG_RUNTIME_DIR=str(runtime))
    result = subprocess.run(
        ["dbus-run-session", "--", "qs", "-p", str(root), "--no-color"],
        env=env,
        capture_output=True,
        text=True,
        timeout=10,
    )
    output = result.stdout + result.stderr
    assert result.returncode == 0 and "PASS:" in output, output
    assert "Cannot load font" not in output and "ERROR" not in output, output
    print("PASS: native QML components, assets and persistent validation notices")
