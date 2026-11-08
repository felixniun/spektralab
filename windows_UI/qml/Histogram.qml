import QtQuick

Rectangle {
    id: plot
    property var channels: []
    color: Theme.wellDark
    radius: 3
    implicitHeight: 68
    onChannelsChanged: graph.requestPaint()
    Canvas {
        id: graph
        anchors.fill: parent; anchors.margins: 4
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var colors = ["#ec6d63", "#83bb73", "#7499e7"]
            for (var c = 0; c < plot.channels.length; ++c) {
                var values = plot.channels[c]
                ctx.beginPath()
                for (var i = 0; i < values.length; ++i) {
                    var x = i * width / Math.max(1, values.length - 1)
                    var y = height * (1 - values[i])
                    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y)
                }
                ctx.strokeStyle = colors[c]; ctx.lineWidth = 1; ctx.stroke()
            }
        }
    }
    Accessible.role: Accessible.Graphic
    Accessible.name: "当前预览的 RGB 直方图，纵轴为对数"
}
