import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
ColumnLayout {
    id: wheel
    property string zone: "master"
    property string label: "主控"
    property real diameter: 170
    readonly property real hue: Number(preview.adjustments["post_"+zone+"_hue"] || 0)
    readonly property real saturation: Number(preview.adjustments["post_"+zone+"_saturation"] || 0)
    readonly property real luminance: Number(preview.adjustments["post_"+zone+"_luminance"] || 0)
    spacing: 4; Layout.alignment: Qt.AlignHCenter
    RowLayout {
        spacing: 5
        Item {
            width: wheel.diameter; height: wheel.diameter
            Canvas {
                id: disc; anchors.fill: parent
                onWidthChanged: requestPaint()
                onPaint: {
                    var c=getContext("2d"),r=Math.min(width,height)/2-2,cx=width/2,cy=height/2
                    c.clearRect(0,0,width,height)
                    for(var i=0;i<72;++i){var h=i*5,a=-h*Math.PI/180,b=-(h+5.2)*Math.PI/180,g=c.createRadialGradient(cx,cy,0,cx,cy,r);g.addColorStop(0,"#d9d9d5");g.addColorStop(1,Qt.hsla(h/360,0.75,0.50,1));c.fillStyle=g;c.beginPath();c.moveTo(cx,cy);c.arc(cx,cy,r,a,b,true);c.closePath();c.fill()}
                    c.strokeStyle="#5f5f5f";c.lineWidth=1;c.beginPath();c.arc(cx,cy,r,0,Math.PI*2);c.stroke()
                }
            }
            Rectangle { width: 7; height: 7; radius: 4; color: "#333333"; border.width: 1; border.color: Theme.text; x: parent.width/2+Math.cos(wheel.hue*Math.PI/180)*wheel.saturation*(parent.width/2-2)-width/2; y: parent.height/2-Math.sin(wheel.hue*Math.PI/180)*wheel.saturation*(parent.height/2-2)-height/2 }
            MouseArea {
                anchors.fill: parent; enabled: preview.settingsEditable
                function update(mouse){var x=(mouse.x-width/2)/(width/2-2),y=-(mouse.y-height/2)/(height/2-2);var h=(Math.atan2(y,x)*180/Math.PI+360)%360;preview.setAdjustment("post_"+wheel.zone+"_hue",h);preview.setAdjustment("post_"+wheel.zone+"_saturation",Math.min(1,Math.sqrt(x*x+y*y)))}
                onPressed: function(mouse){preview.beginEditGesture("post_"+wheel.zone);update(mouse)}
                onPositionChanged: function(mouse){if(pressed)update(mouse)}
                onReleased: preview.endEditGesture()
                onCanceled: preview.endEditGesture()
                onDoubleClicked: {preview.setAdjustment("post_"+wheel.zone+"_saturation",0);preview.setAdjustment("post_"+wheel.zone+"_luminance",0);preview.endEditGesture()}
            }
        }
        Slider { id: light; orientation: Qt.Vertical; implicitHeight: wheel.diameter; implicitWidth: 12; from: -1; to: 1; value: wheel.luminance; stepSize: 0.01; enabled: preview.settingsEditable
            onMoved: preview.setAdjustment("post_"+wheel.zone+"_luminance",value)
            onPressedChanged: {if(pressed)preview.beginEditGesture("post_"+wheel.zone+"_luminance");else preview.endEditGesture()}
            background: Rectangle { x: 5; y: 0; width: 2; height: light.height; gradient: Gradient {GradientStop{position:0;color:"white"}GradientStop{position:1;color:"#111111"}} }
            handle: Rectangle {x:1;y:light.visualPosition*(light.height-height);width:10;height:4;radius:2;color:Theme.text}
        }
    }
    Text { Layout.alignment: Qt.AlignHCenter; text: wheel.label; color: Theme.text; font.pixelSize: 10 }
    Text { Layout.alignment: Qt.AlignHCenter; text: wheel.hue.toFixed(0)+"° · "+(wheel.saturation*100).toFixed(0)+"% · "+wheel.luminance.toFixed(2); color: Theme.muted; font.pixelSize: 9 }
}
