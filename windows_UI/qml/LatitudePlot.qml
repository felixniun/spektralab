import QtQuick
import QtQuick.Layouts
ColumnLayout {
    id: plot
    property var analysis: preview.latitude
    readonly property var medium: analysis.medium || ({})
    readonly property var histogram: (analysis.scene || {}).histogram || ({})
    readonly property var fractions: histogram.placed_fractions || histogram.fractions || []
    readonly property bool valid: typeof medium.shadow_ev === "number" && typeof medium.highlight_ev === "number"
    Layout.fillWidth: true; spacing: 2
    function statistics() {
        var below=0,above=0
        var lo=histogram.lo_ev || -24, hi=histogram.hi_ev || 24
        for(var i=0;i<fractions.length;++i){var ev=lo+(i+0.5)*(hi-lo)/fractions.length;if(ev<medium.shadow_ev)below+=fractions[i];else if(ev>medium.highlight_ev)above+=fractions[i]}
        return [below,Math.max(0,1-below-above),above]
    }
    Rectangle {
        Layout.fillWidth: true; implicitHeight: 68; color: "#1e1f1e"
        Canvas {
            id: chart; anchors.fill: parent
            property var analysisValue: plot.analysis
            onAnalysisValueChanged: requestPaint()
            onWidthChanged: requestPaint()
            onPaint: {
                var c=getContext("2d");c.clearRect(0,0,width,height)
                if(!plot.valid)return
                var fractions=plot.fractions,lo=plot.histogram.lo_ev,hi=plot.histogram.hi_ev,peak=0
                for(var i=0;i<fractions.length;++i)peak=Math.max(peak,fractions[i])
                var left=(plot.medium.shadow_ev+8)/16*width,right=(plot.medium.highlight_ev+8)/16*width
                c.fillStyle="#41423f";c.fillRect(left,0,right-left,height)
                for(var j=0;j<fractions.length;++j){var ev=lo+(j+0.5)*(hi-lo)/fractions.length
                    if(ev< -8||ev>8)continue
                    var x=(ev+8)/16*width,h=peak?fractions[j]/peak*height:0
                    c.fillStyle=ev>=plot.medium.shadow_ev&&ev<=plot.medium.highlight_ev?"#b4b4b0":"#666762"
                    c.fillRect(x,height-h,Math.max(1,(hi-lo)/fractions.length/16*width),h)
                }
                c.strokeStyle="#dad8d4";c.lineWidth=1;c.beginPath();c.moveTo(left,0);c.lineTo(left,height);c.moveTo(right,0);c.lineTo(right,height);c.stroke()
            }
        }
        Text { visible: !plot.valid; anchors.centerIn: parent; text: preview.hasFrame ? "此设置无法测量宽容度" : "导入照片后测量"; color: Theme.muted; font.pixelSize: 10 }
    }
    RowLayout { Layout.fillWidth: true; Repeater { model: ["−8","−6","−4","−2","0","+2","+4","+6","+8"]; Text { required property string modelData; text: modelData; color: Theme.muted; font.pixelSize: 9; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter } } }
    Text { Layout.fillWidth: true; color: Theme.text; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter; text: {if(!plot.valid)return "";var s=plot.statistics();return "低于 "+(s[0]*100).toFixed(1)+"% · 范围内 "+(s[1]*100).toFixed(1)+"% · 高于 "+(s[2]*100).toFixed(1)+"%"} }
    Text { Layout.fillWidth: true; text: plot.valid ? Number(plot.medium.latitude_stops).toFixed(2)+" 档保留 · "+Number(plot.medium.shadow_ev).toFixed(2)+" → +"+Number(plot.medium.highlight_ev).toFixed(2) : ""; color: Theme.muted; font.pixelSize: 9; horizontalAlignment: Text.AlignHCenter }
}
