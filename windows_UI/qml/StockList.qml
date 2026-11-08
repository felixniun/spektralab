import QtQuick
import QtQuick.Layouts
ColumnLayout {
    id: list
    property var stocks: []
    property string group: "negative"
    Layout.fillWidth: true; spacing: 0
    Text { visible: list.group !== "paper"; text: list.group === "positive" ? "正片" : "负片"; color: Theme.muted; font.pixelSize: 10; Layout.preferredHeight: 18 }
    Repeater {
        model: list.stocks
        delegate: Rectangle {
            id: cell
            required property var modelData
            required property int index
            readonly property bool paper: list.group === "paper"
            readonly property bool chosen: index === (paper ? preview.paperIndex : preview.filmIndex)
            readonly property bool available: {var changed=preview.filmIndex;return !paper||preview.paperAvailable(index)}
            visible: paper || (modelData.positive ? list.group === "positive" : list.group === "negative")
            Layout.fillWidth: true; implicitHeight: visible ? 19 : 0; radius: 7
            color: chosen && available ? Theme.selection : hover.containsMouse ? "#3a3b39" : "transparent"
            Text { anchors.fill: parent; anchors.leftMargin: 5; anchors.rightMargin: 24; text: modelData.name; color: cell.chosen && cell.available ? Theme.selectionText : cell.available ? Theme.text : Theme.muted; font.pixelSize: 10; font.weight: Font.DemiBold; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter }
            Image { anchors.right: parent.right; anchors.rightMargin: 3; anchors.verticalCenter: parent.verticalCenter; width: 17; height: 18; source: modelData.cover || ""; fillMode: Image.PreserveAspectFit; asynchronous: true }
            MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; enabled: preview.settingsEditable && cell.available; onClicked: {if(cell.paper)preview.paperIndex=cell.index;else preview.filmIndex=cell.index} }
        }
    }
}
