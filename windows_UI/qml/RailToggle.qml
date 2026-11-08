import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
RowLayout {
    id: row
    property string label
    property string setting
    property bool available: true
    property bool checked: !!preview.adjustments[setting]
    Layout.fillWidth: true; Layout.preferredHeight: 23
    opacity: available ? 1 : 0.38
    Text { text: row.label; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
    CheckBox {
        id: toggle; checked: row.checked; enabled: preview.settingsEditable && row.available
        implicitWidth: 13; implicitHeight: 18; padding: 0
        indicator: Rectangle { width: 11; height: 11; y: (toggle.height-height)/2; radius: 2; color: toggle.checked ? Theme.accent : "transparent"; border.color: toggle.checked ? Theme.accent : Theme.muted; border.width: 1 }
        onToggled: preview.setAdjustment(row.setting, checked)
        Accessible.name: row.label
    }
}
