import QtQuick
import QtQuick.Layouts
ColumnLayout {
    id: editor
    property int tab: 0
    Layout.fillWidth: true; spacing: 7
    RowLayout {
        Layout.fillWidth: true; spacing: 3
        Repeater { model: ["主控","三向","阴影","中间调","高光"]
            ActionButton { required property int index; required property string modelData; text: modelData; outlined: true; accented: editor.tab===index; implicitHeight: 19; font.pixelSize: 10; Layout.fillWidth: true; onClicked: editor.tab=index }
        }
    }
    ColorWheel { visible: editor.tab!==1; zone: ["master","master","shadows","midtones","highlights"][editor.tab]; label: ["主控","主控","阴影","中间调","高光"][editor.tab]; diameter: Math.min(185,editor.width-30); Layout.alignment: Qt.AlignHCenter }
    ColumnLayout {
        visible: editor.tab===1; Layout.fillWidth: true; spacing: 8
        ColorWheel { zone: "midtones"; label: "中间调"; diameter: Math.max(60,(editor.width-48)/2); Layout.alignment: Qt.AlignHCenter }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter; spacing: 12
            ColorWheel { zone: "shadows"; label: "阴影"; diameter: Math.max(60,(editor.width-48)/2) }
            ColorWheel { zone: "highlights"; label: "高光"; diameter: Math.max(60,(editor.width-48)/2) }
        }
    }
}
