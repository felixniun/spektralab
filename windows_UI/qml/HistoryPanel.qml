import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Section {
    id: history
    objectName: "historyPanel"
    title: "历史记录"
    RowLayout {
        Layout.fillWidth: true; spacing: 5
        ActionButton { text: "撤销"; enabled: preview.canUndo; onClicked: preview.undo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Z · " + preview.undoLabel }
        ActionButton { text: "重做"; enabled: preview.canRedo; onClicked: preview.redo(); ToolTip.visible: hovered; ToolTip.text: "Ctrl+Shift+Z / Ctrl+Y · " + preview.redoLabel }
        Item { Layout.fillWidth: true }
        Text { text: preview.historyIndex >= 0 ? preview.historyIndex + " 步" : ""; color: Theme.muted; font.pixelSize: 10 }
    }
    ListView {
        id: steps
        objectName: "historyList"
        Layout.fillWidth: true; Layout.preferredHeight: Math.max(28,Math.min(140,count*28))
        clip: true; model: preview.historyItems; currentIndex: preview.historyIndex
        onCurrentIndexChanged: Qt.callLater(function(){steps.positionViewAtIndex(steps.currentIndex,ListView.Contain)})
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: ItemDelegate {
            id: step
            required property var modelData
            width: steps.width; height: 28
            enabled: preview.settingsEditable
            opacity: modelData.future ? 0.48 : 1
            contentItem: Text {
                text: (step.modelData.current ? "● " : step.modelData.future ? "↪ " : "  ") + step.modelData.label
                color: step.modelData.current ? Theme.accent : Theme.text
                font.pixelSize: 10; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { radius: 3; color: step.modelData.current ? "#45413a" : step.hovered ? "#3b3c39" : "#242523" }
            onClicked: preview.jumpHistory(modelData.step)
            Accessible.name: modelData.label + (modelData.current ? "，当前状态" : modelData.future ? "，可重做" : "")
            ToolTip.visible: hovered; ToolTip.text: modelData.detail
        }
        Text { anchors.centerIn: parent; visible: steps.count===0; text: "导入照片后记录调整"; color: Theme.muted; font.pixelSize: 10 }
    }
    Text { text: "本次运行 · 每张照片最多保留 64 步"; color: Theme.muted; font.pixelSize: 9 }
}
