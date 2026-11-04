// Generated from assets/brand/current/source/geometry.json; do not hand-edit paths.
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes
Item {
    id: root
    implicitWidth: 28; implicitHeight: 28
    property color ink: Theme.text
    property bool monochrome: false
    readonly property real physicalSize: Math.min(width,height)*Screen.devicePixelRatio
    readonly property bool singleColor: monochrome || Theme.highContrast || physicalSize<32
    readonly property int artworkSize: physicalSize<18 ? 16 : physicalSize<22 ? 20 : physicalSize<32 ? 24 : 100
    readonly property var artwork: artworkSize===100 ? regularPaths : microPaths[artworkSize]
    readonly property var regularPaths: ["M9.14 91.004V49.1C9.14 31.604 23.324 17.42 40.82 17.42H55.22Q57.38 17.42 57.38 19.58V32.54Q57.38 35.42 54.5 35.42H43.7C34.16 35.42 26.42 43.16 26.42 52.7V64.94C26.42 68.612 25.196 71.42 22.82 74.3L9.572 91.22Q9.14 91.796 9.14 91.004Z", "M14.9 92.3L32.18 77.18Q34.34 75.02 37.94 75.02H43.7C54.0392 75.02 62.42 66.6392 62.42 56.3V43.34Q62.42 40.46 65.3 40.46H78.98Q81.86 40.46 81.86 43.34V62.06C81.86 78.764 68.324 92.3 51.62 92.3Z", "M66.1155392 22.8687539L81.2355392 9.1887539A5.76 5.76 0 0 1 88.9644608 17.7312461L73.8444608 31.4112461A5.76 5.76 0 0 1 66.1155392 22.8687539Z"]
    readonly property var microPaths: {"16": ["M9.375 90.625V49.21875C9.375 31.25 23.4375 17.1875 40.625 17.1875H55.46875Q57.03125 17.1875 57.03125 19.53125V32.8125Q57.03125 35.15625 54.6875 35.15625H43.75C34.375 35.15625 26.5625 42.96875 26.5625 52.34375V64.84375C26.5625 68.75 25 71.09375 22.65625 74.21875L9.375 91.40625Q9.375 91.40625 9.375 90.625Z", "M14.84375 92.1875L32.03125 77.34375Q34.375 75 38.28125 75H43.75C53.90625 75 62.5 66.40625 62.5 56.25V42.96875Q62.5 40.625 65.625 40.625H78.90625Q82.03125 40.625 82.03125 42.96875V61.71875C82.03125 78.90625 67.96875 92.1875 51.5625 92.1875Z", "M66.40625 22.65625L81.25 9.375A5.46875 5.46875 0 0 1 89.0625 17.96875L74.21875 31.25A5.46875 5.46875 0 0 1 66.40625 22.65625Z"], "20": ["M9.375 91.25V49.375C9.375 31.875 23.125 17.5 40.625 17.5H55Q57.5 17.5 57.5 19.375V32.5Q57.5 35.625 54.375 35.625H43.75C34.375 35.625 26.25 43.125 26.25 52.5V65C26.25 68.75 25 71.25 23.125 74.375L9.375 91.25Q9.375 91.875 9.375 91.25Z", "M15 92.5L31.875 76.875Q34.375 75 38.125 75H43.75C53.75 75 62.5 66.875 62.5 56.25V43.125Q62.5 40.625 65 40.625H78.75Q81.875 40.625 81.875 43.125V61.875C81.875 78.75 68.125 92.5 51.875 92.5Z", "M66.25 23.125L81.25 9.375A5.625 5.625 0 0 1 88.75 17.5L73.75 31.25A5.625 5.625 0 0 1 66.25 23.125Z"], "24": ["M9.375 91.1458333V48.9583333C9.375 31.7708333 23.4375 17.1875 40.625 17.1875H55.2083333Q57.2916667 17.1875 57.2916667 19.7916667V32.2916667Q57.2916667 35.4166667 54.6875 35.4166667H43.75C34.375 35.4166667 26.5625 43.2291667 26.5625 52.6041667V65.1041667C26.5625 68.75 25 71.3541667 22.9166667 74.4791667L9.375 91.1458333Q9.375 91.6666667 9.375 91.1458333Z", "M15.1041667 92.1875L32.2916667 77.0833333Q34.375 75 38.0208333 75H43.75C54.1666667 75 62.5 66.6666667 62.5 56.25V43.2291667Q62.5 40.625 65.1041667 40.625H79.1666667Q81.7708333 40.625 81.7708333 43.2291667V61.9791667C81.7708333 78.6458333 68.2291667 92.1875 51.5625 92.1875Z", "M66.1458333 22.9166667L81.25 9.375A5.7291667 5.7291667 0 0 1 89.0625 17.7083333L73.9583333 31.25A5.7291667 5.7291667 0 0 1 66.1458333 22.9166667Z"]}
    readonly property var palette: Theme.dark ? ["#83BAFF", "#83D6AA", "#FFC18C", "#D0B3FF"] : ["#287EF0", "#28A680", "#E4A13C", "#A783DE"]
    Accessible.role: Accessible.StaticText
    Accessible.name: "daymark"
    property bool paletteReady: false
    function redraw(){if(paletteReady){drawing.active=false;drawing.active=true}}
    Component.onCompleted: paletteReady=true
    onInkChanged: redraw()
    onSingleColorChanged: redraw()
    onPaletteChanged: redraw()
    Loader {id:drawing;sourceComponent:art}
    Component {
        id:art
        Shape {
            width:100;height:100
            x:(root.width-Math.min(root.width,root.height))/2
            y:(root.height-Math.min(root.width,root.height))/2
            transform: Scale {xScale:Math.min(root.width,root.height)/100;yScale:xScale}
            antialiasing: true
            layer.enabled: true; layer.samples: 8; layer.smooth: true
            preferredRendererType: Shape.GeometryRenderer
            ShapePath { strokeWidth: -1; fillColor: root.ink; PathSvg { path: root.artwork[0] } }
            ShapePath { strokeWidth: -1; fillColor: root.ink; PathSvg { path: root.artwork[1] } }
            ShapePath { strokeWidth: -1; fillColor: root.singleColor ? root.ink : "transparent"; PathSvg { path: root.artwork[2] } }
            ShapePath { strokeWidth: -1; fillColor: root.singleColor ? "transparent" : root.palette[0]; PathSvg { path: "M67.7705936 21.371324L75.4995128 29.9138144L73.8444632 31.4112488A5.76 5.76 0 0 1 66.1155368 22.8687512Z" } }
            ShapePath { strokeWidth: -1; fillColor: root.singleColor ? "transparent" : root.palette[1]; PathSvg { path: "M67.7492384 21.3906488L73.6862144 16.019096L81.4151408 24.5615864L75.4781576 29.9331392Z" } }
            ShapePath { strokeWidth: -1; fillColor: root.singleColor ? "transparent" : root.palette[2]; PathSvg { path: "M73.6648592 16.0384136L79.6018424 10.6668608L87.3307616 19.2093512L81.3937856 24.580904Z" } }
            ShapePath { strokeWidth: -1; fillColor: root.singleColor ? "transparent" : root.palette[3]; PathSvg { path: "M79.5804872 10.6861856L81.2355368 9.1887512A5.76 5.76 0 0 1 88.9644632 17.7312488L87.3094064 19.228676Z" } }
        }
    }
}
