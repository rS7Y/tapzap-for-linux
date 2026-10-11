// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// TAP ZAP mark from the Mac active/off PDF artwork, with its 24x24 view box.
import QtQuick

Canvas {
    id: root
    implicitWidth: 24
    implicitHeight: 24
    property color color: "white"
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        var scale = Math.min(width, height) / 24;
        ctx.translate((width - 24 * scale) / 2, (height - 24 * scale) / 2);
        ctx.scale(scale, scale);
        ctx.beginPath();
        ctx.moveTo(0, 21);
        ctx.lineTo(14.210938, 3.128906);
        ctx.lineTo(13.199219, 8.230469);
        ctx.lineTo(24, 1);
        ctx.lineTo(12.765625, 15.984375);
        ctx.lineTo(13.199219, 10.988281);
        ctx.closePath();
        ctx.fillStyle = root.color;
        ctx.fill();
    }
}
