import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
Rectangle {
    id: bar
    required property var editor
    color: Theme.card; implicitHeight: Theme.topHeight
    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12; spacing: 8
        ActionButton { visible: !bar.editor.leftVisible; text: "◧"; iconOnly: true; onClicked: bar.editor.toggleLeft(); Accessible.name: "显示胶片与相纸侧栏" }
        ActionButton { text: "⇩"; iconOnly: true; font.pixelSize: 20; enabled: bar.editor.canBrowse; onClicked: bar.editor.importPhotos(); ToolTip.visible: hovered; ToolTip.text: "导入照片 · Ctrl+O"; Accessible.name: "导入照片" }
        ActionButton { text: "⇧"; iconOnly: true; font.pixelSize: 20; enabled: preview.hasFrame && !preview.busy; onClicked: bar.editor.openExport(); ToolTip.visible: hovered; ToolTip.text: "导出 · Ctrl+E"; Accessible.name: "导出" }
        ActionButton { objectName: "undoToolbarButton"; text: "↶"; iconOnly: true; font.pixelSize: 19; enabled: preview.canUndo && !bar.editor.cropMode; onClicked: preview.undo(); ToolTip.visible: hovered; ToolTip.text: "撤销 · Ctrl+Z\n" + preview.undoLabel; Accessible.name: "撤销 Ctrl+Z" }
        ActionButton { objectName: "redoToolbarButton"; text: "↷"; iconOnly: true; font.pixelSize: 19; enabled: preview.canRedo && !bar.editor.cropMode; onClicked: preview.redo(); ToolTip.visible: hovered; ToolTip.text: "重做 · Ctrl+Shift+Z / Ctrl+Y\n" + preview.redoLabel; Accessible.name: "重做 Ctrl+Shift+Z Ctrl+Y" }
        ActionButton { objectName: "historyToolbarButton"; text: "历史"; accented: bar.editor.historyVisible; implicitHeight: 20; onClicked: bar.editor.toggleHistory(); Accessible.name: "显示或隐藏历史记录" }
        ActionButton { objectName: "lighttableToolbarButton"; text: "观片台"; implicitHeight: 20; onClicked: bar.editor.openLighttable(); Accessible.name: "打开观片台" }
        ActionButton { text: "↖"; iconOnly: true; font.pixelSize: 18; enabled: preview.hasFrame; onClicked: bar.editor.cancelCrop(); ToolTip.visible: hovered; ToolTip.text: "选择工具" }
        ActionButton { text: "⌗"; iconOnly: true; font.pixelSize: 20; accented: bar.editor.cropMode; enabled: bar.editor.canEditCrop; onClicked: bar.editor.cropMode ? bar.editor.finishCrop() : bar.editor.startCrop(); ToolTip.visible: hovered; ToolTip.text: "裁剪" }
        ActionButton { text: "配光"; outlined: true; accented: enabled; implicitWidth: 83; implicitHeight: 20; font.pixelSize: 13; enabled: preview.hasFrame && !preview.busy && !preview.filmIsPositive && !preview.adjustments.ui_scan_film; onClicked: preview.solve(); ToolTip.visible: hovered; ToolTip.text: "求解测光与放大机配光" }
        ActionButton { text: "原图"; outlined: true; accented: bar.editor.showingOriginal; implicitWidth: 83; implicitHeight: 20; font.pixelSize: 13; enabled: preview.hasFrame && !preview.adjustments.overscan_active; onClicked: bar.editor.toggleOriginal(); ToolTip.visible: hovered; ToolTip.text: "输入照片解码后的画面，胶片模拟之前" }
        Text { text: preview.hasFrame ? preview.fileName + " · " + preview.metadata.width + "×" + preview.metadata.height + " · sRGB" : ""; color: Theme.muted; font.pixelSize: 10; Layout.fillWidth: true; elide: Text.ElideMiddle }
        ActionButton { text: "◧"; iconOnly: true; accented: bar.editor.splitCompare; enabled: preview.hasFrame && !preview.adjustments.overscan_active; onClicked: bar.editor.toggleSplit(); ToolTip.visible: hovered; ToolTip.text: "原图与显影结果分屏对比" }
        ActionButton { text: bar.editor.canvasScale.toFixed(0)+"%"; implicitHeight: 18; onClicked: bar.editor.fitCanvas(); enabled: preview.hasFrame; ToolTip.visible: hovered; ToolTip.text: "适合窗口" }
        ActionButton { text: "⊖"; iconOnly: true; font.pixelSize: 18; enabled: preview.hasFrame; onClicked: bar.editor.zoomCanvas(1/1.25) }
        ActionButton { text: "⊕"; iconOnly: true; font.pixelSize: 18; enabled: preview.hasFrame; onClicked: bar.editor.zoomCanvas(1.25) }
        ActionButton { visible: !bar.editor.rightVisible; text: "◨"; iconOnly: true; onClicked: bar.editor.toggleRight(); Accessible.name: "显示参数侧栏" }
    }
}
