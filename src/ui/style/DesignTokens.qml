pragma Singleton
import QtQuick
QtObject {
    property int mode: 0
    property real scale: 1
    property bool compact: false
    property SystemPalette systemPalette: SystemPalette {colorGroup: SystemPalette.Active}
    readonly property bool dark: mode===2 || (mode===0 && systemPalette.window.hslLightness<0.5)
    readonly property color surface: dark ? "#202124" : "#ffffff"
    readonly property color sidebar: dark ? "#191a1d" : "#f3f4f6"
    readonly property color hover: dark ? "#2b2d32" : "#f1f3f6"
    readonly property color pressed: dark ? "#373b43" : "#e1e5ec"
    readonly property color selected: dark ? "#263a5b" : "#e8effc"
    readonly property color text: dark ? "#f2f3f5" : "#24272d"
    readonly property color secondary: dark ? "#b3b8c2" : "#626976"
    readonly property color disabledText: dark ? "#737984" : "#8a909b"
    readonly property color accent: dark ? "#93b6ff" : "#315db5"
    readonly property color accentText: dark ? "#191a1d" : "#ffffff"
    readonly property color danger: dark ? "#ffb4ab" : "#b3261e"
    readonly property color success: dark ? "#8bd5ad" : "#216b46"
    readonly property color border: dark ? "#41454e" : "#d8dce3"
    readonly property color outline: dark ? "#858c99" : "#8b929e"
    readonly property color fieldFill: dark ? "#292c32" : "#f5f6f8"
    readonly property color fieldLine: dark ? "#858c99" : "#808a99"
    readonly property string fontFamily: Application.font.family
    readonly property int taskTitle: Math.round(20*scale)
    readonly property int body: Math.round(14*scale)
    readonly property int caption: Math.round(12*scale)
    readonly property int section: Math.round(15*scale)
    readonly property int heading: Math.round(18*scale)
    readonly property int title: Math.round(24*scale)
    readonly property int controlHeight: Math.max(32,body+(compact ? 14 : 18))
    readonly property int radius: 6
    readonly property int rowRadius: 8
    readonly property int panelRadius: 12
    readonly property int gap: 8
    readonly property int fieldGap: compact ? 8 : 10
    readonly property int pagePadding: 20
    readonly property int panelPadding: 16
}
