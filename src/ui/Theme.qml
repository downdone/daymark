pragma Singleton
import QtQuick
QtObject {
    property int mode: 0
    property real scale: 1.0
    property SystemPalette systemPalette: SystemPalette { colorGroup: SystemPalette.Active }
    readonly property bool dark: mode === 2 || (mode === 0 && systemPalette.window.hslLightness < 0.5)
    readonly property color content: dark ? "#202124" : "#ffffff"
    readonly property color sidebar: dark ? "#191a1d" : "#f3f4f6"
    readonly property color hover: dark ? "#2b2d32" : "#f1f3f6"
    readonly property color selected: dark ? "#263a5b" : "#e8effc"
    readonly property color text: dark ? "#f2f3f5" : "#24272d"
    readonly property color secondary: dark ? "#b3b8c2" : "#626976"
    readonly property color accent: dark ? "#93b6ff" : "#315db5"
    readonly property color danger: dark ? "#ffb4ab" : "#b3261e"
    readonly property color success: dark ? "#8bd5ad" : "#216b46"
    readonly property color separator: dark ? "#41454e" : "#d8dce3"
}
