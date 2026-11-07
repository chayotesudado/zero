import QtQuick

// The bar widget's mark, three stacked checks, for the sidebar header.
Canvas {
    id: mark
    property color color: theme.foreground
    implicitWidth: 18
    implicitHeight: 18
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        var s = width / 64
        ctx.reset()
        ctx.strokeStyle = color
        ctx.lineWidth = 8.5 * s
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        var rows = [17, 34, 51]
        for (var i = 0; i < rows.length; i++) {
            var y = rows[i] * s
            ctx.beginPath()
            ctx.moveTo(6 * s, y - 2 * s)
            ctx.lineTo(19 * s, y + 7 * s)
            ctx.lineTo(58 * s, y - 10 * s)
            ctx.stroke()
        }
    }
}
