pragma ComponentBehavior: Bound
import QtQuick

Item {
    id: root
    implicitWidth: 200
    implicitHeight: implicitWidth*72/320
    property color ink: Theme.text
    property bool monochrome: false
    Accessible.role: Accessible.StaticText
    Accessible.name: "daymark"
    Item {
        width: 320; height: 72
        transform: Scale {xScale: root.width/320; yScale: root.height/72}
        BrandMark {x: 4; y: 0; width: 64; height: 64; ink: root.ink; monochrome: root.monochrome; Accessible.ignored: true}
        BrandWordmark {x: 76; y: 11; width: 241.12; height: 51.04; ink: root.ink; Accessible.ignored: true}
    }
}
