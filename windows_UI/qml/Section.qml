import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
ColumnLayout {
    id: root
    property string title
    property bool collapsed: false
    property bool checkable: false
    property bool checked: false
    property bool toggleEnabled: true
    property bool resettable: false
    signal resetRequested()
    signal toggled(bool checked)
    default property alias content: rows.data
    Layout.fillWidth: true
    spacing: 0
    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line }
    RowLayout {
        Layout.fillWidth: true; Layout.preferredHeight: 26; Layout.leftMargin: 7; Layout.rightMargin: 10; spacing: 6
        Text { text: root.collapsed ? "▸" : "▾"; color: Theme.text; font.pixelSize: 12; TapHandler { onTapped: root.collapsed = !root.collapsed } }
        Text { text: root.title; color: Theme.text; font.pixelSize: 12; font.bold: true; Layout.fillWidth: true; TapHandler { onTapped: root.collapsed = !root.collapsed } }
        Text { text: "↺"; color: Theme.muted; font.pixelSize: 12; visible: root.resettable; TapHandler { onTapped: root.resetRequested() } }
        CheckBox {
            visible: root.checkable; checked: root.checked; enabled: preview.settingsEditable && root.toggleEnabled; padding: 0; implicitWidth: 14; implicitHeight: 18
            indicator: Rectangle { width: 11; height: 11; y: (parent.height-height)/2; radius: 2; color: parent.checked ? Theme.accent : "transparent"; border.color: parent.checked ? Theme.accent : Theme.muted; border.width: 1 }
            onToggled: root.toggled(checked)
        }
        Text { text: "⋯"; color: Theme.text; font.pixelSize: 15; visible: !root.checkable; TapHandler { onTapped: sectionMenu.popup() } }
    }
    ColumnLayout { id: rows; visible: !root.collapsed; Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 12; Layout.bottomMargin: 10; spacing: 3 }
    Menu { id: sectionMenu
        MenuItem { text: root.collapsed ? "展开此部分" : "收起此部分"; onTriggered: root.collapsed = !root.collapsed }
        MenuItem { text: "重置此部分"; visible: root.resettable; enabled: preview.settingsEditable; onTriggered: root.resetRequested() }
    }
}
