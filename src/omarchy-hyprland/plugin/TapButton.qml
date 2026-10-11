// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

Rectangle {
    id: root
    property string text: ""
    property string fontFamily: "monospace"
    property color textColor: "#dddddd"
    property bool selected: false
    property bool danger: false
    property int textSize: 10
    signal clicked
    radius: 4
    border.width: 1
    border.color: activeFocus ? "#eeeeee" : (selected ? "#777777" : "#3c3c3c")
    color: danger ? "#990000" : selected ? "#363636" : mouse.containsMouse ? "#303030" : "#242424"
    gradient: danger ? zapGradient : null
    Gradient {
        id: zapGradient
        GradientStop {
            position: 0
            color: "#cc0000"
        }
        GradientStop {
            position: 1
            color: "#990000"
        }
    }
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: text
    Accessible.onPressAction: root.clicked()
    Keys.onPressed: function (event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            root.clicked();
            event.accepted = true;
        }
    }
    Text {
        anchors.centerIn: parent
        text: root.text
        font.family: root.fontFamily
        font.pixelSize: root.textSize
        font.bold: true
        color: root.textColor
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus();
            root.clicked();
        }
    }
}
