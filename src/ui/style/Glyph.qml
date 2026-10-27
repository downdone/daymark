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
    // The raster Shape backend needs a bounded layer to honor nested scroll
    // clips. Native RHI keeps vector antialiasing without a texture per icon.
    layer.enabled:shape.rendererType===Shape.SoftwareRenderer
    Shape {
        id:shape
        preferredRendererType:Shape.CurveRenderer
        x:(root.width-root.drawingSize)/2;y:(root.height-root.drawingSize)/2
        width:24;height:24;scale:root.drawingSize/24;transformOrigin:Item.TopLeft
        ShapePath {
            strokeColor:root.ink;fillColor:"transparent"
            strokeWidth:root.drawingSize<=14 ? 1.85 : 1.65
            capStyle:ShapePath.RoundCap;joinStyle:ShapePath.RoundJoin
            PathSvg {path:root.pathData}
        }
    }
}
