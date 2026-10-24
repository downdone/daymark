pragma Singleton
import QtQuick
QtObject {
    property int mode: 0
    property real scale: 1
    property bool compact: false
    property bool highContrast: false
    property SystemPalette systemPalette: SystemPalette {colorGroup: SystemPalette.Active}
    readonly property bool dark: mode===2 || (mode===0 && systemPalette.window.hslLightness<0.5)
    readonly property color surface: dark ? "#1c1c1e" : "#ffffff"
    readonly property color sidebar: dark ? "#111113" : "#f2f2f7"
    readonly property color grouped: dark ? "#242426" : "#f7f7fa"
    readonly property color hover: dark ? "#303034" : "#e9e9ef"
    readonly property color pressed: dark ? "#38383e" : "#dedee6"
    readonly property color selected: highContrast ? (dark ? "#29466d" : "#d9e8ff") : dark ? "#243b5b" : "#e7f0ff"
    readonly property color text: dark ? "#f2f2f7" : "#1c1c1e"
    readonly property color secondary: highContrast ? (dark ? "#ced2da" : "#484a50") : dark ? "#b7b7c2" : "#5d5d66"
    readonly property color disabledText: dark ? "#6f7580" : "#989ca5"
    readonly property color accent: highContrast ? (dark ? "#a7ceff" : "#004ca8") : dark ? "#8ebdff" : "#0057b8"
    readonly property color accentText: dark ? "#102038" : "#ffffff"
    readonly property color danger: dark ? "#ff9ca4" : "#b92330"
    readonly property color success: dark ? "#83d6aa" : "#236e46"
    readonly property color toggleOn: dark ? "#32d45a" : "#248a3d"
    readonly property color border: highContrast ? (dark ? "#77808d" : "#9fa5b0") : dark ? "#414147" : "#dedee5"
    readonly property color outline: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8b8b97" : "#787883"
    readonly property color fieldFill: dark ? "#2b2b30" : "#f2f2f7"
    readonly property color fieldLine: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8b8b97" : "#787883"
    readonly property color calendarToday: dark ? "#f58d96" : "#c22c3b"
    readonly property color calendarTodayText: dark ? "#2d1017" : "#ffffff"
    readonly property color categoryGreen: dark ? "#83d6aa" : "#236e46"
    readonly property color categoryOrange: dark ? "#ffc18c" : "#945000"
    readonly property color categoryPurple: dark ? "#d0b3ff" : "#7042a0"
    readonly property string fontFamily: Application.font.family
    readonly property int taskTitle: Math.round(18*scale)
    readonly property int body: Math.round(14*scale)
    readonly property int caption: Math.round(12*scale)
    readonly property int section: Math.round(14*scale)
    readonly property int heading: Math.round(18*scale)
    readonly property int title: Math.round(24*scale)
    readonly property int controlHeight: Math.max(32,body+(compact ? 14 : 18))
    readonly property int radius: 8
    readonly property int rowRadius: 10
    readonly property int panelRadius: 12
    readonly property int gap: 8
    readonly property int fieldGap: compact ? 8 : 10
    readonly property int pagePadding: 20
    readonly property int panelPadding: 16
}
