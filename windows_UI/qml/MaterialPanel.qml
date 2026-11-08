import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SpektraLab.Native 1.0

ColumnLayout {
    id: panel
    required property var editor
    signal collapseRequested()
    spacing: 0
    function options(values, labels) { return values.map(function(v,i) {return {value:v,label:labels[i]}}) }
    ListModel {
        id: clipboardGroups
        ListElement { key: "film"; label: "胶片与相纸"; selected: true }
        ListElement { key: "exposure"; label: "曝光"; selected: true }
        ListElement { key: "whiteBalance"; label: "白平衡"; selected: true }
        ListElement { key: "filmEffects"; label: "胶片效果"; selected: true }
        ListElement { key: "printEffects"; label: "相纸效果"; selected: true }
        ListElement { key: "scenePlacement"; label: "场景置位"; selected: true }
    }
    function pasteGroups() { var groups=[];for(var i=0;i<clipboardGroups.count;++i)if(clipboardGroups.get(i).selected)groups.push(clipboardGroups.get(i).key);preview.pasteSettings(groups) }
    RowLayout {
        Layout.fillWidth: true; Layout.preferredHeight: Theme.topHeight; Layout.leftMargin: 12; Layout.rightMargin: 10
        Text { text: "胶片与相纸"; color: Theme.text; font.pixelSize: 13; font.bold: true; Layout.fillWidth: true }
        ActionButton { text: "◧"; iconOnly: true; onClicked: panel.collapseRequested(); Accessible.name: "折叠胶片与相纸侧栏" }
    }
    ScrollView {
        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; contentWidth: availableWidth
        ColumnLayout {
            width: parent.width; spacing: 0
            Section {
                title: "导航"; objectName: "navigatorSection"
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 150; color: "#1a1a19"
                    FrameCanvas { anchors.fill: parent; anchors.margins: 5; image: preview.previewImage; enabled: false }
                    Text { visible: !preview.hasFrame; anchors.centerIn: parent; text: "SpektraLab"; color: Theme.muted; font.pixelSize: 14 }
                    ActionButton { anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 3; text: "适合窗口"; implicitHeight: 17; font.pixelSize: 9; enabled: preview.hasFrame; onClicked: panel.editor.fitCanvas() }
                }
            }
            HistoryPanel { visible: panel.editor.historyVisible }
            Section {
                title: "设置剪贴板"; objectName: "clipboardSection"
                Repeater {
                    model: clipboardGroups
                    delegate: RowLayout {
                        required property int index
                        required property string label
                        required property bool selected
                        Layout.fillWidth: true; Layout.preferredHeight: 22
                        Text { text: label; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
                        CheckBox { id: clipCheck; implicitWidth: 14; implicitHeight: 16; padding: 0; checked: selected
                            indicator: Rectangle { width: 10; height: 10; radius: 2; y: 3; color: clipCheck.checked ? Theme.accent : "transparent"; border.color: Theme.muted; border.width: clipCheck.checked ? 0 : 1 }
                            onToggled: clipboardGroups.setProperty(index,"selected",checked)
                            Accessible.name: label
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    ActionButton { text: "复制"; enabled: preview.hasFrame && !preview.busy; onClicked: preview.copySettings() }
                    ActionButton { text: "粘贴"; enabled: preview.canPaste && preview.hasFrame && preview.settingsEditable; onClicked: panel.pasteGroups() }
                    ActionButton { text: "同步"; enabled: preview.hasFrame && !preview.busy && preview.selectedCount>1; onClicked: preview.syncSelectedSettings(false) }
                    Item { Layout.fillWidth: true }
                }
                Text { text: preview.canPaste ? "已复制当前照片设置。" : "剪贴板为空。"; color: Theme.muted; font.pixelSize: 9 }
            }
            Section {
                title: "胶片"; objectName: "filmSection"
                StockList { stocks: preview.films; group: "positive" }
                StockList { stocks: preview.films; group: "negative" }
                RailToggle { label: "去除防光晕层"; setting: "antihalation_removed" }
                RailMenu { label: "镜头滤镜"; setting: "camera_filter"; options: panel.options(["","w8","w11","w12","w15","w21","w25","w47","w58"],["无","W8 黄色","W11 黄绿","W12 深黄","W15 深黄","W21 橙色","W25 红色","W47 蓝色","W58 绿色"]) }
            }
            Section {
                id: filmEdgeSection; objectName: "filmEdgeSection"; title: "片边"; resettable: true; onResetRequested: preview.resetSection("edge")
                checkable: true; checked: !!preview.adjustments.overscan_active
                onToggled: function(on) { preview.setAdjustment("overscan_active",on) }
                RailMenu { label: "画幅"; setting: "overscan_format"; available: filmEdgeSection.checked; options: panel.options(["135","135_half","135_xpan","120_645","120_6x6","120_6x7","120_6x8","120_6x9","120_6x12","120_6x17"],["135","135 半格","135 XPan","120 · 645","120 · 6×6","120 · 6×7","120 · 6×8","120 · 6×9","120 · 6×12","120 · 6×17"]) }
                RailMenu { label: "视图"; setting: "overscan_mode"; available: filmEdgeSection.checked; options: panel.options(["strip","filed"],["片条","片框"]) }
                RailMenu { label: "片门"; setting: "overscan_gate"; available: filmEdgeSection.checked; options: panel.options(["auto","square","rounded","eared","shouldered","kicked"],["自动","方角","圆角","耳形","肩形","斜角"]) }
                RailMenu { label: "齿孔"; setting: "overscan_holes"; available: filmEdgeSection.checked; options: panel.options(["white","black"],["透光","遮光"]) }
                RailMenu { label: "片夹"; setting: "overscan_carrier"; available: filmEdgeSection.checked; options: panel.options(["none","black","open"],["无","黑色片夹","开放片夹"]) }
                RailSlider { label: "帧号"; setting: "overscan_frame_number"; from: 0; to: 99; step: 1; decimals: 0; available: filmEdgeSection.checked }
                RailSlider { label: "雾化"; setting: "overscan_fog"; from: 0; to: 4; fallback: 1; decimals: 1; available: filmEdgeSection.checked }
                RailSlider { label: "漏光"; setting: "overscan_leaks"; from: 0; to: 4; decimals: 1; available: filmEdgeSection.checked }
                RailText { label: "边码"; setting: "overscan_edge_text"; available: filmEdgeSection.checked }
                RailSlider { label: "镜头光圈"; setting: "overscan_f_number"; from: 0; to: 64; decimals: 1; available: filmEdgeSection.checked }
                RowLayout {
                    Layout.fillWidth: true
                    ActionButton { text: "机身变化"; enabled: filmEdgeSection.checked && preview.settingsEditable; onClicked: preview.setAdjustment("overscan_camera_seed",1+Math.floor(Math.random()*2147483646)) }
                    ActionButton { text: "过片"; enabled: filmEdgeSection.checked && preview.settingsEditable; onClicked: preview.setAdjustment("overscan_frame_seed",1+Math.floor(Math.random()*2147483646)) }
                    Item { Layout.fillWidth: true }
                }
            }
            Section {
                id: dateSection; title: "日期后背"; checkable: true; checked: !!preview.adjustments.date_imprint_active; resettable: true; onResetRequested: preview.resetSection("date")
                onToggled: function(on) { if(on && !preview.adjustments.date_imprint_text)preview.setAdjustment("date_imprint_text",preview.metadata.captureDate || Qt.formatDate(new Date(),"yy.MM.dd"));preview.setAdjustment("date_imprint_active",on) }
                RailMenu { label: "字面"; setting: "date_imprint_style"; available: dateSection.checked; options: [{value:"lcd",label:"LCD"},{value:"dots",label:"点阵"},{value:"data",label:"拍摄数据",available:!!preview.adjustments.overscan_active}] }
                RailMenu { label: "位置"; setting: "date_imprint_placement"; available: dateSection.checked; options: [{value:"frame",label:"画面内"},{value:"rebate",label:"片边",available:!!preview.adjustments.overscan_active}] }
                RailMenu { label: "角落"; setting: "date_imprint_corner"; available: dateSection.checked; options: panel.options(["br","bl","tr","tl"],["右下","左下","右上","左上"]) }
                RailText { label: "日期"; setting: "date_imprint_text"; available: dateSection.checked }
                RailSlider { label: "亮度"; setting: "date_imprint_ev"; from: -2; to: 8; fallback: 3.5; decimals: 1; available: dateSection.checked }
                RailSlider { label: "尺寸"; setting: "date_imprint_size"; from: 0.4; to: 3; fallback: 1; decimals: 1; available: dateSection.checked }
                Text { visible: dateSection.checked && !preview.adjustments.overscan_active && (preview.adjustments.date_imprint_style==="data" || preview.adjustments.date_imprint_placement==="rebate"); text: "片边印字需要开启片边。"; color: Theme.muted; font.pixelSize: 9 }
            }
            Section {
                title: "相纸"
                StockList { stocks: preview.papers; group: "paper" }
                Text { visible: preview.filmIsPositive; text: "正片直接扫描，不使用相纸。"; color: Theme.muted; font.pixelSize: 10 }
                RailToggle { label: "直接扫描（不使用相纸）"; setting: "ui_scan_film"; available: !preview.filmIsPositive }
                RailToggle { label: "相纸效果"; setting: "ui_print_effects"; available: !preview.filmIsPositive && !preview.adjustments.ui_scan_film }
                RailToggle { label: "扩展动态范围（EDR）"; setting: "extended_dynamic_range"; available: !!(!preview.filmIsPositive && !preview.adjustments.ui_scan_film && preview.adjustments.ui_print_effects && preview.papers[preview.paperIndex] && !preview.papers[preview.paperIndex].monochrome) }
            }
            Section {
                title: "裁剪"
                RailMenu { label: "宽高比"; available: !preview.adjustments.overscan_active; value: 0; options: panel.options([0,1,1.5,4/3,16/9],["自由","1:1","3:2","4:3","16:9"]); onChosen: function(value){ preview.setCropAspect(value);panel.editor.fitCanvas() } }
                ActionButton { text: panel.editor.cropMode ? "完成裁剪" : "在画面中裁剪"; enabled: panel.editor.canEditCrop; onClicked: panel.editor.cropMode ? panel.editor.finishCrop() : panel.editor.startCrop() }
                Text { visible: !!preview.adjustments.overscan_active; text: "在原图上取景，宽高比由片门锁定。关闭片边恢复原裁剪。"; color: Theme.muted; font.pixelSize: 9; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    Layout.fillWidth: true; spacing: 5
                    Text { text: "旋转"; color: Theme.text; font.pixelSize: 11; Layout.fillWidth: true }
                    ActionButton { objectName: "rotateLeftButton"; text: "↶"; enabled: panel.editor.canEditCrop; onClicked: {preview.rotateCounterClockwise();panel.editor.fitCanvas()} }
                    ActionButton { objectName: "rotateRightButton"; text: "↷"; enabled: panel.editor.canEditCrop; onClicked: {preview.rotateClockwise();panel.editor.fitCanvas()} }
                    ActionButton { objectName: "flipHorizontalButton"; text: "↔"; enabled: panel.editor.canEditCrop; onClicked: preview.setFlipHorizontal(!preview.flipHorizontal) }
                    ActionButton { objectName: "flipVerticalButton"; text: "↕"; enabled: panel.editor.canEditCrop; onClicked: preview.setFlipVertical(!preview.flipVertical) }
                }
                RowLayout {
                    Text { text: "拉直"; color: Theme.text; font.pixelSize: 11; Layout.preferredWidth: 74 }
                    Slider { id: straightenSlider; objectName: "straightenSlider"; Layout.fillWidth: true; from: -45; to: 45; stepSize: 0.1; value: preview.straightenDegrees; enabled: panel.editor.canEditCrop
                        onPressedChanged: {if(pressed)preview.beginEditGesture("straightenDegrees");else {preview.setStraightenDegrees(value);preview.endEditGesture()}}
                        onMoved: {if(!pressed)preview.setStraightenDegrees(value)}
                        background: Rectangle { y: parent.height/2; width: parent.width; height: 1; color: "#878783" }
                        handle: Rectangle { x: straightenSlider.visualPosition*(straightenSlider.width-width); y:(straightenSlider.height-height)/2; width:5;height:5;radius:3;color:Theme.text }
                    }
                    Text { text: preview.straightenDegrees.toFixed(1)+"°"; color: Theme.text; font.pixelSize: 10 }
                }
                ActionButton { objectName: "resetGeometryButton"; text: "重置裁剪与方向"; enabled: panel.editor.canEditCrop; onClicked: {preview.resetGeometry();panel.editor.fitCanvas()} }
            }
            Section {
                title: "放大机"; resettable: true; onResetRequested: preview.resetSection("enlarger")
                RailSlider { label: "亮度"; setting: "exposureEv"; from: -2; to: 2; decimals: 2; available: !preview.filmIsPositive && !preview.adjustments.ui_scan_film; controlName: "exposureSlider" }
                RailSlider { label: "黄色"; setting: "yFilterShift"; from: -1; to: 1; available: !preview.filmIsPositive && !preview.adjustments.ui_scan_film }
                RailSlider { label: "洋红"; setting: "mFilterShift"; from: -1; to: 1; available: !preview.filmIsPositive && !preview.adjustments.ui_scan_film }
                RailSlider { label: "预曝光"; setting: "preflash_exposure"; from: 0; to: 1; available: !preview.filmIsPositive && !!preview.adjustments.ui_print_effects }
            }
        }
    }
}
