import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: table
    objectName: "lighttableWindow"
    required property var board
    property bool hasCurrent: false
    signal sendCurrent(string format)
    title: "观片台 — SpektraLab Windows"
    width: 1280; height: 860; minimumWidth: 900; minimumHeight: 640
    color: Theme.card
    font.family: "Segoe UI"
    font.pixelSize: 12
    palette.window: Theme.card
    palette.windowText: Theme.text
    palette.text: Theme.text
    palette.buttonText: Theme.text
    palette.base: Theme.well
    palette.button: Theme.well
    palette.highlight: Theme.accent
    property string mode: "drag"
    property string held: ""
    property bool loupe: false
    property real zoom: 4
    property real fitZoom: 4
    property real panX: 450
    property real panY: 300
    property url loupeSource
    property real loupeU: 0.5
    property real loupeV: 0.5
    property real loupePhotoWidth: 300
    property real loupeRatio: 1.5
    property real loupeX: 0
    property real loupeY: 0
    readonly property var picked: {board.selected;return board.photo(board.selected)}
    function fitBoard(){var b=board.bounds();zoom=Math.max(0.1,Math.min(18,(stage.width-100)/b.width,(stage.height-100)/b.height));fitZoom=zoom;panX=stage.width/2-(b.x+b.width/2)*zoom;panY=stage.height/2-(b.y+b.height/2)*zoom}
    function zoomAt(x,y,factor){var u=(x-panX)/zoom,v=(y-panY)/zoom;zoom=Math.max(0.1,Math.min(40,zoom*factor));panX=x-u*zoom;panY=y-v*zoom}
    function addCurrent(){if(hasCurrent)sendCurrent(format.currentValue)}
    onVisibleChanged: if(visible)Qt.callLater(fitBoard)
    onClosing: board.flush()
    Connections {target: board;function onCountChanged(){Qt.callLater(table.fitBoard)}}
    FileDialog {id: addDialog; title: "添加导出的照片";fileMode: FileDialog.OpenFiles;nameFilters:["图像 (*.png *.jpg *.jpeg *.bmp)"];onAccepted:board.addFiles(selectedFiles,format.currentValue)}
    FileDialog {id: saveDialog;title:"保存观片台图片（不覆盖已有文件）";fileMode:FileDialog.SaveFile;defaultSuffix:"png";nameFilters:["PNG 图片 (*.png)"];onAccepted:board.saveBoard(selectedFile)}
    Shortcut {sequence:"D";enabled:table.active;onActivated:table.mode="drag"}
    Shortcut {sequence:"C";enabled:table.active;onActivated:table.mode="click"}
    Shortcut {sequence:"L";enabled:table.active;onActivated:table.loupe=!table.loupe}
    Shortcut {sequence:"0";enabled:table.active;onActivated:table.fitBoard()}
    Shortcut {sequence:"Delete";enabled:table.active&&!board.busy;onActivated:board.remove(board.selected)}
    Shortcut {sequence:"Escape";enabled:table.active;onActivated:{table.held="";table.loupe=false}}
    ColumnLayout {
        anchors.fill:parent;spacing:1
        Rectangle {
            Layout.fillWidth:true;implicitHeight:48;color:Theme.card
            RowLayout {
                anchors.fill:parent;anchors.margins:8;spacing:7
                Text {text:"观片台";color:Theme.text;font.pixelSize:16;font.bold:true}
                ActionButton {text:"拖动";accented:table.mode==="drag";onClicked:{table.mode="drag";table.held=""}}
                ActionButton {text:"点击移动";accented:table.mode==="click";onClicked:table.mode="click"}
                ActionButton {text:"放大镜";accented:table.loupe;onClicked:table.loupe=!table.loupe}
                ActionButton {text:"−";onClicked:table.zoomAt(stage.width/2,stage.height/2,1/1.2)}
                ActionButton {text:Math.round(table.zoom/table.fitZoom*100)+"%";onClicked:table.fitBoard()}
                ActionButton {text:"+";onClicked:table.zoomAt(stage.width/2,stage.height/2,1.2)}
                ActionButton {text:"适合窗口";onClicked:table.fitBoard()}
                Item {Layout.fillWidth:true}
                ActionButton {text:"复制图片";enabled:board.count>0&&!board.busy;onClicked:board.copyBoard()}
                ActionButton {text:"保存图片…";enabled:board.count>0&&!board.busy;onClicked:saveDialog.open()}
            }
        }
        Item {
            id:stage;objectName:"lighttableStage";Layout.fillWidth:true;Layout.fillHeight:true;clip:true
            Rectangle {anchors.fill:parent;color:"#d9ddd2"}
            Rectangle {anchors.fill:parent;anchors.margins:18;color:"#f0f3e9";border.color:"#c6ccbf";border.width:2;radius:8}
            MouseArea {
                anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.MiddleButton;hoverEnabled:true
                property point pressPoint
                property point pressPan
                onPressed:function(mouse){pressPoint=Qt.point(mouse.x,mouse.y);pressPan=Qt.point(table.panX,table.panY)}
                onPositionChanged:function(mouse){
                    if(table.held.length&&table.mode==="click")board.move(table.held,(mouse.x-table.panX)/table.zoom,(mouse.y-table.panY)/table.zoom)
                    else if(pressed){table.panX=pressPan.x+mouse.x-pressPoint.x;table.panY=pressPan.y+mouse.y-pressPoint.y}
                }
                onClicked:{table.held="";board.select("")}
                onExited:table.loupeSource=""
                onWheel:function(wheel){table.zoomAt(wheel.x,wheel.y,Math.pow(1.2,wheel.angleDelta.y/120));wheel.accepted=true}
            }
            Item {
                id:world;objectName:"lighttableWorld";x:table.panX;y:table.panY;scale:table.zoom;transformOrigin:Item.TopLeft
                Repeater {
                    model:board
                    delegate:Item {
                        id:card
                        required property string cardId
                        required property string caption
                        required property url sourceUrl
                        required property real dimW
                        required property real dimH
                        required property real posX
                        required property real posY
                        required property real angle
                        required property real cardScale
                        required property int zOrder
                        objectName:"lighttableCard_"+cardId
                        x:posX-width/2;y:posY-height/2;width:dimW*cardScale;height:dimH*cardScale;rotation:angle;z:zOrder
                        Rectangle {x:1;y:1;width:parent.width;height:parent.height;color:"#30000000"}
                        Image {anchors.fill:parent;source:card.sourceUrl;sourceSize:Qt.size(1024,1024);asynchronous:true;mipmap:true;smooth:true}
                        Rectangle {anchors.fill:parent;anchors.margins:-1/table.zoom;color:"transparent";border.color:Theme.accent;border.width:1.5/table.zoom;visible:board.selected===card.cardId}
                        MouseArea {
                            anchors.fill:parent;hoverEnabled:true;enabled:!board.busy;acceptedButtons:Qt.LeftButton|Qt.RightButton
                            cursorShape:table.loupe?Qt.CrossCursor:pressed?Qt.ClosedHandCursor:Qt.OpenHandCursor
                            property point pressPoint
                            property point origin
                            onPressed:function(mouse){board.select(card.cardId);pressPoint=mapToItem(stage,mouse.x,mouse.y);origin=Qt.point(card.posX,card.posY);if(mouse.button===Qt.RightButton)cardMenu.popup()}
                            onPositionChanged:function(mouse){
                                var p=mapToItem(stage,mouse.x,mouse.y)
                                if(table.loupe){table.loupeSource=card.sourceUrl;table.loupeU=mouse.x/width;table.loupeV=mouse.y/height;table.loupePhotoWidth=card.width*table.zoom;table.loupeRatio=card.dimW/card.dimH;table.loupeX=Math.min(stage.width-206,p.x+20);table.loupeY=Math.min(stage.height-206,p.y+20)}
                                else if(table.mode==="click"&&table.held===card.cardId)board.move(card.cardId,(p.x-table.panX)/table.zoom,(p.y-table.panY)/table.zoom)
                                else if(pressed&&table.mode==="drag")board.move(card.cardId,origin.x+(p.x-pressPoint.x)/table.zoom,origin.y+(p.y-pressPoint.y)/table.zoom)
                            }
                            onClicked:function(mouse){if(mouse.button===Qt.LeftButton&&table.mode==="click"&&!table.loupe)table.held=table.held===card.cardId?"":card.cardId}
                            onExited:table.loupeSource=""
                            onWheel:function(wheel){var p=mapToItem(stage,wheel.x,wheel.y);table.zoomAt(p.x,p.y,Math.pow(1.2,wheel.angleDelta.y/120));wheel.accepted=true}
                            ToolTip.visible:containsMouse&&!table.loupe;ToolTip.text:card.caption+"\n"+(card.dimW*card.cardScale).toFixed(1)+" × "+(card.dimH*card.cardScale).toFixed(1)+" mm"
                        }
                        Rectangle {
                            anchors.right:parent.right;anchors.bottom:parent.bottom;width:8/table.zoom;height:8/table.zoom;color:Theme.accent;visible:board.selected===card.cardId&&!table.loupe
                            MouseArea {
                                anchors.fill:parent;cursorShape:Qt.SizeFDiagCursor;enabled:!board.busy
                                property real startDistance
                                property real startScale
                                onPressed:function(mouse){var p=mapToItem(stage,mouse.x,mouse.y);startDistance=Math.max(1,Math.hypot(p.x-(card.posX*table.zoom+table.panX),p.y-(card.posY*table.zoom+table.panY)));startScale=card.cardScale}
                                onPositionChanged:function(mouse){if(!pressed)return;var p=mapToItem(stage,mouse.x,mouse.y),d=Math.hypot(p.x-(card.posX*table.zoom+table.panX),p.y-(card.posY*table.zoom+table.panY));board.resize(card.cardId,startScale*d/startDistance)}
                            }
                        }
                        Menu {id:cardMenu;MenuItem{text:"旋转 90°";onTriggered:board.rotate(card.cardId,card.angle+90)}MenuItem{text:"恢复实际尺寸";onTriggered:board.resize(card.cardId,1)}MenuItem{text:"移出观片台";onTriggered:board.remove(card.cardId)}}
                    }
                }
            }
            Text {anchors.centerIn:parent;visible:board.count===0;text:"添加导出的照片，或从编辑器发送当前显影结果\n按胶片毫米尺寸比较，照片只保存在本机";color:"#6d7566";font.pixelSize:16;horizontalAlignment:Text.AlignHCenter}
            Rectangle {
                x:table.loupeX;y:table.loupeY;width:200;height:200;z:100000;clip:true;visible:table.loupe&&table.loupeSource.toString().length>0;color:"#262821";border.width:4;border.color:"#454b40"
                Image {source:table.loupeSource;width:Math.max(200,table.loupePhotoWidth*3);height:width/table.loupeRatio;x:100-table.loupeU*width;y:100-table.loupeV*height;smooth:true}
                Rectangle {anchors.fill:parent;color:"transparent";border.width:4;border.color:"#454b40"}
                Text {anchors.right:parent.right;anchors.bottom:parent.bottom;anchors.margins:7;text:"3×";color:"white";style:Text.Outline;styleColor:"black"}
            }
            BusyIndicator {anchors.centerIn:parent;running:board.busy;visible:running}
            DropArea {anchors.fill:parent;enabled:!board.busy;onDropped:function(drop){if(drop.hasUrls)board.addFiles(drop.urls,format.currentValue)}}
        }
        Rectangle {
            Layout.fillWidth:true;implicitHeight:98;color:Theme.card
            ColumnLayout {
                anchors.fill:parent;anchors.margins:10;spacing:7
                RowLayout {
                    Text {text:"导入画幅";color:Theme.text}
                    ComboBox {id:format;objectName:"lighttableFormat";Layout.preferredWidth:200;model:board.formats;textRole:"label";valueRole:"value";font.pixelSize:11}
                    ActionButton {text:"添加图片…";enabled:!board.busy;onClicked:addDialog.open()}
                    ActionButton {objectName:"lighttableSendCurrent";text:"加入当前显影结果";enabled:table.hasCurrent&&!board.busy;onClicked:table.addCurrent()}
                    ActionButton {text:"铺开";enabled:board.count>0&&!board.busy;onClicked:{board.arrange();table.fitBoard()}}
                    ActionButton {text:"清空";enabled:board.count>0&&!board.busy;onClicked:clearDialog.open()}
                    Item {Layout.fillWidth:true}
                    ActionButton {text:"↶ 15°";enabled:board.selected.length>0&&!board.busy;onClicked:board.rotate(board.selected,Number(table.picked.angle)-15)}
                    ActionButton {text:"↷ 15°";enabled:board.selected.length>0&&!board.busy;onClicked:board.rotate(board.selected,Number(table.picked.angle)+15)}
                    ActionButton {text:"移出";enabled:board.selected.length>0&&!board.busy;onClicked:board.remove(board.selected)}
                }
                Text {Layout.fillWidth:true;elide:Text.ElideRight;color:Theme.muted;font.pixelSize:11;text:board.message.length?board.message:board.count+" 张 · 滚轮缩放，拖空白处平移；拖照片右下角缩放，右键旋转／移出。只保存本机副本，不上传。"}
            }
        }
    }
    Dialog {id:clearDialog;title:"清空观片台？";modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Yes|Dialog.No;onAccepted:board.clear();Label{text:"只移除观片台的缓存副本，不删除原照片。"}}
}
