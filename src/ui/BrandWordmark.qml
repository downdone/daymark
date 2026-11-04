// Generated from assets/brand/current/source/geometry.json; original custom lettering.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
Item {
    id:root
    property color ink:Theme.text
    implicitWidth:120;implicitHeight:implicitWidth*58/274
    Accessible.role:Accessible.StaticText;Accessible.name:"daymark"
    property bool paletteReady:false
    Component.onCompleted:paletteReady=true
    onInkChanged:if(paletteReady){drawing.active=false;drawing.active=true}
    Loader {id:drawing;sourceComponent:lettering}
    Component {
        id:lettering
        Shape {
            width:274;height:58
            layer.enabled:true;layer.smooth:true
            preferredRendererType:Shape.CurveRenderer
            transform:Scale{xScale:root.width/274;yScale:root.height/58}
            ShapePath {
                strokeColor:root.ink;strokeWidth:4.5;fillColor:"transparent"
                capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
            PathSvg {path:"M31 5V39M31 27C31 19 26 14 18 14C10 14 5 19 5 27C5 35 10 40 18 40C26 40 31 35 31 27"}
            PathSvg {path:"M69.5 15V39M69.5 27C69.5 19 64.5 14 56.5 14C48.5 14 43.5 19 43.5 27C43.5 35 48.5 40 56.5 40C64.5 40 69.5 35 69.5 27"}
            PathSvg {path:"M82 15L94 39M107 15L93 44C90 50 87 52 81 52"}
            PathSvg {path:"M121.7 39V15M121.7 25C121.7 18 125.7 14 131.7 14C137.7 14 141.7 18 141.7 25V39M141.7 25C141.7 18 145.7 14 151.7 14C157.7 14 161.7 18 161.7 25V39"}
            PathSvg {path:"M200.1 15V39M200.1 27C200.1 19 195.1 14 187.1 14C179.1 14 174.1 19 174.1 27C174.1 35 179.1 40 187.1 40C195.1 40 200.1 35 200.1 27"}
            PathSvg {path:"M213.7 39V15M213.7 26C213.7 19 219.7 14 228.7 15"}
            PathSvg {path:"M240.9 5V39M261.9 15L241.9 29M250.9 23L263.9 39"}
            }
        }
    }
}
