import QtQuick
Canvas {
    id: icon
    objectName: "daymarkIcon"
    property string name: "circle"
    property color ink: Theme.secondary
    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize
    readonly property real drawingSize: Math.min(width,height,Theme.iconSize)
    onDrawingSizeChanged: requestPaint()
    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const p=getContext("2d")
        p.reset();p.translate((width-drawingSize)/2,(height-drawingSize)/2);p.scale(drawingSize/24,drawingSize/24);p.strokeStyle=ink;p.fillStyle=ink;p.lineWidth=1.6;p.lineCap="round";p.lineJoin="round"
        function line(points,closed) {p.beginPath();p.moveTo(points[0][0],points[0][1]);for(let i=1;i<points.length;i++)p.lineTo(points[i][0],points[i][1]);if(closed)p.closePath();p.stroke()}
        function circle(x,y,r) {p.beginPath();p.arc(x,y,r,0,Math.PI*2);p.stroke()}
        if(name==="inbox") {line([[4,4],[20,4],[22,17],[22,20],[2,20],[2,17]],true);line([[3,14],[8,14],[10,17],[14,17],[16,14],[21,14]],false)}
        else if(name==="today") {circle(12,12,4);for(let i=0;i<8;i++){const a=i*Math.PI/4;line([[12+7*Math.cos(a),12+7*Math.sin(a)],[12+10*Math.cos(a),12+10*Math.sin(a)]],false)}}
        else if(name==="calendar" || name==="upcoming") {p.strokeRect(3,5,18,16);line([[3,10],[21,10]],false);line([[8,3],[8,7]],false);line([[16,3],[16,7]],false);if(name==="upcoming")line([[10,13],[14,13],[14,17]],false)}
        else if(name==="cursor")line([[4,3],[4,20],[9,15],[13,22],[17,20],[13,13],[21,13]],true)
        else if(name==="lock") {p.strokeRect(5,10,14,11);p.beginPath();p.arc(12,10,5,Math.PI,0);p.stroke();line([[12,14],[12,17]],false)}
        else if(name==="bell") {line([[4,17],[6,14],[6,9]],false);p.beginPath();p.arc(12,9,6,Math.PI,0);p.stroke();line([[18,9],[18,14],[20,17],[4,17]],false);p.beginPath();p.arc(12,19,2,0,Math.PI);p.stroke()}
        else if(name==="clock") {circle(12,12,9);line([[12,6],[12,12],[17,15]],false)}
        else if(name==="moon") {p.beginPath();p.moveTo(16,3);p.bezierCurveTo(4,0,0,17,11,21);p.bezierCurveTo(16,23,22,19,22,14);p.bezierCurveTo(12,19,7,8,16,3);p.closePath();p.stroke()}
        else if(name==="someday") {p.beginPath();p.arc(12,12,9,Math.PI*0.15,Math.PI*1.65);p.stroke();line([[16,3],[19,3],[19,6]],false)}
        else if(name==="logbook" || name==="check")line([[5,12],[10,17],[20,6]],false)
        else if(name==="area") {p.strokeRect(3,3,7,7);p.strokeRect(14,3,7,7);p.strokeRect(3,14,7,7);p.strokeRect(14,14,7,7)}
        else if(name==="folder" || name==="project")line([[3,7],[3,20],[21,20],[21,6],[12,6],[10,3],[3,3]],true)
        else if(name==="tag") {line([[9,3],[6,21]],false);line([[18,3],[15,21]],false);line([[4,9],[21,9]],false);line([[3,15],[20,15]],false)}
        else if(name==="info") {circle(12,12,9);line([[12,10],[12,17]],false);circle(12,6.5,0.5)}
        else if(name==="flag") {line([[5,22],[5,3],[12,3],[14,5],[21,5],[21,15],[14,15],[12,13],[5,13]],false)}
        else if(name==="grip") {line([[5,8],[19,8]],false);line([[5,12],[19,12]],false);line([[5,16],[19,16]],false)}
        else if(name==="more") {for(let i=0;i<3;i++){p.beginPath();p.arc(5+i*7,12,1.5,0,Math.PI*2);p.fill()}}
        else if(name==="left")line([[15,5],[8,12],[15,19]],false)
        else if(name==="right")line([[9,5],[16,12],[9,19]],false)
        else if(name==="up")line([[5,15],[12,8],[19,15]],false)
        else if(name==="collapse" || name==="down")line([[5,9],[12,16],[19,9]],false)
        else if(name==="plus") {line([[5,12],[19,12]],false);line([[12,5],[12,19]],false)}
        else if(name==="expand") {line([[5,19],[19,5]],false);line([[9,5],[19,5],[19,15]],false)}
        else if(name==="pin") {line([[8,3],[16,3],[15,10],[19,14],[5,14],[9,10]],true);line([[12,14],[12,22]],false)}
        else if(name==="search") {circle(10.5,10.5,6.5);line([[15.5,15.5],[21,21]],false)}
        else if(name==="filter") {line([[4,6],[20,6]],false);line([[7,12],[17,12]],false);line([[10,18],[14,18]],false)}
        else if(name==="settings") {p.beginPath();for(let i=0;i<32;i++){const a=i*Math.PI/16;const r=i%4<2?9:7;const x=12+r*Math.cos(a),y=12+r*Math.sin(a);if(i===0)p.moveTo(x,y);else p.lineTo(x,y)}p.closePath();p.stroke();circle(12,12,3)}
        else if(name==="close") {line([[6,6],[18,18]],false);line([[18,6],[6,18]],false)}
        else if(name==="sidebar") {p.strokeRect(3,4,18,16);line([[9,4],[9,20]],false)}
        else if(name==="trash") {line([[4,6],[20,6]],false);line([[9,3],[15,3]],false);line([[6,6],[7,21],[17,21],[18,6]],false);line([[10,10],[10,17]],false);line([[14,10],[14,17]],false)}
        else if(name==="edit") {line([[4,16],[4,21],[9,20],[21,8],[16,3],[4,16]],false);line([[13,6],[18,11]],false)}
        else if(name==="reload" || name==="reset") {p.beginPath();p.arc(12,12,8,0.6,5.4);p.stroke();line([[16,3],[18,7],[13,7]],false)}
        else if(name==="window") {p.strokeRect(3,4,18,16);line([[3,9],[21,9]],false);circle(6,6.5,0.6)}
        else if(name==="subtasks") {line([[4,4],[12,4]],false);line([[6,8],[6,19],[11,19]],false);line([[6,12],[11,12]],false);line([[15,12],[21,12]],false);line([[15,19],[21,19]],false)}
        else if(name==="list" || name==="batch") {for(let i=0;i<3;i++){p.strokeRect(3,4+6*i,3,3);line([[10,5.5+6*i],[21,5.5+6*i]],false)}}
        else if(name==="export") {line([[4,14],[4,21],[20,21],[20,14]],false);line([[12,16],[12,3]],false);line([[7,8],[12,3],[17,8]],false)}
        else if(name==="import") {line([[4,14],[4,21],[20,21],[20,14]],false);line([[12,3],[12,16]],false);line([[7,11],[12,16],[17,11]],false)}
        else if(name==="backup") {p.strokeRect(3,3,18,5);line([[5,8],[5,21],[19,21],[19,8]],false);line([[9,12],[15,12]],false)}
        else if(name==="undo") {line([[8,4],[3,9],[8,14]],false);p.beginPath();p.moveTo(3,9);p.lineTo(13,9);p.bezierCurveTo(23,9,23,21,13,21);p.stroke()}
        else if(name==="eye" || name==="eyeOff") {p.beginPath();p.moveTo(2,12);p.bezierCurveTo(7,3,17,3,22,12);p.bezierCurveTo(17,21,7,21,2,12);p.stroke();circle(12,12,3);if(name==="eyeOff")line([[3,3],[21,21]],false)}
        else if(name==="filterClear") {line([[3,5],[21,5]],false);line([[6,11],[15,11]],false);line([[9,17],[11,17]],false);line([[16,15],[21,20]],false);line([[21,15],[16,20]],false)}
        else circle(12,12,8)
    }
}
