import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
ScrollView {
    id: panel
    clip: true; contentWidth: availableWidth
    ColumnLayout {
        width: parent.width; spacing: 0
        Section {
            title: "白平衡"; checkable: true; checked: preview.adjustments.post_enabled !== false; resettable: true; onResetRequested: preview.resetSection("post_wb")
            onToggled: function(on){preview.setAdjustment("post_enabled",on)}
            RailSlider { label: "色温"; setting: "post_temperature"; from: -100; to: 100; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "色调"; setting: "post_tint"; from: -100; to: 100; decimals: 1; available: !!preview.adjustments.post_enabled }
        }
        Section {
            title: "曝光"; resettable: true; onResetRequested: preview.resetSection("post_exposure")
            RailSlider { label: "曝光"; setting: "post_exposure"; controlName: "postExposureSlider"; from: -3; to: 3; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "对比度"; setting: "post_contrast"; from: -50; to: 50; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "亮度"; setting: "post_brightness"; from: -50; to: 50; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "饱和度"; setting: "post_saturation"; from: -100; to: 100; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "高光"; setting: "post_highlights"; from: -100; to: 100; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "阴影"; setting: "post_shadows"; from: -100; to: 100; decimals: 1; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "黑点"; setting: "post_black_point"; from: 0; to: 50; available: !!preview.adjustments.post_enabled }
            RailSlider { label: "白点"; setting: "post_white_point"; from: 0; to: 50; available: !!preview.adjustments.post_enabled }
        }
        Section { title: "曲线"; resettable: true; onResetRequested: preview.resetSection("post_curves"); CurveEditor { enabled: !!preview.adjustments.post_enabled } }
        Section { title: "色彩平衡"; collapsed: true; resettable: true; onResetRequested: preview.resetSection("post_balance"); ColorBalanceEditor { enabled: !!preview.adjustments.post_enabled } }
        ActionButton { Layout.leftMargin: 16; Layout.bottomMargin: 12; text: "重置显影后调整"; enabled: preview.settingsEditable; onClicked: preview.resetPost() }
    }
}
