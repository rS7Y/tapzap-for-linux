// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Classic TAP ZAP layout with native Omarchy integration and shared schedules.
import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Hyprland
import qs.Ui as OmarchyUi
import qs.Commons

OmarchyUi.Panel {
    id: root
    moduleName: "app.tapzap"
    ipcTarget: "app.tapzap"
    readonly property string launcher: Quickshell.env("HOME") + "/.local/bin/tapzap-omarchy"
    property var snapshot: ({
            enabled: false,
            intensity: 1,
            brightness: .7,
            extreme: false,
            login: false,
            pwmAvailable: false,
            pwmRequested: false,
            pwmVerified: false,
            hardwarePercent: -1,
            error: "",
            schedule: {
                enabled: false,
                review: false,
                minutes: [420, 1080, 1320],
                selected: [true, true, true],
                next: 0
            }
        })
    property var queue: []
    property string currentKey: ""
    property string notice: ""
    property bool connected: false
    property int view: 0
    property int editingPhase: -1
    property var draftMinutes: [420, 1080, 1320]
    property var draftSelected: [true, true, true]
    implicitWidth: barButton.implicitWidth
    implicitHeight: barButton.implicitHeight
    FontLoader {
        id: anybody
        source: Qt.resolvedUrl("assets/Anybody-ExtraBold.ttf")
    }
    FontLoader {
        id: anybodyBold
        source: Qt.resolvedUrl("assets/Anybody-Bold.ttf")
    }
    readonly property string mono: Style.font.family

    function applyState(text, actionReply) {
        try {
            var value = JSON.parse(text);
            if (!value.engine)
                return;
            snapshot = value;
            connected = true;
            if (!value.ok)
                notice = value.error || "Could not apply this change.";
            else if (actionReply)
                notice = currentKey === "schedule-save" ? "Times saved" : "";
            if (!warmth.dragging && !hasPending("warmth"))
                warmth.value = value.intensity;
            if (!brightness.dragging && !hasPending("brightness"))
                brightness.value = value.brightness;
        } catch (_) {
            connected = false;
        }
    }
    function hasPending(key) {
        if (currentKey === key)
            return true;
        for (var i = 0; i < queue.length; i++)
            if (queue[i].key === key)
                return true;
        return false;
    }
    function send(args, key) {
        var next = queue.slice();
        // Coalesce only adjacent drag samples; preserve ordering around presets,
        // OFF and settings changes. Never spawn a process for every pointer event.
        if (key && next.length && next[next.length - 1].key === key)
            next[next.length - 1] = {
                args: args,
                key: key
            };
        else
            next.push({
                args: args,
                key: key || ""
            });
        queue = next;
        flush.start();
    }
    function drain() {
        if (command.running || !queue.length)
            return;
        var next = queue.slice(), item = next.shift();
        queue = next;
        currentKey = item.key;
        command.command = [launcher].concat(item.args);
        command.running = true;
    }
    function refresh() {
        if (!probe.running && !command.running && !queue.length)
            probe.running = true;
    }
    function settingsPage() {
        draftMinutes = snapshot.schedule.minutes.slice();
        draftSelected = snapshot.schedule.selected.slice();
        editingPhase = -1;
        notice = "";
        view = 1;
    }
    function editTime(index) {
        editingPhase = index;
        timeEditor.minutes = draftMinutes[index];
    }
    function formatTime(minutes) {
        return String(Math.floor(minutes / 60)).padStart(2, "0") + ":" + String(minutes % 60).padStart(2, "0");
    }
    function scheduleSummary() {
        var s = snapshot.schedule;
        if (s.review)
            return "REVIEW TIMES";
        if (!s.enabled)
            return "OFF";
        if (!s.next)
            return "DAILY TIMES";
        var when = new Date(s.next * 1000), hh = String(when.getHours()).padStart(2, "0"), mm = String(when.getMinutes()).padStart(2, "0");
        return (s.holding ? "MANUAL UNTIL " : s.waiting ? "NEXT " : "NEXT ") + hh + ":" + mm;
    }
    function saveTimes() {
        editingPhase = -1;
        var values = draftMinutes.slice(), chosen = draftSelected.slice(), times = [];
        for (var i = 0; i < 3; i++) {
            if (chosen[i]) {
                if (times.indexOf(values[i]) >= 0) {
                    notice = "Choose a different start time for each preset.";
                    return;
                }
                times.push(values[i]);
            }
        }
        if (!times.length) {
            notice = "Choose at least one preset to schedule.";
            return;
        }
        var args = ["schedule", "save"];
        for (i = 0; i < 3; i++)
            args.push(String(values[i]), chosen[i] ? "1" : "0");
        draftMinutes = values;
        draftSelected = chosen;
        send(args, "schedule-save");
    }
    onOpenedChanged: {
        if (opened) {
            view = 0;
            editingPhase = -1;
            starter.running = true;
            retry.start();
            refresh();
            warmth.forceActiveFocus();
        } else {
            if (bar && bar.activePopout === root)
                bar.releasePopout(root);
        }
        if (opened && bar)
            bar.requestPopout(root);
    }
    Connections {
        target: root.bar
        ignoreUnknownSignals: true
        function onActivePopoutChanged() {
            if (root.opened && root.bar.activePopout !== root)
                root.close();
        }
    }
    Process {
        id: starter
        command: ["omarchy-shell", "app.tapzap.engine", "start"]
    }
    Timer {
        id: retry
        interval: 150
        repeat: true
        property int attempts: 0
        onRunningChanged: if (running)
            attempts = 0
        onTriggered: {
            root.refresh();
            if (root.connected || ++attempts >= 20)
                stop();
        }
    }
    Timer {
        id: flush
        interval: 35
        onTriggered: root.drain()
    }
    Timer {
        interval: root.opened ? 600 : 3000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }
    Process {
        id: probe
        command: [root.launcher, "status"]
        stdout: StdioCollector {
            onStreamFinished: root.applyState(text, false)
        }
        onExited: function (code, status) {
            if (code !== 0)
                root.connected = false;
        }
    }
    Process {
        id: command
        stdout: StdioCollector {
            onStreamFinished: root.applyState(text, true)
        }
        onExited: function (code, status) {
            root.currentKey = "";
            if (code !== 0 && !root.notice)
                root.notice = "COULD NOT APPLY CHANGE";
            Qt.callLater(root.drain);
        }
    }
    OmarchyUi.BarIconButton {
        id: barButton
        anchors.fill: parent
        bar: root.bar
        text: " "
        fontFamily: anybody.name
        fontSize: 21
        iconComponent: Component {
            Item {
                Bolt {
                    anchors.centerIn: parent
                    width: 12
                    height: 16
                    color: root.barForeground
                    opacity: root.snapshot.enabled ? 1 : .45
                }
            }
        }
        foreground: root.barForeground
        dimmed: !root.snapshot.enabled
        tooltipText: root.snapshot.enabled ? "TAP ZAP · ON" : "TAP ZAP · OFF"
        onPressed: function (button) {
            root.toggle();
        }
    }
    PopupWindow {
        id: popup
        visible: root.opened
        color: "transparent"
        surfaceFormat.opaque: false
        readonly property real scale: Math.max(.45, Math.min(.8 * Style.font.baseSize / 12, screen ? (screen.height - 70) / 540 : 1, screen ? (screen.width - 24) / 320 : 1))
        implicitWidth: Math.ceil(320 * scale)
        implicitHeight: Math.ceil(540 * scale)
        anchor.window: barButton.QsWindow.window
        anchor.adjustment: PopupAdjustment.Slide
        anchor.edges: Edges.Top | Edges.Left
        anchor.gravity: Edges.Bottom | Edges.Right
        anchor.rect.width: 1
        anchor.rect.height: 1
        anchor.onAnchoring: {
            var window = barButton.QsWindow.window;
            if (!window)
                return;
            var p = window.contentItem.mapFromItem(barButton, barButton.width / 2 - popup.implicitWidth / 2, barButton.height + 12);
            if (root.bar && root.bar.position === "bottom")
                p.y = -popup.implicitHeight - 12;
            if (root.bar && root.bar.position === "left") {
                p.x = barButton.width + 12;
                p.y = 0;
            }
            if (root.bar && root.bar.position === "right") {
                p.x = -popup.implicitWidth - 12;
                p.y = 0;
            }
            if (!root.bar || root.bar.position === "top" || root.bar.position === "bottom")
                p.x = Math.max(12, Math.min(p.x, window.width - popup.implicitWidth - 12));
            anchor.rect.x = Math.round(p.x);
            anchor.rect.y = Math.round(p.y);
        }
        HyprlandFocusGrab {
            active: root.opened
            windows: barButton.QsWindow.window ? [popup, barButton.QsWindow.window] : [popup]
            onCleared: root.close()
        }
        Rectangle {
            id: body
            width: 320
            height: 540
            scale: popup.scale
            transformOrigin: Item.TopLeft
            radius: 0
            color: "#0d0d0d"
            border.width: 0
            border.color: "#484848"
            focus: true
            Keys.onEscapePressed: {
                if (root.editingPhase >= 0)
                    root.editingPhase = -1;
                else if (root.view !== 0) {
                    root.view = root.view === 2 ? 1 : 0;
                    warmth.forceActiveFocus();
                } else
                    root.close();
            }
            Rectangle {
                width: 320
                height: 56
                color: "#1a1a1a"
            }
            Rectangle {
                visible: root.view === 0
                y: 356
                width: 320
                height: 184
                color: "#1a1a1a"
            }
            Rectangle {
                visible: root.view === 0
                x: 159
                y: 110
                width: 2
                height: 190
                color: "#000000"
            }
            Text {
                x: 24
                y: 21
                text: root.view === 0 ? "TAP ZAP" : root.view === 1 ? "SETTINGS" : "AUTO SCHEDULE"
                font.family: anybody.name
                font.pixelSize: 11
                font.letterSpacing: 1
                color: "#686868"
            }
            TapButton {
                x: 252
                y: 6
                width: 52
                height: 44
                quiet: true
                text: root.view === 0 ? "⚙︎" : "BACK"
                textSize: root.view === 0 ? 26 : 12
                Accessible.name: root.view === 0 ? "Settings" : "Back"
                fontFamily: root.mono
                onClicked: {
                    if (root.view === 0)
                        root.settingsPage();
                    else {
                        root.editingPhase = -1;
                        root.view = root.view === 2 ? 1 : 0;
                    }
                }
            }
            Item {
                visible: root.view === 0
                x: 4
                y: 70
                width: 312
                height: 465
                Row {
                    spacing: 0
                    Fader {
                        id: warmth
                        width: 156
                        height: 270
                        brandFont: anybody.name
                        monoFont: root.mono
                        label: "WARMTH"
                        readout: Math.round(6500 * (1 - value)).toLocaleString(Qt.locale("en_US"), "f", 0) + "K"
                        value: 1
                        signalColor: "#ff0000"
                        filterEnabled: root.snapshot.enabled
                        onEdited: function (v) {
                            root.send(["set", "warmth", String(v)], "warmth");
                        }
                    }
                    Fader {
                        id: brightness
                        width: 156
                        height: 270
                        brandFont: anybody.name
                        monoFont: root.mono
                        label: "BRIGHTNESS"
                        readout: Math.round(value * 100) + "%"
                        topLabel: "100%"
                        bottomLabel: root.snapshot.extreme ? "0%" : "10%"
                        minimum: root.snapshot.extreme ? 0 : .1
                        value: .7
                        signalColor: "#eeeeee"
                        filterEnabled: root.snapshot.enabled
                        onEdited: function (v) {
                            root.send(["set", "brightness", String(v)], "brightness");
                        }
                    }
                }
                Rectangle {
                    id: pwmRow
                    x: 20
                    y: 293
                    width: 272
                    height: 34
                    readonly property bool toggleAvailable: root.connected && root.snapshot.pwmAvailable && !root.snapshot.error
                    readonly property bool showMessage: !toggleAvailable || !!root.snapshot.pwmError
                    readonly property color indicatorColor: root.snapshot.pwmVerified ? "#ffffff" : root.snapshot.pwmRequested ? "#ffcc00" : "#ffffff"
                    color: "#0e0e0e"
                    border.width: 1
                    border.color: root.snapshot.error || root.snapshot.pwmError ? "#ff5555" : root.snapshot.pwmRequested ? Qt.alpha(indicatorColor, .4) : "#2a2a2a"
                    Rectangle {
                        visible: !pwmRow.showMessage
                        x: 10
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 4
                        color: pwmRow.indicatorColor
                        opacity: root.snapshot.pwmRequested ? 1 : .25
                    }
                    Text {
                        visible: !pwmRow.showMessage
                        x: 26
                        anchors.verticalCenter: parent.verticalCenter
                        text: "PWM-SAFE MODE"
                        color: root.snapshot.pwmRequested ? "#f2f2f2" : "#808080"
                        font.family: root.mono
                        font.pixelSize: 10
                        font.bold: true
                        font.letterSpacing: .8
                    }
                    Text {
                        visible: !pwmRow.showMessage
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.snapshot.pwmVerified ? "ON" : root.snapshot.pwmRequested ? "…" : "OFF"
                        color: pwmRow.indicatorColor
                        opacity: root.snapshot.pwmRequested ? 1 : .4
                        font.family: root.mono
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Text {
                        visible: pwmRow.showMessage
                        anchors.centerIn: parent
                        width: 252
                        text: root.snapshot.error || (!root.connected ? "CONNECTING TO TAP ZAP…" : root.snapshot.pwmError ? root.snapshot.pwmError : root.snapshot.hardwarePercent >= 0 ? "DISPLAY BRIGHTNESS " + root.snapshot.hardwarePercent + "%" : "CHECK DISPLAY BRIGHTNESS")
                        color: root.snapshot.error || root.snapshot.pwmError ? "#ff5555" : "#aaaaaa"
                        font.family: root.mono
                        font.pixelSize: 10
                        wrapMode: Text.Wrap
                        textFormat: Text.PlainText
                        horizontalAlignment: Text.AlignHCenter
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: pwmRow.toggleAvailable
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.send(["set", "pwm", root.snapshot.pwmRequested ? "0" : "1"])
                    }
                    activeFocusOnTab: toggleAvailable
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: "PWM-Safe Mode"
                    Accessible.checked: root.snapshot.pwmRequested
                    Keys.onSpacePressed: if (toggleAvailable)
                        root.send(["set", "pwm", root.snapshot.pwmRequested ? "0" : "1"])
                }
                Row {
                    x: 20
                    y: 350
                    spacing: 12
                    Repeater {
                        model: ["DAY", "EVENING", "NIGHT"]
                        TapButton {
                            required property int index
                            required property string modelData
                            width: 82.66
                            height: 36
                            text: modelData
                            fontFamily: anybodyBold.name
                            selected: Math.abs(root.snapshot.intensity - [1 - 4000 / 6500, 1 - 2700 / 6500, 1][index]) < .02
                            onClicked: root.send(["preset", ["day", "evening", "night"][index]])
                        }
                    }
                }
                TapButton {
                    x: 20
                    y: 406
                    width: 272
                    height: 48
                    bolt: true
                    text: "ZAP"
                    textColor: root.snapshot.enabled ? "#ffffff" : "#8e8e93"
                    fontFamily: anybody.name
                    textSize: 14
                    danger: root.snapshot.enabled
                    onClicked: root.send(["toggle"])
                }
            }
            Item {
                visible: root.view === 1
                x: 16
                y: 76
                width: 288
                height: 450
                Column {
                    spacing: 12
                    TapButton {
                        width: 288
                        height: 44
                        text: "AUTO SCHEDULE   " + root.scheduleSummary()
                        fontFamily: root.mono
                        onClicked: root.view = 2
                    }
                    TapButton {
                        width: 288
                        height: 44
                        text: "LAUNCH AT LOGIN   " + (root.snapshot.login ? "ON" : "OFF")
                        fontFamily: root.mono
                        onClicked: root.send(["set", "login", root.snapshot.login ? "0" : "1"])
                    }
                    TapButton {
                        width: 288
                        height: 44
                        text: "ALLOW EXTREME DIM   " + (root.snapshot.extreme ? "ON" : "OFF")
                        fontFamily: root.mono
                        onClicked: root.send(["set", "extreme", root.snapshot.extreme ? "0" : "1"])
                    }
                    TapButton {
                        width: 288
                        height: 44
                        text: "REPORT A BUG / GET SUPPORT"
                        fontFamily: root.mono
                        onClicked: {
                            Quickshell.execDetached(["xdg-open", "https://github.com/rS7Y/tapzap-for-linux/issues/new/choose"]);
                            root.close();
                        }
                    }
                    TapButton {
                        width: 288
                        height: 44
                        text: "EMERGENCY STOP (QUIT)"
                        danger: true
                        fontFamily: root.mono
                        onClicked: {
                            root.send(["quit"]);
                            root.close();
                        }
                    }
                }
                Text {
                    y: 300
                    width: 288
                    text: root.notice || root.snapshot.error
                    font.family: root.mono
                    font.pixelSize: 10
                    color: "#ff5555"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                Text {
                    y: 388
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Made by Rusty"
                    font.family: root.mono
                    font.pixelSize: 13
                    color: "#dddddd"
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Quickshell.execDetached(["xdg-open", "https://x.com/ZE_RUSTY"])
                    }
                }
                Text {
                    y: 416
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "tapzap.app"
                    font.family: root.mono
                    font.pixelSize: 10
                    color: "#777777"
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: Quickshell.execDetached(["xdg-open", "https://tapzap.app"])
                    }
                }
            }
            Item {
                visible: root.view === 2
                x: 16
                y: 76
                width: 288
                height: 450
                TapButton {
                    width: 288
                    height: 40
                    text: "AUTO SCHEDULE   " + (root.snapshot.schedule.enabled ? "ON" : "OFF")
                    fontFamily: root.mono
                    selected: root.snapshot.schedule.enabled
                    onClicked: root.send(["schedule", "enabled", root.snapshot.schedule.enabled ? "0" : "1"])
                }
                Column {
                    y: 62
                    spacing: 12
                    Repeater {
                        id: timeRows
                        model: 3
                        Item {
                            id: timeRow
                            required property int index
                            width: 288
                            height: 64
                            property bool chosen: root.draftSelected[index]
                            TapButton {
                                width: 100
                                height: 54
                                text: ["DAY", "EVENING", "NIGHT"][index] + (timeRow.chosen ? "  ON" : "  OFF")
                                fontFamily: anybodyBold.name
                                textSize: 11
                                selected: timeRow.chosen
                                onClicked: {
                                    var selected = root.draftSelected.slice();
                                    selected[index] = !selected[index];
                                    root.draftSelected = selected;
                                    root.notice = "";
                                }
                            }
                            TapButton {
                                x: 116
                                width: 172
                                height: 54
                                quiet: true
                                color: "#050505"
                                opacity: timeRow.chosen ? 1 : .4
                                enabled: timeRow.chosen
                                border.width: 1
                                border.color: activeFocus ? "#999999" : "#3a3a3a"
                                text: root.formatTime(root.draftMinutes[index])
                                fontFamily: root.mono
                                textSize: 25
                                Accessible.name: ["Day", "Evening", "Night"][index] + " start time " + text
                                onClicked: root.editTime(index)
                            }
                        }
                    }
                }
                TapButton {
                    y: 296
                    width: 288
                    height: 44
                    text: "SAVE TIMES"
                    fontFamily: anybody.name
                    onClicked: root.saveTimes()
                }
                Text {
                    y: 354
                    width: 288
                    text: root.notice || (root.snapshot.schedule.review ? "Review saved times before enabling." : root.snapshot.schedule.enabled ? root.scheduleSummary() : "Schedule off")
                    font.family: root.mono
                    font.pixelSize: 12
                    color: (root.notice && root.notice !== "Times saved") || root.snapshot.schedule.review ? "#ff5555" : "#cccccc"
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                }
                Text {
                    y: 400
                    width: 288
                    text: "Warmth fades over 60 seconds.\nManual changes hold until the next start."
                    font.family: root.mono
                    font.pixelSize: 11
                    color: "#999999"
                    wrapMode: Text.WordWrap
                    textFormat: Text.PlainText
                }
                Rectangle {
                    anchors.fill: parent
                    visible: root.editingPhase >= 0
                    color: "#dd0d0d0d"
                    MouseArea {
                        anchors.fill: parent
                        onClicked: root.editingPhase = -1
                    }
                }
                TimeEditor {
                    id: timeEditor
                    y: 84
                    visible: root.editingPhase >= 0
                    phaseName: ["DAY", "EVENING", "NIGHT"][Math.max(0, root.editingPhase)]
                    fontFamily: root.mono
                    onEdited: function (minutes) {
                        if (root.editingPhase < 0)
                            return;
                        var draft = root.draftMinutes.slice();
                        draft[root.editingPhase] = minutes;
                        root.draftMinutes = draft;
                        root.notice = "";
                    }
                    onDone: root.editingPhase = -1
                }
            }
        }
    }
}
