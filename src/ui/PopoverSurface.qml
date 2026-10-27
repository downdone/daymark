pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects

Item {
    id: root
    property point sourcePoint: Qt.point(-1,-1)
    property bool attached: false
    readonly property real cornerRadius: Theme.panelRadius
    readonly property bool alignedVertically: sourcePoint.y>=24 && sourcePoint.y<=height-24
    readonly property bool alignedHorizontally: sourcePoint.x>=24 && sourcePoint.x<=width-24
    readonly property string arrowEdge: !attached ? "" : sourcePoint.x<=0 && alignedVertically ? "left" : sourcePoint.x>=width && alignedVertically ? "right" : sourcePoint.y<=0 && alignedHorizontally ? "top" : sourcePoint.y>=height && alignedHorizontally ? "bottom" : ""
    readonly property real arrowOffset: Math.max(24,Math.min(arrowEdge==="left" || arrowEdge==="right" ? height-24 : width-24,arrowEdge==="left" || arrowEdge==="right" ? sourcePoint.y : sourcePoint.x))
    RectangularShadow {
        anchors.fill: parent;radius: root.cornerRadius;blur: 20;spread: 1
        offset: Qt.vector2d(0,5)
        color: Qt.rgba(0,0,0,Theme.dark ? 0.32 : 0.13)
        visible: !Theme.highContrast
    }
    Rectangle {objectName:"popoverBody";anchors.fill:parent;radius:root.cornerRadius;color:Theme.content;border.width:1;border.color:Theme.separator}
    Canvas {
        id:arrowCanvas
        visible:root.arrowEdge.length>0
        width:root.arrowEdge==="left" || root.arrowEdge==="right" ? 12 : 22
        height:root.arrowEdge==="left" || root.arrowEdge==="right" ? 22 : 12
        x:root.arrowEdge==="left" ? -11 : root.arrowEdge==="right" ? root.width-1 : root.arrowOffset-11
        y:root.arrowEdge==="top" ? -11 : root.arrowEdge==="bottom" ? root.height-1 : root.arrowOffset-11
        property color fillColor:Theme.content
        property color lineColor:Theme.separator
        onFillColorChanged:requestPaint()
        onLineColorChanged:requestPaint()
        onVisibleChanged:requestPaint()
        Connections {target:root;function onArrowEdgeChanged(){arrowCanvas.requestPaint()}}
        onPaint:{
            const c=getContext("2d");c.reset();c.fillStyle=fillColor;c.strokeStyle=lineColor;c.lineWidth=1;c.beginPath();
            if(root.arrowEdge==="left"){c.moveTo(12,0);c.lineTo(1,11);c.lineTo(12,22);}
            else if(root.arrowEdge==="right"){c.moveTo(0,0);c.lineTo(11,11);c.lineTo(0,22);}
            else if(root.arrowEdge==="top"){c.moveTo(0,12);c.lineTo(11,1);c.lineTo(22,12);}
            else {c.moveTo(0,0);c.lineTo(11,11);c.lineTo(22,0);}
            c.fill();c.stroke();
        }
    }
}
