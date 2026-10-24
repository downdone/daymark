pragma Singleton
import QtQuick
QtObject {
    property int mode: 0
    property real scale: 1
    property bool compact: false
    property bool highContrast: false
    property SystemPalette systemPalette: SystemPalette {colorGroup: SystemPalette.Active}
    readonly property bool dark: mode===2 || (mode===0 && systemPalette.window.hslLightness<0.5)
    readonly property color surface: dark ? "#1c1d20" : "#ffffff"
    readonly property color sidebar: dark ? "#16171a" : "#f5f5f7"
    readonly property color grouped: dark ? "#25272c" : "#f5f6f8"
    readonly property color hover: dark ? "#303238" : "#eceef2"
    readonly property color pressed: dark ? "#3b3f46" : "#e2e5eb"
    readonly property color selected: highContrast ? (dark ? "#29466d" : "#d9e8ff") : dark ? "#243c60" : "#e9f1ff"
    readonly property color text: dark ? "#f5f5f7" : "#1d1d1f"
    readonly property color secondary: highContrast ? (dark ? "#ced2da" : "#484a50") : dark ? "#b2b7c1" : "#61646b"
    readonly property color disabledText: dark ? "#6f7580" : "#989ca5"
    readonly property color accent: highContrast ? (dark ? "#a7ceff" : "#004ca8") : dark ? "#85b7ff" : "#005bc4"
    readonly property color accentText: dark ? "#102038" : "#ffffff"
    readonly property color danger: dark ? "#ff9ba2" : "#bd2730"
    readonly property color success: dark ? "#83d6aa" : "#24734b"
    readonly property color border: highContrast ? (dark ? "#77808d" : "#9fa5b0") : dark ? "#3d4148" : "#dddfe5"
    readonly property color outline: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#828b99" : "#7c818b"
    readonly property color fieldFill: dark ? "#25282e" : "#f3f4f6"
    readonly property color fieldLine: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#828b99" : "#7c818b"
    readonly property color calendarToday: dark ? "#f58d96" : "#c9343f"
    readonly property color calendarTodayText: dark ? "#2d1017" : "#ffffff"
    readonly property color categoryGreen: dark ? "#83d6aa" : "#24734b"
    readonly property color categoryOrange: dark ? "#ffc18c" : "#945000"
    readonly property color categoryPurple: dark ? "#d0b3ff" : "#7042a0"
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
