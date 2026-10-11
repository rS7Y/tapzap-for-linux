// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Quickshell
import Quickshell.Io

Item {
    id: root
    property var shell: null
    readonly property string launcher: Quickshell.env("HOME") + "/.local/bin/tapzap-omarchy"

    // This one shell service owns the engine across all monitors. Quickshell
    // kills its direct child on unload; the launcher's lifetime guardian gives
    // the engine SIGTERM for cleanup. A session marker resumes an engine that
    // was already running, while optional login startup stays optional.
    Process {
        id: engine
        command: [root.launcher, "engine-resume"]
        running: true
        stdout: StdioCollector {}
        stderr: StdioCollector {}
    }
    IpcHandler {
        target: "app.tapzap.engine"
        function start(): void {
        if (!engine.running) {
            engine.command = [root.launcher, "engine"];
            engine.running = true;
        }
        }
            function stop(): void {
                                 Quickshell.execDetached([root.launcher, "quit"]);
                             }
        }
    }
