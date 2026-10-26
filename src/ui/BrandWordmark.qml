pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// Original Daymark lettering; independent of installed fonts and third-party logos.
Item {
    id: root
    property color ink: Theme.text
    implicitWidth: 104
    implicitHeight: implicitWidth*36/164
    Accessible.role: Accessible.StaticText
    Accessible.name: "Daymark"
    // Recreate this small static path on palette changes: the Windows main
    // surface can otherwise retain the previous Shape stroke color.
    property bool paletteReady: false
    Component.onCompleted: paletteReady=true
    onInkChanged: if(paletteReady){drawing.active=false;drawing.active=true}
    Loader {id:drawing;sourceComponent:lettering}
    Component {
        id: lettering
        Shape {
            layer.enabled: true;layer.samples: 4
            width: 164;height: 36
            preferredRendererType: Shape.GeometryRenderer
            transform: Scale {xScale:root.width/164;yScale:root.height/36}
            ShapePath {
                strokeColor: root.ink;strokeWidth: 3.6;fillColor: "transparent"
                capStyle: ShapePath.RoundCap;joinStyle: ShapePath.RoundJoin
                PathSvg {path: "M18 3V25 M18 18C18 8 3 8 3 18C3 28 18 28 18 18 M40 11V25 M40 18C40 8 25 8 25 18C25 28 40 28 40 18 M48 11L55 25 M62 11L54 29Q52 33 48 33 M69 25V11 M69 17C69 8 82 8 82 17V25 M82 17C82 8 95 8 95 17V25 M117 11V25 M117 18C117 8 102 8 102 18C102 28 117 28 117 18 M125 25V11 M125 17C125 12 131 9 137 12 M145 3V25 M160 11L146 20 M153 17L161 25"}
            }
        }
    }
}
