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
    readonly property int menuRowHeight: Math.max(compact ? 26 : 32,body+10)
    property SystemPalette systemPalette: SystemPalette {colorGroup: SystemPalette.Active}
    readonly property bool dark: mode===2 || (mode===0 && systemPalette.window.hslLightness<0.5)
    readonly property color surface: dark ? "#1c1c1e" : "#ffffff"
    readonly property color sidebar: dark ? "#242426" : "#f3f3f5"
    readonly property color grouped: dark ? "#2c2c2e" : "#fafafa"
    readonly property color hover: dark ? "#38383d" : "#eeeef1"
    readonly property color pressed: dark ? "#414146" : "#e3e3e8"
    readonly property color selected: highContrast ? (dark ? "#29466d" : "#d9e8ff") : dark ? "#293d60" : "#e7effd"
    readonly property color text: dark ? "#f2f2f4" : "#202124"
    readonly property color secondary: highContrast ? (dark ? "#ced2da" : "#484a50") : dark ? "#bcbcc4" : "#60616a"
    readonly property color disabledText: dark ? "#6f7580" : "#989ca5"
    readonly property color accent: highContrast ? (dark ? "#a7ceff" : "#004ca8") : dark ? "#79afff" : "#0964d4"
    readonly property color accentText: dark ? "#101828" : "#ffffff"
    readonly property color danger: dark ? "#ff9ca4" : "#b92330"
    readonly property color success: dark ? "#83d6aa" : "#236e46"
    readonly property color toggleOn: highContrast ? (dark ? "#72dea0" : "#19642e") : dark ? "#26a659" : "#208946"
    readonly property color toggleThumb: dark && highContrast ? accentText : "#ffffff"
    readonly property color border: highContrast ? (dark ? "#77808d" : "#9fa5b0") : dark ? "#49494e" : "#d9d9df"
    readonly property color taskDivider: highContrast ? border : dark ? "#323234" : "#ececef"
    readonly property color detailDivider: highContrast ? border : dark ? "#48484b" : "#e2e2e6"
    readonly property color navigationSelection: highContrast ? selected : dark ? "#46464a" : "#e2e2e7"
    readonly property color inactiveSelection: highContrast ? selected : dark ? "#303034" : "#f0f0f3"
    readonly property color quietFieldFill: dark ? "#303032" : "#f2f2f4"
    readonly property color outline: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8e8e98" : "#85858f"
    readonly property color fieldFill: dark ? "#38383d" : "#eeeef1"
    readonly property color fieldLine: highContrast ? (dark ? "#a4afbe" : "#626772") : dark ? "#8e8e98" : "#85858f"
    readonly property color calendarToday: dark ? "#f58d96" : "#c22c3b"
    readonly property color calendarTodayText: dark ? "#2d1017" : "#ffffff"
    readonly property color primaryFill: dark ? "#183b5d" : "#e4efff"
    readonly property color primaryInk: dark ? "#90c5ff" : "#005bbb"
    readonly property color scheduleFill: dark ? "#173b29" : "#def3e6"
    readonly property color scheduleInk: dark ? "#a1e6b7" : "#205b35"
    readonly property color categoryGreen: dark ? "#83d6aa" : "#236e46"
    readonly property color categoryOrange: dark ? "#ffc18c" : "#945000"
    readonly property color categoryPurple: dark ? "#d0b3ff" : "#7042a0"
    // Semantic roles are shared by every window; glyph size never sets its hit area.
    readonly property color brandFill: dark ? "#1e63ba" : "#0964d4"
    readonly property color smartFill: dark ? "#39393c" : "#eaeaee"
    readonly property var smartBadgeColors: ({
        inbox: dark ? "#b995ea" : "#8053aa",
        today: dark ? "#42aafa" : "#1279cb",
        scheduled: dark ? "#ff6965" : "#d63b43",
        all: dark ? "#b0b1ba" : "#626773",
        important: dark ? "#f4ac47" : "#ba6d09",
        completed: dark ? "#9aaebe" : "#657985"
    })
    readonly property int windowTitle: Math.round(22*scale)
    readonly property int panelTitle: Math.round(15*scale)
    readonly property int smartCount: Math.round(20*scale)
    readonly property int completionSize: Math.round(Math.min(24,16*scale))
    readonly property string fontFamily: Application.font.family
    readonly property int taskTitle: Math.round(17*scale)
    readonly property int body: Math.round(13*scale)
    readonly property int navigation: Math.round(13*scale)
    readonly property int caption: Math.round(12*scale)
    readonly property int section: Math.round(13*scale)
    readonly property int heading: Math.round(17*scale)
    readonly property int title: Math.round(20*scale)
    readonly property int pageTitle: Math.round(26*scale)
    readonly property int pageCount: Math.round(24*scale)
    readonly property int footnote: Math.round(11*scale)
    readonly property int iconSize: Math.round(Math.min(24,16*scale))
    readonly property int controlHeight: Math.max(32,body+(compact ? 12 : 16))
    readonly property int controlVisualHeight: Math.max(compact ? 28 : 32,body+10)
    readonly property int controlInset: Math.max(0,(controlHeight-controlVisualHeight)/2)
    readonly property int navigationRowHeight: Math.max(32,navigation+12)
    readonly property int taskRowHeight: Math.max(compact ? 36 : 40,body+16)
    readonly property int smartTileHeight: Math.max(compact ? 52 : 60,caption+smartCount+12)
    readonly property int toolbarHeight: Math.max(compact ? 40 : 44,controlHeight+8)
    readonly property int propertyLabelWidth: Math.round(72*scale)
    readonly property int listTitle: Math.round(14*scale)
    readonly property int listInset: 6
    readonly property int hierarchyGutter: 24
    readonly property int hierarchyIndent: 20
    readonly property int listTextGap: 8
    readonly property int radius: 4
    readonly property int rowRadius: 4
    readonly property int groupRadius: 8
    readonly property int panelRadius: 8
    readonly property int menuRadius: 6
    readonly property int eventRadius: 4
    readonly property int blockGap: 16
    readonly property int gap: 8
    readonly property int fieldGap: compact ? 4 : 8
    readonly property int groupGap: compact ? 8 : 12
    readonly property int sectionPadding: compact ? 4 : 8
    readonly property int pagePadding: compact ? 16 : 20
    // Page regions follow the display heading, not the form-field density.
    readonly property int pageRegionGap: Math.ceil((compact ? 16 : 20)*scale/4)*4
    readonly property int panelPadding: compact ? 12 : 16
    readonly property int scrollGutter: 18
    readonly property int rowVerticalPadding: Math.round((compact ? 4 : 8)*scale)
    readonly property int metadataActionHeight: Math.max(24,caption+8)
    readonly property int textLineGap: Math.round(2*scale)
}
