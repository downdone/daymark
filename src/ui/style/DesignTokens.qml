pragma Singleton
import QtQuick
QtObject {
    property int mode: 0
    property real scale: 1
    property bool compact: true
    property bool highContrast: false
    property int activeMenus: 0
    property bool dragActive: false
    readonly property int hintDelay: 600
    readonly property int hintTimeout: 5000
    readonly property int menuFontSize: body
    readonly property int menuRowHeight: Math.max(compact ? 28 : 32,body+10)
    property SystemPalette systemPalette: SystemPalette {colorGroup: SystemPalette.Active}
    readonly property bool dark: mode===2 || (mode===0 && systemPalette.window.hslLightness<0.5)
    readonly property color surface: dark ? "#1c1c1e" : "#ffffff"
    readonly property color sidebar: dark ? "#171719" : "#f3f4f6"
    readonly property color grouped: dark ? "#252527" : "#f6f6f8"
    readonly property color hover: dark ? "#303034" : "#f0f2f6"
    readonly property color pressed: dark ? "#38383e" : "#edf0f5"
    readonly property color selected: highContrast ? (dark ? "#29466d" : "#d9e8ff") : dark ? "#263a52" : "#eaf1fc"
    readonly property color text: dark ? "#f2f2f7" : "#1c1c1e"
    readonly property color secondary: highContrast ? (dark ? "#ced2da" : "#484a50") : dark ? "#b7b7c2" : "#5d5d66"
    readonly property color disabledText: dark ? "#6f7580" : "#989ca5"
    readonly property color accent: highContrast ? (dark ? "#a7ceff" : "#004ca8") : dark ? "#83baff" : "#0063cf"
    readonly property color accentText: dark ? "#102038" : "#ffffff"
    readonly property color danger: dark ? "#ff9ca4" : "#b92330"
    readonly property color success: dark ? "#83d6aa" : "#236e46"
    readonly property color toggleOn: dark ? "#32d45a" : "#248a3d"
    readonly property color border: highContrast ? (dark ? "#77808d" : "#9fa5b0") : dark ? "#424249" : "#e0e1e7"
    readonly property color outline: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8b8b97" : "#787883"
    readonly property color fieldFill: dark ? "#2b2b30" : "#f2f3f6"
    readonly property color fieldLine: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8b8b97" : "#787883"
    readonly property color calendarToday: dark ? "#f58d96" : "#c22c3b"
    readonly property color calendarTodayText: dark ? "#2d1017" : "#ffffff"
    readonly property color primaryFill: dark ? "#183b5d" : "#e4efff"
    readonly property color primaryInk: dark ? "#90c5ff" : "#005bbb"
    readonly property color scheduleFill: dark ? "#173b29" : "#def3e6"
    readonly property color scheduleInk: dark ? "#a1e6b7" : "#205b35"
    readonly property color categoryGreen: dark ? "#83d6aa" : "#236e46"
    readonly property color categoryOrange: dark ? "#ffc18c" : "#945000"
    readonly property color categoryPurple: dark ? "#d0b3ff" : "#7042a0"
    readonly property string fontFamily: Application.font.family
    readonly property int taskTitle: Math.round(17*scale)
    readonly property int body: Math.round(13*scale)
    readonly property int navigation: Math.round(13*scale)
    readonly property int caption: Math.round(12*scale)
    readonly property int section: Math.round(13*scale)
    readonly property int heading: Math.round(17*scale)
    readonly property int title: Math.round(20*scale)
    readonly property int footnote: Math.round(11*scale)
    readonly property int iconSize: Math.round(Math.min(24,18*scale))
    readonly property int controlHeight: Math.max(32,body+(compact ? 12 : 16))
    readonly property int controlVisualHeight: Math.max(compact ? 28 : 32,body+10)
    readonly property int controlInset: Math.max(0,(controlHeight-controlVisualHeight)/2)
    readonly property int navigationRowHeight: Math.max(32,navigation+12)
    readonly property int taskRowHeight: Math.max(compact ? 36 : 40,body+16)
    readonly property int smartTileHeight: Math.max(compact ? 56 : 64,caption+heading+20)
    readonly property int toolbarHeight: Math.max(compact ? 40 : 44,controlHeight+8)
    readonly property int propertyLabelWidth: Math.round(72*scale)
    readonly property int listTitle: Math.round(14*scale)
    readonly property int listInset: 6
    readonly property int hierarchyGutter: 24
    readonly property int hierarchyIndent: 20
    readonly property int listTextGap: 8
    readonly property int radius: 7
    readonly property int rowRadius: 8
    readonly property int groupRadius: 12
    readonly property int panelRadius: 14
    readonly property int menuRadius: 10
    readonly property int eventRadius: 4
    readonly property int blockGap: 16
    readonly property int gap: 8
    readonly property int fieldGap: compact ? 4 : 8
    readonly property int groupGap: compact ? 8 : 12
    readonly property int sectionPadding: compact ? 4 : 8
    readonly property int pagePadding: compact ? 12 : 16
    readonly property int panelPadding: compact ? 12 : 16
}
