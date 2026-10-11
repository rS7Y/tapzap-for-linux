// Copyright (C) 2026 Tap Zap.
// SPDX-License-Identifier: GPL-3.0-or-later
// Monochrome settings outline, independent of emoji and font substitutions.
import QtQuick

Canvas {
    id: root
    implicitWidth: 24
    implicitHeight: 24
    property color color: "#dddddd"
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();
        var scale = Math.min(width, height) / 24;
        ctx.translate(width / 2, height / 2);
        ctx.scale(scale, scale);
        ctx.beginPath();
        for (var i = 0; i < 32; i++) {
            var angle = (i + .5) * Math.PI / 16;
            var radius = i % 4 === 1 || i % 4 === 2 ? 10 : 8;
            var x = Math.cos(angle) * radius, y = Math.sin(angle) * radius;
            if (i === 0)
                ctx.moveTo(x, y);
            else
                ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.moveTo(3.4, 0);
        ctx.arc(0, 0, 3.4, 0, Math.PI * 2);
        ctx.strokeStyle = root.color;
        ctx.lineWidth = 1.6;
        ctx.lineJoin = "round";
        ctx.stroke();
    }
}
