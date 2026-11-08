import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
ColumnLayout {
    id: editor
    objectName: "curveEditor"
    property string channel: "rgb"
    property string serialized: preview.adjustments.post_curves || "{}"
    property var document: JSON.parse(serialized)
    readonly property var points: document[channel] || [[0,0],[1,1]]
    property int picked: -1
    Layout.fillWidth: true; spacing: 5
    function commit(points) { var next=JSON.parse(serialized);next[channel]=points;preview.setAdjustment("post_curves",JSON.stringify(next)) }
    function evaluate(points,x) {
        var n=points.length,d=[],m=[]
        for(var j=0;j<n-1;++j)d[j]=(points[j+1][1]-points[j][1])/Math.max(points[j+1][0]-points[j][0],0.000001)
        m[0]=d[0];m[n-1]=d[n-2]
        for(var k=1;k<n-1;++k)m[k]=d[k-1]*d[k]<=0?0:(d[k-1]+d[k])/2
        for(var a=0;a<n-1;++a)if(d[a]!==0){var aa=m[a]/d[a],b=m[a+1]/d[a],s=aa*aa+b*b;if(s>9){var q=3/Math.sqrt(s);m[a]=q*aa*d[a];m[a+1]=q*b*d[a]}}
        var i=0;while(i<n-2&&x>points[i+1][0])++i
        var h=Math.max(points[i+1][0]-points[i][0],0.000001),t=(x-points[i][0])/h,t2=t*t,t3=t2*t
        return Math.max(0,Math.min(1,(2*t3-3*t2+1)*points[i][1]+(t3-2*t2+t)*h*m[i]+(-2*t3+3*t2)*points[i+1][1]+(t3-t2)*h*m[i+1]))
    }
    RowLayout {
        Layout.fillWidth: true; spacing: 3
        Repeater { model: [{key:"rgb",name:"RGB"},{key:"luma",name:"亮度"},{key:"r",name:"R"},{key:"g",name:"G"},{key:"b",name:"B"}]
            ActionButton { required property var modelData; Layout.fillWidth: true; text: modelData.name; accented: editor.channel===modelData.key; outlined: true; implicitHeight: 19; font.pixelSize: 10; onClicked: editor.channel=modelData.key }
        }
    }
    Rectangle {
        Layout.fillWidth: true; implicitHeight: width/1.05; color: "#1e1f1e"
        Canvas {
            id: chart; anchors.fill: parent
            property var curvePoints: editor.points
            onCurvePointsChanged: requestPaint()
            onWidthChanged: requestPaint()
            onPaint: {
                var c=getContext("2d");c.clearRect(0,0,width,height)
                c.strokeStyle="#3a3b39";c.lineWidth=1
                for(var g=1;g<4;++g){c.beginPath();c.moveTo(g*width/4,0);c.lineTo(g*width/4,height);c.moveTo(0,g*height/4);c.lineTo(width,g*height/4);c.stroke()}
                c.strokeStyle="#565753";c.beginPath();c.moveTo(0,height);c.lineTo(width,0);c.stroke()
                c.strokeStyle=editor.channel==="r"?"#e8524e":editor.channel==="g"?"#62c462":editor.channel==="b"?"#5d7de8":"#f9f7f3"
                c.beginPath();for(var i=0;i<256;++i){var x=i/255*width,y=(1-editor.evaluate(editor.points,i/255))*height;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}c.stroke()
                for(var j=0;j<editor.points.length;++j){c.beginPath();c.arc(editor.points[j][0]*width,(1-editor.points[j][1])*height,3.5,0,Math.PI*2);c.fillStyle="#2d2d2c";c.fill();c.stroke()}
            }
        }
        MouseArea {
            anchors.fill: parent; acceptedButtons: Qt.LeftButton|Qt.RightButton
            enabled: preview.settingsEditable
            function nearest(mouse) {var best=-1,dist=100;for(var i=0;i<editor.points.length;++i){var dx=mouse.x-editor.points[i][0]*width,dy=mouse.y-(1-editor.points[i][1])*height,d=dx*dx+dy*dy;if(d<dist){dist=d;best=i}}return best}
            function movePoint(mouse) {
                if(editor.picked<0)return
                var points=JSON.parse(JSON.stringify(editor.points)),i=editor.picked
                var x=i===0?0:i===points.length-1?1:Math.max(points[i-1][0]+0.01,Math.min(points[i+1][0]-0.01,mouse.x/width))
                points[i]=[x,Math.max(0,Math.min(1,1-mouse.y/height))];editor.commit(points)
            }
            onPressed: function(mouse) {
                editor.picked=nearest(mouse)
                if(mouse.button===Qt.RightButton){if(editor.picked>0&&editor.picked<editor.points.length-1){var removed=JSON.parse(JSON.stringify(editor.points));removed.splice(editor.picked,1);editor.commit(removed)}editor.picked=-1;return}
                preview.beginEditGesture("post_curves")
                if(editor.picked<0&&editor.points.length<32){var x=Math.max(0.01,Math.min(0.99,mouse.x/width)),points=JSON.parse(JSON.stringify(editor.points)),index=1;while(index<points.length-1&&points[index][0]<x)++index
                    if(x-points[index-1][0]>=0.01&&points[index][0]-x>=0.01){points.splice(index,0,[x,Math.max(0,Math.min(1,1-mouse.y/height))]);editor.picked=index;editor.commit(points)}
                }else movePoint(mouse)
            }
            onPositionChanged: function(mouse) {if(pressed)movePoint(mouse)}
            onReleased: {editor.picked=-1;preview.endEditGesture()}
            onCanceled: {editor.picked=-1;preview.endEditGesture()}
            onDoubleClicked: {editor.commit([[0,0],[1,1]]);editor.picked=-1;preview.endEditGesture()}
        }
    }
    Text { text: "点击添加 · 拖动调整 · 右键删除 · 双击重置"; color: Theme.muted; font.pixelSize: 9 }
}
