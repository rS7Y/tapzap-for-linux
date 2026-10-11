// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root
    property int minutes: 420
    property string phaseName: "DAY"
    property string fontFamily: "monospace"
    signal edited(int minutes)
    signal done
    width: 288
    height: 212
    color: "#202020"
    radius: 4
    border.color: "#666666"
    onMinutesChanged: edited(minutes)

    function adjust(part, delta) {
        var hour = Math.floor(minutes / 60), minute = minutes % 60;
        if (part === "hour")
            hour = (hour + delta + 24) % 24;
        else
            minute = (minute + delta + 60) % 60;
        minutes = hour * 60 + minute;
    }
    function commit(part, text) {
        if (!/^\d{1,2}$/.test(text))
            return false;
        var value = Number(text), maximum = part === "hour" ? 23 : 59;
        if (value > maximum)
            return false;
        minutes = part === "hour" ? value * 60 + minutes % 60 : Math.floor(minutes / 60) * 60 + value;
        return true;
    }

    // Consume empty-card clicks so they cannot dismiss the editor underneath.
    MouseArea {
        anchors.fill: parent
    }
    Text {
        x: 14
        y: 16
        text: root.phaseName + " · START AT"
        color: "#eeeeee"
        font.family: root.fontFamily
        font.pixelSize: 11
        font.bold: true
    }
    TapButton {
        x: 218
        y: 6
        width: 60
        height: 34
        text: "DONE"
        textColor: "#ff5555"
        fontFamily: root.fontFamily
        textSize: 11
        quiet: true
        onClicked: root.done()
    }
    component NumberField: Item {
        id: field
        property string part
        property int value
        width: 92
        height: 144
        function resetText() {
            input.text = Qt.binding(function () {
                return String(field.value).padStart(2, "0");
            });
        }
        TapButton {
            width: 92
            height: 36
            text: "+"
            textSize: 22
            fontFamily: root.fontFamily
            Accessible.name: "Increase " + field.part
            onClicked: root.adjust(field.part, 1)
        }
        Rectangle {
            y: 42
            width: 92
            height: 60
            color: "#080808"
            radius: 4
            border.color: input.activeFocus ? "#eeeeee" : "#444444"
            TextInput {
                id: input
                anchors.centerIn: parent
                width: 80
                text: String(field.value).padStart(2, "0")
                font.family: root.fontFamily
                font.pixelSize: 32
                color: "#ffffff"
                horizontalAlignment: Text.AlignHCenter
                maximumLength: 2
                selectByMouse: true
                activeFocusOnTab: true
                validator: IntValidator {
                    bottom: 0
                    top: field.part === "hour" ? 23 : 59
                }
                Accessible.name: root.phaseName + " " + field.part
                onActiveFocusChanged: if (activeFocus)
                    selectAll()
                onEditingFinished: {
                    root.commit(field.part, text);
                    field.resetText();
                }
                Keys.onUpPressed: {
                    root.adjust(field.part, 1);
                    field.resetText();
                }
                Keys.onDownPressed: {
                    root.adjust(field.part, -1);
                    field.resetText();
                }
                Keys.onReturnPressed: {
                    root.commit(field.part, text);
                    root.done();
                }
            }
        }
        TapButton {
            y: 108
            width: 92
            height: 36
            text: "−"
            textSize: 22
            fontFamily: root.fontFamily
            Accessible.name: "Decrease " + field.part
            onClicked: root.adjust(field.part, -1)
        }
    }
    NumberField {
        x: 34
        y: 52
        part: "hour"
        value: Math.floor(root.minutes / 60)
    }
    Text {
        x: 134
        y: 102
        width: 20
        text: ":"
        color: "#ffffff"
        font.family: root.fontFamily
        font.pixelSize: 28
        horizontalAlignment: Text.AlignHCenter
    }
    NumberField {
        x: 162
        y: 52
        part: "minute"
        value: root.minutes % 60
    }
}
