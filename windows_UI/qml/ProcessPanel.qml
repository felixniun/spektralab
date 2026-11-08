import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtCore
ColumnLayout {
    id: panel
    signal collapseRequested()
    Settings {id: viewState;location: workspaceSettingsUrl;category:"Parameters";property bool postActive:false}
    function options(values, labels) {return values.map(function(v,i){return {value:v,label:labels[i]}})}
    readonly property real highlightPlacement: Number(preview.adjustments.ui_highlight_pull_back || 0)
    readonly property real shadowPlacement: Number(preview.adjustments.ui_shadow_pull_back || 0)
    spacing: 0
    RowLayout {
        Layout.fillWidth: true; Layout.preferredHeight: Theme.topHeight; Layout.leftMargin: 12; Layout.rightMargin: 9; spacing: 7
        Text { text: "参数"; color: Theme.text; font.pixelSize: 13; font.bold: true }
        ActionButton { text: "显影前"; outlined: true; accented: !viewState.postActive; implicitHeight: 18; font.pixelSize: 11; Layout.fillWidth: true; onClicked: viewState.postActive=false }
        ActionButton { objectName: "postTab"; text: "显影后"; outlined: true; accented: viewState.postActive; implicitHeight: 18; font.pixelSize: 11; Layout.fillWidth: true; onClicked: viewState.postActive=true }
        ActionButton { text: "◨"; iconOnly: true; onClicked: panel.collapseRequested(); Accessible.name: "折叠参数侧栏" }
    }
    ScrollView {
        visible: !viewState.postActive
        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 0
            Section { title: "宽容度"; LatitudePlot {} }
            Section {
                title: "输入 / 相机"; resettable: true; onResetRequested: preview.resetSection("camera")
                RailMenu { label: "测光方式"; value: preview.adjustments.auto_exposure ? preview.adjustments.auto_exposure_method : "custom"; options: panel.options(["custom","balanced","center","protect_highlights","protect_shadows","center_weighted","average","median"],["自定义","均衡","居中","保护高光","保护阴影","中央重点","平均","中值"]); onChosen: function(value){preview.setMetering(value)} }
                RailSlider { label: "胶片曝光"; setting: "filmExposureEv"; from: -8; to: 8; decimals: 1; suffix: " EV" }
                RailToggle { label: "白平衡 · 拍摄设置"; setting: "raw_as_shot"; available: preview.decodeMode===0 }
                RailSlider { label: "色温"; setting: "raw_temperature"; from: 2000; to: 12000; step: 10; decimals: 0; fallback: 5500; available: preview.decodeMode===0 }
                RailSlider { label: "色调"; setting: "raw_tint"; from: -150; to: 150; decimals: 1; available: preview.decodeMode===0 }
                RailSlider { label: "暗角"; setting: "post_vignette"; from: -100; to: 100; decimals: 1 }
                RailMenu { label: "RAW 解码"; value: preview.decodeMode; options: panel.options([0,1],["兼容模式","高光扩展"]); onChosen: function(value){preview.decodeMode=value} }
            }
            Section {
                title: "胶片画幅"; resettable: true; onResetRequested: preview.resetSection("format")
                RailMenu { label: "尺寸"; setting: "filmFormatMm"; available: !preview.adjustments.overscan_active; options: panel.options([18,24,35,36,56,70,90,168],["半格 · 18 mm","半格 · 24 mm","135 · 35 mm","135 · 36 mm","120 · 56 mm","120 · 70 mm","120 · 90 mm","6×17 · 168 mm"]) }
                RailSlider { label: "边长"; setting: "filmFormatMm"; from: 4; to: 200; fallback: 35; decimals: 1; suffix: " mm"; available: !preview.adjustments.overscan_active }
                RailToggle { label: "颗粒"; setting: "grainActive" }
                RailSlider { label: "颗粒强度"; setting: "grainAmount"; from: 0; to: 2; fallback: 1; available: !!preview.adjustments.grainActive }
                RailToggle { label: "光晕"; setting: "halationActive" }
                RailSlider { label: "光晕强度"; setting: "halationAmount"; from: 0; to: 4; fallback: 1; available: !!preview.adjustments.halationActive }
                RailToggle { label: "耀光"; setting: "glareActive" }
                RailSlider { label: "耀光强度"; setting: "glareAmount"; from: 0; to: 30; fallback: 1; available: !!preview.adjustments.glareActive && !preview.filmIsPositive }
            }
            Section {
                title: "场景置位"; resettable: true; onResetRequested: preview.resetSection("placement")
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: 23
                    Text { text: "高光"; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 74 }
                    Slider { id: highlights; Layout.fillWidth: true; from: 0; to: 8; value: panel.highlightPlacement; stepSize: 0.05; enabled: preview.settingsEditable && preview.hasFrame
                        onMoved: {if(!pressed)preview.placeScene(value,shadows.value)}
                        onPressedChanged: {if(!pressed)preview.placeScene(value,shadows.value)}
                        background: Rectangle {y:parent.height/2;width:parent.width;height:1;color:"#878783"}
                        handle: Rectangle {x:highlights.visualPosition*(highlights.width-width);y:(highlights.height-height)/2;width:5;height:5;radius:3;color:Theme.text}
                    }
                    Text { text: (highlights.pressed?highlights.value:panel.highlightPlacement).toFixed(2); color: Theme.text; font.pixelSize: 10; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredHeight: 23
                    Text { text: "阴影"; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 74 }
                    Slider { id: shadows; Layout.fillWidth: true; from: 0; to: 8; value: panel.shadowPlacement; stepSize: 0.05; enabled: preview.settingsEditable && preview.hasFrame
                        onMoved: {if(!pressed)preview.placeScene(highlights.value,value)}
                        onPressedChanged: {if(!pressed)preview.placeScene(highlights.value,value)}
                        background: Rectangle {y:parent.height/2;width:parent.width;height:1;color:"#878783"}
                        handle: Rectangle {x:shadows.visualPosition*(shadows.width-width);y:(shadows.height-height)/2;width:5;height:5;radius:3;color:Theme.text}
                    }
                    Text { text: (shadows.pressed?shadows.value:panel.shadowPlacement).toFixed(2); color: Theme.text; font.pixelSize: 10; Layout.preferredWidth: 35; horizontalAlignment: Text.AlignRight }
                }
            }
            Section {
                title: "状态"; visible: preview.failed || preview.batchExporting || preview.persistenceWarning.length>0
                Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: preview.status; color: preview.failed ? "#ffb39f" : Theme.text; font.pixelSize: 10 }
                Text { objectName: "persistenceWarningNote"; Layout.fillWidth: true; wrapMode: Text.WordWrap; text: preview.persistenceWarning; visible: text.length>0; color: "#ffb39f"; font.pixelSize: 10 }
                ProgressBar { Layout.fillWidth: true; visible: preview.batchExporting; from: 0; to: Math.max(1,preview.batchTotal); value: preview.batchProgress }
                ActionButton { text: "当前照片完成后停止"; visible: preview.batchExporting; onClicked: preview.cancelExport() }
                ActionButton { text: "重新尝试预览"; visible: preview.failed && preview.hasFrame && !preview.busy; onClicked: preview.apply() }
            }
            Text { objectName: "savedStatusNote"; text: preview.savedStatus; visible: false }
        }
    }
    PostPanel { visible: viewState.postActive; Layout.fillWidth: true; Layout.fillHeight: true }
}
