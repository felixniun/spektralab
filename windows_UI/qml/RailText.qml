import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
RowLayout {
    id: row
    property string label
    property string setting
    property bool available: true
    Layout.fillWidth: true; Layout.preferredHeight: 23; spacing: 6
    opacity: available ? 1 : 0.38
    Text { text: row.label; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 58 }
    TextField {
        Layout.fillWidth: true; implicitHeight: 18; padding: 3
        text: preview.adjustments[row.setting] || ""; color: Theme.text; font.pixelSize: 10; selectByMouse: true
        enabled: preview.settingsEditable && row.available
        background: Rectangle { radius: 3; color: "#454543" }
        property bool committing: false
        onEditingFinished: {
            if(committing)return
            committing=true; preview.setAdjustment(row.setting,text); focus=false; committing=false
        }
        Accessible.name: row.label
    }
}
