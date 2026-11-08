import QtQuick
import QtQuick.Controls.Basic
Button {
    id: root
    property bool accented: false
    property bool iconOnly: false
    property bool outlined: false
    padding: iconOnly ? 3 : 7
    implicitHeight: iconOnly ? 26 : 22
    font.pixelSize: 11
    hoverEnabled: true
    contentItem: Text {
        text: root.text; color: root.enabled ? root.accented ? Theme.accent : Theme.text : "#777773"
        font: root.font; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 6
        color: root.down ? "#686762" : root.hovered ? "#585752" : root.iconOnly || root.outlined ? "transparent" : Theme.well
        border.width: root.accented || root.outlined ? 1 : 0
        border.color: root.accented ? Theme.accent : Theme.muted
    }
}
