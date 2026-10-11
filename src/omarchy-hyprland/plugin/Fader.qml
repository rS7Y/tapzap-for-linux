// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root
    property string label: "WARMTH"
    property string readout: "0K"
    property string topLabel: "0K"
    property string bottomLabel: "6500K"
    property string brandFont: "sans-serif"
    property string monoFont: "monospace"
    property color signalColor: "#ff0000"
    property real value: 1
    property real minimum: 0
    property bool filterEnabled: false
    property bool dragging: false
    signal edited(real nextValue)
    color: "#101010"
    radius: 6
    border.color: activeFocus ? "#777777" : "#303030"
    border.width: 1
    activeFocusOnTab: true
    Accessible.role: Accessible.Slider
    Accessible.name: label
    Accessible.description: readout + ". Use up and down arrows; Shift adjusts finely."
    Accessible.onIncreaseAction: root.editValue(root.value + .01)
    Accessible.onDecreaseAction: root.editValue(root.value - .01)
    function editValue(next) {
        value = Math.max(minimum, Math.min(1, next));
        edited(value);
    }
    Keys.onPressed: function (event) {
        var step = event.modifiers & Qt.ShiftModifier ? .001 : .01;
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Right)
            editValue(value + step);
        else if (event.key === Qt.Key_Down || event.key === Qt.Key_Left)
            editValue(value - step);
        else if (event.key === Qt.Key_Home)
            editValue(minimum);
        else if (event.key === Qt.Key_End)
            editValue(1);
        else
            return;
        event.accepted = true;
    }
    Text {
        x: 14
        y: 15
        text: root.label
        font.family: root.brandFont
        font.pixelSize: 12
        font.bold: true
        color: root.signalColor
    }
    Rectangle {
        x: 13
        y: 42
        width: parent.width - 26
        height: 36
        color: "#050505"
        radius: 4
        border.width: 1
        border.color: "#2d2d2d"
        Text {
            anchors.centerIn: parent
            text: root.readout
            color: root.signalColor
            font.family: root.monoFont
            font.pixelSize: 23
        }
    }
    Item {
        id: lane
        x: 12
        y: 96
        width: parent.width - 24
        height: parent.height - 116
        Text {
            y: -7
            anchors.right: parent.right
            text: root.topLabel
            font.family: root.monoFont
            font.pixelSize: 8
            color: "#777777"
        }
        Text {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            text: root.bottomLabel
            font.family: root.monoFont
            font.pixelSize: 8
            color: "#777777"
        }
        Item {
            id: travel
            x: 0
            y: 18
            width: parent.width
            height: parent.height - 36
            Repeater {
                model: 11
                Item {
                    required property int index
                    y: index * travel.height / 10
                    width: travel.width
                    height: 1
                    Rectangle {
                        anchors.right: parent.horizontalCenter
                        anchors.rightMargin: 14
                        width: index % 5 === 0 ? 20 : 10
                        height: 1
                        color: index % 5 === 0 ? "#777777" : "#343434"
                    }
                    Rectangle {
                        anchors.left: parent.horizontalCenter
                        anchors.leftMargin: 14
                        width: index % 5 === 0 ? 20 : 10
                        height: 1
                        color: index % 5 === 0 ? "#777777" : "#343434"
                    }
                }
            }
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: -4
                width: 16
                height: travel.height + 8
                radius: 4
                color: "#050505"
                border.width: 1
                border.color: "#292929"
                Rectangle {
                    x: 4
                    y: 4
                    width: 8
                    height: parent.height - 8
                    color: "#262626"
                    radius: 2
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: parent.height * root.value
                        color: root.filterEnabled ? root.signalColor : "#484848"
                        radius: 2
                    }
                }
            }
            Rectangle {
                id: cap
                anchors.horizontalCenter: parent.horizontalCenter
                y: (1 - root.value) * travel.height - height / 2
                width: 72
                height: 35
                radius: 5
                border.width: 1
                border.color: "#606060"
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#595959"
                    }
                    GradientStop {
                        position: .3
                        color: "#383838"
                    }
                    GradientStop {
                        position: .7
                        color: "#2e2e2e"
                    }
                    GradientStop {
                        position: 1
                        color: "#404040"
                    }
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: 32
                    height: 3
                    radius: 1
                    color: root.signalColor
                    opacity: root.filterEnabled ? 1 : .4
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 9
                    width: 32
                    height: 1
                    color: "#555555"
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 25
                    width: 32
                    height: 1
                    color: "#555555"
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeVerCursor
            property real startY: 0
            property real startValue: 0
            onPressed: function (mouse) {
                root.forceActiveFocus();
                root.dragging = true;
                var center = travel.y + (1 - root.value) * travel.height;
                if (Math.abs(mouse.y - center) > cap.height / 2)
                    root.editValue(1 - (mouse.y - travel.y) / travel.height);
                startY = mouse.y;
                startValue = root.value;
            }
            onPositionChanged: function (mouse) {
                if (!pressed)
                    return;
                var fine = mouse.modifiers & Qt.ShiftModifier ? .15 : 1;
                root.editValue(startValue + (startY - mouse.y) / travel.height * fine);
            }
            onReleased: root.dragging = false
            onCanceled: root.dragging = false
        }
    }
}
