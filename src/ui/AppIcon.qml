import QtQuick
Canvas {
    id: icon
    property string name: "circle"
    property color ink: Theme.secondary
    implicitWidth: Theme.iconSize
    implicitHeight: Theme.iconSize
    onNameChanged: requestPaint()
    onInkChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onPaint: {
        const p=getContext("2d")
        p.reset();p.scale(width/24,height/24);p.strokeStyle=ink;p.fillStyle=ink;p.lineWidth=1.7;p.lineCap="round";p.lineJoin="round"
        function line(points,closed) {p.beginPath();p.moveTo(points[0][0],points[0][1]);for(let i=1;i<points.length;i++)p.lineTo(points[i][0],points[i][1]);if(closed)p.closePath();p.stroke()}
        function circle(x,y,r) {p.beginPath();p.arc(x,y,r,0,Math.PI*2);p.stroke()}
        if(name==="inbox") {line([[4,4],[20,4],[22,17],[22,20],[2,20],[2,17]],true);line([[3,14],[8,14],[10,17],[14,17],[16,14],[21,14]],false)}
        else if(name==="today") {circle(12,12,4);for(let i=0;i<8;i++){const a=i*Math.PI/4;line([[12+7*Math.cos(a),12+7*Math.sin(a)],[12+10*Math.cos(a),12+10*Math.sin(a)]],false)}}
        else if(name==="calendar" || name==="upcoming") {p.strokeRect(3,5,18,16);line([[3,10],[21,10]],false);line([[8,3],[8,7]],false);line([[16,3],[16,7]],false);if(name==="upcoming")line([[10,13],[14,13],[14,17]],false)}
        else if(name==="someday") {p.beginPath();p.arc(12,12,9,Math.PI*0.15,Math.PI*1.65);p.stroke();line([[16,3],[19,3],[19,6]],false)}
        else if(name==="logbook" || name==="check")line([[5,12],[10,17],[20,6]],false)
        else if(name==="area") {p.strokeRect(3,3,7,7);p.strokeRect(14,3,7,7);p.strokeRect(3,14,7,7);p.strokeRect(14,14,7,7)}
        else if(name==="project")line([[3,7],[3,20],[21,20],[21,6],[12,6],[10,3],[3,3]],true)
        else if(name==="tag") {line([[3,3],[12,3],[22,13],[13,22],[3,12]],true);circle(8,8,1.5)}
        else if(name==="more") {for(let i=0;i<3;i++){p.beginPath();p.arc(5+i*7,12,1.5,0,Math.PI*2);p.fill()}}
        else if(name==="left")line([[15,5],[8,12],[15,19]],false)
        else if(name==="right")line([[9,5],[16,12],[9,19]],false)
        else if(name==="up")line([[5,15],[12,8],[19,15]],false)
        else if(name==="collapse")line([[5,9],[12,16],[19,9]],false)
        else if(name==="plus") {line([[5,12],[19,12]],false);line([[12,5],[12,19]],false)}
        else if(name==="expand") {line([[5,19],[19,5]],false);line([[9,5],[19,5],[19,15]],false)}
        else if(name==="close") {line([[6,6],[18,18]],false);line([[18,6],[6,18]],false)}
        else if(name==="sidebar") {p.strokeRect(3,4,18,16);line([[9,4],[9,20]],false)}
        else if(name==="trash") {line([[4,6],[20,6]],false);line([[9,3],[15,3]],false);line([[6,6],[7,21],[17,21],[18,6]],false);line([[10,10],[10,17]],false);line([[14,10],[14,17]],false)}
        else circle(12,12,8)
    }
}
