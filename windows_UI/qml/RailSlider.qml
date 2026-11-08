import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

RowLayout {
    id: row
    property string label
    property string setting
    property real from: 0
    property real to: 1
    property real step: 0.01
    property int decimals: 2
    property real fallback: 0
    property string suffix: ""
    property real value: setting === "exposureEv" ? preview.exposureEv : Number(preview.adjustments[setting] === undefined ? fallback : preview.adjustments[setting])
    property bool available: true
    property string controlName: ""
    Layout.fillWidth: true; Layout.preferredHeight: 23; spacing: 7
    opacity: available ? 1 : 0.38
    function commit(v) {
        if (!isFinite(v)) return
        v = Math.max(from, Math.min(to, v))
        if (setting === "exposureEv") preview.exposureEv = v
        else preview.setAdjustment(setting, v)
    }
    Text { text: row.label; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 74; elide: Text.ElideRight }
    Slider {
        id: slider; objectName: row.controlName
        Layout.fillWidth: true; Layout.minimumWidth: 25; implicitHeight: 20
        from: row.from; to: row.to; stepSize: row.step; value: row.value
        enabled: preview.settingsEditable && row.available
        onMoved: row.commit(value)
        onPressedChanged: { if (pressed) preview.beginEditGesture(row.setting); else preview.endEditGesture() }
        background: Rectangle { x: slider.leftPadding; y: (slider.height-height)/2; width: slider.availableWidth; height: 1; color: "#878783" }
        handle: Rectangle { x: slider.leftPadding + slider.visualPosition*(slider.availableWidth-width); y: (slider.height-height)/2; width: 5; height: 5; radius: 3; color: Theme.text }
    }
    TextField {
        objectName: row.controlName.length ? row.controlName + "Field" : ""
        Layout.preferredWidth: 43; implicitHeight: 17; padding: 2
        text: Number(row.value).toFixed(row.decimals)
        horizontalAlignment: Text.AlignRight; color: Theme.text; font.pixelSize: 10
        enabled: preview.settingsEditable && row.available; selectByMouse: true
        background: Rectangle { radius: 4; color: parent.activeFocus ? Theme.well : "#454543" }
        property bool committing: false
        onEditingFinished: {
            if(committing)return
            committing=true; row.commit(parseFloat(text)); focus=false; committing=false
        }
    }
    Text { visible: row.suffix.length>0; text: row.suffix.trim(); color: Theme.muted; font.pixelSize: 9 }
}
