import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
RowLayout {
    id: row
    property string label
    property string setting
    property var options: []
    property bool available: true
    property var value: preview.adjustments[setting]
    signal chosen(var value)
    Layout.fillWidth: true; Layout.preferredHeight: 23; spacing: 6
    opacity: available ? 1 : 0.38
    Text { text: row.label; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
    ComboBox {
        id: menu; objectName: "railMenu_" + (row.setting.length ? row.setting : row.label); Layout.preferredWidth: 114; implicitHeight: 17
        model: row.options; textRole: "label"; valueRole: "value"
        currentIndex: { for (var i=0;i<row.options.length;++i) if (row.options[i].value === row.value) return i; return -1 }
        enabled: preview.settingsEditable && row.available; font.pixelSize: 10
        background: Rectangle { radius: 8; color: Theme.well }
        contentItem: Text { text: menu.displayText; color: Theme.text; font: menu.font; leftPadding: 8; rightPadding: 15; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        indicator: Text { text: "⌄"; x: menu.width-14; y: -1; font.pixelSize: 12; color: Theme.text }
        delegate: ItemDelegate {
            id: option
            required property var modelData
            required property int index
            width: menu.width; implicitHeight: 26
            padding: 4; leftPadding: 8; rightPadding: 8
            text: modelData.label; enabled: modelData.available !== false
            highlighted: menu.highlightedIndex === index
            font.pixelSize: 11
            contentItem: Text {
                text: option.text; font: option.font
                color: !option.enabled ? Theme.muted : option.highlighted ? Theme.accent : Theme.text
                verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
            }
            background: Rectangle { color: option.highlighted || option.hovered ? "#4c4c48" : Theme.card }
        }
        onActivated: { if (row.setting.length) preview.setAdjustment(row.setting, currentValue); row.chosen(currentValue) }
    }
}
