import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import QtCore
import SpektraLab.Native 1.0

ApplicationWindow {
    id: window
    objectName: "editorWindow"
    width: 1600; height: 1000
    minimumWidth: 1180; minimumHeight: 740
    visible: true
    color: Theme.card
    title: (preview.hasFrame ? preview.fileName + " — " : "") + "SpektraLab"
    font.family: "Segoe UI"
    font.pixelSize: 12
    palette.window: Theme.card
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.base: Theme.well
    palette.button: Theme.well
    palette.highlight: Theme.accent
    property bool closePending: false
    property bool cropMode: false
    property rect draftCrop: Qt.rect(0, 0, 1, 1)
    property int exportFormat: 0
    property int exportQuality: 95
    property int exportLongEdge: 0
    property bool showingOriginal: false
    property bool splitCompare: false
    readonly property real canvasScale: canvas.scalePercent
    readonly property bool leftVisible: workspace.leftVisible
    readonly property bool rightVisible: workspace.rightVisible
    readonly property bool historyVisible: workspace.historyVisible
    function importPhotos() { openDialog.open() }
    function fitCanvas() { canvas.resetView(true) }
    function zoomCanvas(factor) { canvas.zoomBy(factor) }
    function toggleLeft() {workspace.leftVisible = !workspace.leftVisible}
    function toggleRight() {workspace.rightVisible = !workspace.rightVisible}
    function toggleHistory() { if(!workspace.leftVisible || !workspace.historyVisible){workspace.leftVisible=true;workspace.historyVisible=true}else workspace.historyVisible=false }
    function toggleOriginal() { if(preview.adjustments.overscan_active)return; showingOriginal = !showingOriginal; splitCompare = false }
    function toggleSplit() { if(preview.adjustments.overscan_active)return; splitCompare = !splitCompare; showingOriginal = false }
    Settings {
        id: workspace
        location: workspaceSettingsUrl
        category: "Workspace"
        property real leftWidth: 254
        property real rightWidth: 288
        property bool leftVisible: true
        property bool rightVisible: true
        property bool stripVisible: true
        property bool historyVisible: true
    }
    readonly property bool canEditCrop: preview.hasFrame && !preview.busy && !preview.batchExporting
    readonly property bool canBrowse: preview.ready && !preview.busy && !preview.batchExporting && !cropMode
    // Text controls keep their own undo stack while they own keyboard focus.
    readonly property bool editingText: activeFocusItem !== null && typeof activeFocusItem.text === "string" && typeof activeFocusItem.undo === "function"
    onCropModeChanged: updateDialogState()

    function updateDialogState() {
        preview.setDialogOpen(cropMode || openDialog.visible || importFolderDialog.visible || saveDialog.visible || batchDialog.visible || exportPopup.visible)
    }
    function startCrop() {
        if (!canEditCrop) return
        showingOriginal = false; splitCompare = false
        draftCrop = preview.cropRect
        cropMode = true
        Qt.callLater(function() { canvas.resetView(true) })
    }
    function finishCrop() {
        var next = draftCrop
        var commit = cropMode && canEditCrop
        cropMode = false
        if (commit) preview.setCropRect(next)
        Qt.callLater(function() { canvas.resetView(true) })
    }
    function cancelCrop() { cropMode = false; canvas.resetView(true) }
    function filmGroup(positive) {
        var rows = []
        for (var i = 0; i < preview.films.length; ++i)
            if (preview.films[i].positive === positive)
                rows.push({ "name": preview.films[i].name, "catalogIndex": i })
        return rows
    }
    function openExport() { if (preview.hasFrame && !preview.busy && !cropMode) exportPopup.open() }
    function showExportOptions() { openExport() }
    function formatFilter() {
        if (exportFormat === 3) return ["JPEG 图像 (*.jpg *.jpeg)"]
        if (exportFormat === 1 || exportFormat === 2) return ["PNG 图像 (*.png)"]
        return ["16 位 TIFF (*.tif *.tiff)"]
    }
    onClosing: function(event) {
        if (preview.batchExporting) preview.cancelExport()
        if (preview.busy) { closePending = true; event.accepted = false }
    }
    Connections {
        target: preview
        function onStateChanged() { if (window.closePending && !preview.busy) window.close() }
        function onSourceOpened() { window.cropMode = false; canvas.resetView(true) }
        function onSettingsChanged() { if(preview.adjustments.overscan_active){window.showingOriginal=false;window.splitCompare=false} }
    }
    FileDialog {
        id: openDialog
        title: "导入一张或多张 RAW 照片"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["相机 RAW (*.arw *.nef *.nrw *.dng *.cr2 *.cr3 *.raf *.rw2 *.orf *.pef *.srw)", "所有文件 (*)"]
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.openFiles(selectedFiles)
    }
    FileDialog {
        id: saveDialog
        title: "导出当前画面 · 不覆盖现有文件"
        fileMode: FileDialog.SaveFile
        nameFilters: window.formatFilter()
        defaultSuffix: window.exportFormat === 3 ? "jpg" : window.exportFormat === 0 ? "tif" : "png"
        currentFolder: preview.sourceFolder
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.exportImage(selectedFile, window.exportFormat, window.exportQuality, window.exportLongEdge)
    }
    FolderDialog {
        id: importFolderDialog
        title: "导入文件夹中的 RAW 照片"
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.openFolder(selectedFolder)
    }
    FolderDialog {
        id: batchDialog
        title: "选择批量导出文件夹 · 跳过已有文件"
        currentFolder: preview.sourceFolder
        onVisibleChanged: window.updateDialogState()
        onAccepted: preview.exportBatch(selectedFolder, window.exportFormat, window.exportQuality, window.exportLongEdge)
    }
    Shortcut { sequences: [StandardKey.Open]; enabled: window.canBrowse; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+E"; enabled: preview.hasFrame && !preview.busy && !window.cropMode; onActivated: window.openExport() }
    Shortcut { sequence: "Ctrl+0"; enabled: preview.hasFrame; onActivated: canvas.resetView(true) }
    Shortcut { sequence: "Ctrl+1"; enabled: preview.hasFrame && !window.cropMode; onActivated: canvas.resetView(false) }
    Shortcut { sequence: "Ctrl+Z"; enabled: preview.settingsEditable && preview.canUndo && !window.cropMode && !window.editingText; onActivated: preview.undo() }
    Shortcut { sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]; enabled: preview.settingsEditable && preview.canRedo && !window.cropMode && !window.editingText; onActivated: preview.redo() }
    Shortcut { sequence: "Escape"; enabled: window.cropMode; onActivated: window.cancelCrop() }
    Shortcut { sequence: "Return"; enabled: window.cropMode; onActivated: window.finishCrop() }
    menuBar: MenuBar {
        Menu {
            title: "文件(&F)"
            MenuItem { text: "导入 RAW…"; enabled: window.canBrowse; onTriggered: openDialog.open() }
            MenuItem { text: "导入文件夹…"; enabled: window.canBrowse; onTriggered: importFolderDialog.open() }
            MenuSeparator {}
            MenuItem { text: "导出…"; enabled: preview.hasFrame && !preview.busy; onTriggered: window.openExport() }
            MenuSeparator {}
            MenuItem { text: "退出"; onTriggered: window.close() }
        }
        Menu {
            title: "编辑(&E)"
            MenuItem { objectName: "undoMenuItem"; text: "撤销  ·  Ctrl+Z"; enabled: preview.settingsEditable && preview.canUndo && !window.cropMode; onTriggered: preview.undo() }
            MenuItem { objectName: "redoMenuItem"; text: "重做  ·  Ctrl+Shift+Z / Ctrl+Y"; enabled: preview.settingsEditable && preview.canRedo && !window.cropMode; onTriggered: preview.redo() }
            MenuSeparator {}
            MenuItem { text: "选择全部照片"; enabled: window.canBrowse && preview.libraryCount > 0; onTriggered: preview.selectAllPhotos(true) }
            MenuItem { text: "清除照片选择"; enabled: window.canBrowse && preview.selectedCount > 0; onTriggered: preview.selectAllPhotos(false) }
            MenuItem { text: "同步当前设置到所选照片"; enabled: window.canBrowse && preview.hasFrame && preview.selectedCount > 0; onTriggered: preview.syncSelectedSettings(false) }
        }
        Menu {
            title: "查看(&V)"
            MenuItem { text: "胶片与相纸侧栏"; checkable: true; checked: workspace.leftVisible; onTriggered: workspace.leftVisible = !workspace.leftVisible }
            MenuItem { text: "参数侧栏"; checkable: true; checked: workspace.rightVisible; onTriggered: workspace.rightVisible = !workspace.rightVisible }
            MenuItem { text: "照片条"; checkable: true; checked: workspace.stripVisible; onTriggered: workspace.stripVisible = !workspace.stripVisible }
            MenuItem { text: "历史记录"; checkable: true; checked: workspace.historyVisible; onTriggered: window.toggleHistory() }
            MenuItem { text: "恢复默认布局"; onTriggered: { workspace.leftWidth = 254; workspace.rightWidth = 288; workspace.leftVisible = true; workspace.rightVisible = true; workspace.stripVisible = true } }
        }
    }

    component Note: Text {
        Layout.fillWidth: true
        color: Theme.muted
        font.pixelSize: 10
        wrapMode: Text.WordWrap
        lineHeight: 1.35
    }


    RowLayout {
        anchors.fill: parent; spacing: 1
        Rectangle {
            id: leftRail
            visible: workspace.leftVisible
            Layout.preferredWidth: Math.max(232, Math.min(380, workspace.leftWidth)); Layout.fillHeight: true; color: Theme.card
            MaterialPanel { anchors.fill: parent; editor: window; onCollapseRequested: workspace.leftVisible = false }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: Theme.ground
            ColumnLayout {
                anchors.fill: parent; spacing: 0
                EditorToolbar { Layout.fillWidth: true; editor: window }
                Item {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    FrameCanvas { id: canvas; objectName: "frameCanvas"; anchors.fill: parent; anchors.margins: 0; image: window.cropMode ? preview.uncroppedImage : window.showingOriginal ? preview.originalImage : preview.previewImage
                        split: window.splitCompare && !window.cropMode && !window.showingOriginal
                        comparisonImage: window.splitCompare ? preview.originalImage : preview.previewImage
                        enabled: !window.cropMode }
                    Item {
                        id: cropOverlay
                        objectName: "cropOverlay"
                        anchors.fill: canvas
                        visible: window.cropMode
                        readonly property rect imageBox: canvas.imageRect
                        readonly property rect selectionBox: Qt.rect(imageBox.x + window.draftCrop.x * imageBox.width, imageBox.y + window.draftCrop.y * imageBox.height, window.draftCrop.width * imageBox.width, window.draftCrop.height * imageBox.height)
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.imageBox.y; width: cropOverlay.imageBox.width; height: Math.max(0, cropOverlay.selectionBox.y - y); color: "#99000000" }
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.selectionBox.y + cropOverlay.selectionBox.height; width: cropOverlay.imageBox.width; height: Math.max(0, cropOverlay.imageBox.y + cropOverlay.imageBox.height - y); color: "#99000000" }
                        Rectangle { x: cropOverlay.imageBox.x; y: cropOverlay.selectionBox.y; width: Math.max(0, cropOverlay.selectionBox.x - x); height: cropOverlay.selectionBox.height; color: "#99000000" }
                        Rectangle { x: cropOverlay.selectionBox.x + cropOverlay.selectionBox.width; y: cropOverlay.selectionBox.y; width: Math.max(0, cropOverlay.imageBox.x + cropOverlay.imageBox.width - x); height: cropOverlay.selectionBox.height; color: "#99000000" }
                        Rectangle {
                            x: cropOverlay.selectionBox.x; y: cropOverlay.selectionBox.y; width: cropOverlay.selectionBox.width; height: cropOverlay.selectionBox.height
                            color: "transparent"; border.color: Theme.accent; border.width: 1
                            Rectangle { x: parent.width / 3; width: 1; height: parent.height; color: "#70ffffff" }
                            Rectangle { x: parent.width * 2 / 3; width: 1; height: parent.height; color: "#70ffffff" }
                            Rectangle { y: parent.height / 3; height: 1; width: parent.width; color: "#70ffffff" }
                            Rectangle { y: parent.height * 2 / 3; height: 1; width: parent.width; color: "#70ffffff" }
                        }
                        MouseArea {
                            anchors.fill: parent; enabled: window.canEditCrop; cursorShape: Qt.CrossCursor
                            property point origin
                            property bool drawing: false
                            function normalized(x, y) {
                                return Qt.point(Math.max(0, Math.min(1, (x - cropOverlay.imageBox.x) / cropOverlay.imageBox.width)), Math.max(0, Math.min(1, (y - cropOverlay.imageBox.y) / cropOverlay.imageBox.height)))
                            }
                            function updateCrop(x, y) {
                                var p = normalized(x, y)
                                window.draftCrop = Qt.rect(Math.min(origin.x, p.x), Math.min(origin.y, p.y), Math.abs(p.x - origin.x), Math.abs(p.y - origin.y))
                            }
                            onPressed: function(mouse) {
                                var b = cropOverlay.imageBox
                                if (mouse.x < b.x || mouse.y < b.y || mouse.x > b.x + b.width || mouse.y > b.y + b.height) { mouse.accepted = false; return }
                                origin = normalized(mouse.x, mouse.y); drawing = true
                            }
                            onPositionChanged: function(mouse) { if (drawing) updateCrop(mouse.x, mouse.y) }
                            onReleased: function(mouse) {
                                if (!drawing) return
                                updateCrop(mouse.x, mouse.y); drawing = false
                                if (window.draftCrop.width > 0.003 && window.draftCrop.height > 0.003) window.finishCrop()
                                else window.draftCrop = preview.cropRect
                            }
                            onCanceled: { drawing = false; window.draftCrop = preview.cropRect }
                        }
                    }
                    Column {
                        anchors.centerIn: parent; spacing: 16; visible: !preview.hasFrame
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "SpektraLab"; color: "#e5e2dc"; font.pixelSize: 32; font.letterSpacing: 1 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "导入 RAW，逐张选择胶片与相纸"; color: "#d0cdc6"; font.pixelSize: 13 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: "支持一次拖入多张照片"; color: "#c0bdb7"; font.pixelSize: 11 }
                    }
                    DropArea { anchors.fill: parent; enabled: window.canBrowse; onDropped: function(drop) { if (drop.hasUrls) preview.openFiles(drop.urls) } }
                    Histogram {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 26
                        width: 168; height: 64; visible: preview.hasFrame && !window.cropMode
                        channels: preview.histogram
                    }
                    Rectangle {
                        anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 26
                        anchors.topMargin: window.cropMode ? 26 : 94
                        width: window.cropMode ? 78 : 48; height: 20; radius: 2; color: "#b32d2d2c"; visible: preview.hasFrame
                        Text { anchors.centerIn: parent; text: window.cropMode ? "裁切原图" : "FULL"; color: Theme.text; font.pixelSize: 9; font.letterSpacing: 1 }
                    }
                    BusyIndicator { anchors.centerIn: parent; running: preview.busy; visible: running; width: 44; height: 44 }
                    Rectangle {
                        anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter
                        width: 54; height: 16; radius: 3; color: Theme.card; opacity: 0.9
                        Text { anchors.centerIn: parent; text: workspace.stripVisible ? "⌄" : "⌃"; color: Theme.muted; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; onClicked: workspace.stripVisible = !workspace.stripVisible }
                    }
                }
                Rectangle {
                    visible: workspace.stripVisible
                    Layout.fillWidth: true; implicitHeight: Theme.stripHeight; color: Theme.card
                    Rectangle { width: parent.width; height: 1; color: Theme.line; opacity: 0.5 }
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8; spacing: 0
                        ListView {
                            id: filmstrip; objectName: "photoStrip"
                            Layout.fillWidth: true; Layout.fillHeight: true
                            orientation: ListView.Horizontal; spacing: 8; clip: true
                            model: preview.libraryModel; currentIndex: preview.activeIndex
                            onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                            ScrollBar.horizontal: ScrollBar {}
                            delegate: Rectangle {
                                id: photoCell
                                required property var record
                                required property var thumbnailImage
                                readonly property var modelData: record
                                required property int index
                                width: Math.max(65, Math.min(200, (height - 5) * modelData.aspect)); height: filmstrip.height - 3
                                color: Theme.wellDark; border.color: modelData.active ? Theme.text : Theme.line; border.width: modelData.active ? 2 : 0
                                Component.onCompleted: preview.requestThumbnail(index)
                                onRecordChanged: preview.requestThumbnail(index)
                                Text { anchors.centerIn: parent; text: modelData.error ? "无法读取" : "RAW"; color: modelData.error ? "#ffb39f" : Theme.muted; font.pixelSize: 10 }
                                FrameCanvas { objectName: "photoThumbnail"; anchors.fill: parent; anchors.margins: 3; image: photoCell.thumbnailImage; enabled: false }
                                Rectangle {
                                    anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 4
                                    width: 28; height: 14; radius: 2; color: "#b32d2d2c"; visible: modelData.cameraThumbnail
                                    Text { anchors.centerIn: parent; text: "RAW"; color: Theme.text; font.pixelSize: 8 }
                                }
                                MouseArea {
                                    anchors.fill: parent; enabled: window.canBrowse; hoverEnabled: true
                                    onClicked: { window.cropMode = false; preview.selectImage(index) }
                                    ToolTip.visible: containsMouse; ToolTip.text: modelData.error || (modelData.name + (modelData.cameraThumbnail ? " · 相机内嵌缩略图，打开后显示显影结果" : ""))
                                }
                                CheckBox { anchors.top: parent.top; anchors.right: parent.right; padding: 0; scale: 0.8; checked: modelData.selected; enabled: window.canBrowse; onToggled: preview.setItemSelected(index, checked) }
                            }
                        }
                    }
                }
            }
        }
        Rectangle {
            id: rightRail
            visible: workspace.rightVisible
            Layout.preferredWidth: Math.max(280, Math.min(364, workspace.rightWidth)); Layout.fillHeight: true; color: Theme.card
            ProcessPanel { anchors.fill: parent; onCollapseRequested: workspace.rightVisible = false }
        }
    }
    RailHandle {
        objectName: "leftRailHandle"
        visible: workspace.leftVisible
        x: leftRail.x + leftRail.width - width / 2; height: window.contentItem.height; z: 10
        currentWidth: leftRail.width
        onResized: function(value) { workspace.leftWidth = value }
    }
    RailHandle {
        objectName: "rightRailHandle"
        visible: workspace.rightVisible
        x: rightRail.x - width / 2; height: window.contentItem.height; z: 10
        currentWidth: rightRail.width; minimum: 280; maximum: 364; defaultWidth: 288; direction: -1
        onResized: function(value) { workspace.rightWidth = value }
    }
    Popup {
        id: exportPopup
        objectName: "exportPopup"
        anchors.centerIn: parent
        width: 470; padding: 22
        modal: true; focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onVisibleChanged: window.updateDialogState()
        background: Rectangle { color: Theme.card; border.color: Theme.line; radius: 7 }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "导出照片"; color: Theme.text; font.pixelSize: 20; font.bold: true }
            Note { text: "当前画面或所选照片 · sRGB\n裁切与效果一起写入文件。" }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line; opacity: 0.5 }
            RowLayout {
                Text { text: "格式"; color: Theme.text; Layout.preferredWidth: 76 }
                ComboBox { Layout.fillWidth: true; model: ["TIFF · 16 位", "PNG · 8 位", "PNG · 16 位", "JPEG · 8 位"]; currentIndex: window.exportFormat; onActivated: window.exportFormat = currentIndex }
            }
            RowLayout {
                visible: window.exportFormat === 3
                Text { text: "JPEG 质量"; color: Theme.text; Layout.preferredWidth: 76 }
                Slider { Layout.fillWidth: true; from: 1; to: 100; stepSize: 1; value: window.exportQuality; onMoved: window.exportQuality = value }
                Text { text: window.exportQuality; color: Theme.muted; Layout.preferredWidth: 28 }
            }
            RowLayout {
                Text { text: "输出尺寸"; color: Theme.text; Layout.preferredWidth: 76 }
                ComboBox {
                    Layout.fillWidth: true; model: ["原始裁切尺寸", "长边 3840 像素", "长边 2560 像素", "长边 1920 像素", "长边 1080 像素"]
                    readonly property var sizes: [0, 3840, 2560, 1920, 1080]
                    currentIndex: Math.max(0, sizes.indexOf(window.exportLongEdge))
                    onActivated: window.exportLongEdge = sizes[currentIndex]
                }
            }
            Note { text: "保持长宽比例，不放大图像。PNG 无损保存，JPEG 为有损压缩。" }
            Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.line; opacity: 0.5 }
            ActionButton { Layout.fillWidth: true; accented: true; text: "导出当前已显示画面…"; enabled: preview.hasFrame && !preview.busy; onClicked: { saveDialog.open(); exportPopup.close() } }
            ActionButton { Layout.fillWidth: true; text: "导出所选 " + preview.selectedCount + " 张到文件夹…"; enabled: preview.selectedCount > 0 && !preview.busy; onClicked: { batchDialog.open(); exportPopup.close() } }
            Note { text: "当前画面导出不覆盖已有文件。批量导出逐张处理，自动跳过同名文件；未访问的照片也按各自设置渲染。" }
            ActionButton { Layout.alignment: Qt.AlignRight; text: "取消"; onClicked: exportPopup.close() }
        }
    }
}
