// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Filled vector bolt, independent of font substitutions and emoji rendering.
import QtQuick

Canvas {
    id: root
    implicitWidth: 12
    implicitHeight: 16
    property color color: "white"
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        var scale = Math.min(width / 18, height / 24);
        ctx.translate((width - 18 * scale) / 2, (height - 24 * scale) / 2);
        ctx.scale(scale, scale);
        ctx.beginPath();
        ctx.moveTo(11.8, 0.8);
        ctx.lineTo(1.6, 13.4);
        ctx.lineTo(7.6, 13.4);
        ctx.lineTo(5.5, 23.2);
        ctx.lineTo(16.4, 9.7);
        ctx.lineTo(10.3, 9.7);
        ctx.closePath();
        ctx.fillStyle = root.color;
        ctx.fill();
    }
}
