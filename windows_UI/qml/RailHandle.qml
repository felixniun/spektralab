import QtQuick

MouseArea {
    id: handle
    property real currentWidth: 254
    property real minimum: 232
    property real maximum: 380
    property real defaultWidth: 254
    property real direction: 1
    property real origin: 0
    property real initialWidth: 0
    signal resized(real value)
    width: 8
    hoverEnabled: true
    cursorShape: Qt.SplitHCursor
    onPressed: function(mouse) {
        origin = mapToItem(null, mouse.x, mouse.y).x
        initialWidth = currentWidth
    }
    onPositionChanged: function(mouse) {
        if (pressed) resized(Math.max(minimum, Math.min(maximum,
            initialWidth + direction * (mapToItem(null, mouse.x, mouse.y).x - origin))))
    }
    onDoubleClicked: resized(defaultWidth)
    Rectangle { anchors.centerIn: parent; width: 1; height: parent.height; color: handle.containsMouse || handle.pressed ? Theme.accent : "transparent" }
}
