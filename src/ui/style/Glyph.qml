pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
Item {
    id: root
    property string name: "circle"
    property color ink: DesignTokens.secondary
    implicitWidth:DesignTokens.iconSize
    implicitHeight:DesignTokens.iconSize
    readonly property real drawingSize:Math.min(width,height,DesignTokens.iconSize)
    readonly property string pathData:IconPaths.path(name)
    // Bound the small geometry paths for reliable initial native rendering,
    // multisampled edges and nested software-renderer scroll clips.
    layer.enabled:true
    layer.samples:4
    property bool drawingReady:false
    Component.onCompleted:drawingReady=true
    function refreshDrawing(){if(drawingReady){drawing.active=false;drawing.active=true}}
    onInkChanged:refreshDrawing()
    onPathDataChanged:refreshDrawing()
    Loader {
        id:drawing
        x:(root.width-root.drawingSize)/2;y:(root.height-root.drawingSize)/2
        sourceComponent:Shape {
        id:shape
        preferredRendererType:Shape.GeometryRenderer
        width:24;height:24;scale:root.drawingSize/24;transformOrigin:Item.TopLeft
        ShapePath {
            strokeColor:root.ink;fillColor:"transparent"
            strokeWidth:root.drawingSize<=14 ? 1.85 : 1.65
            capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
            PathSvg {path:root.pathData}
        }
    }
    }
}
