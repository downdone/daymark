pragma Singleton
import QtQuick
import DaymarkStyle as DS
QtObject {
    property int mode: 0
    property real scale: 1.0
    property bool compact: false
    property bool highContrast: false
    onHighContrastChanged: DS.DesignTokens.highContrast=highContrast
    onModeChanged: DS.DesignTokens.mode=mode
    onScaleChanged: DS.DesignTokens.scale=scale
    onCompactChanged: DS.DesignTokens.compact=compact
    function catalogColor(key) {
        const colors={neutral:secondary,blue:accent,green:DS.DesignTokens.categoryGreen,orange:DS.DesignTokens.categoryOrange,purple:DS.DesignTokens.categoryPurple}
        return colors[key] || secondary
    }
    property SystemPalette systemPalette: SystemPalette { colorGroup: SystemPalette.Active }
    readonly property bool dark: DS.DesignTokens.dark
    readonly property color content: DS.DesignTokens.surface
    readonly property color sidebar: DS.DesignTokens.sidebar
    readonly property color grouped: DS.DesignTokens.grouped
    readonly property color hover: DS.DesignTokens.hover
    readonly property color selected: DS.DesignTokens.selected
    readonly property color text: DS.DesignTokens.text
    readonly property color secondary: DS.DesignTokens.secondary
    readonly property color accent: DS.DesignTokens.accent
    readonly property color accentText: DS.DesignTokens.accentText
    readonly property color disabledText: DS.DesignTokens.disabledText
    readonly property color calendarToday: DS.DesignTokens.calendarToday
    readonly property color calendarTodayText: DS.DesignTokens.calendarTodayText
    readonly property color danger: DS.DesignTokens.danger
    readonly property color toggleOn: DS.DesignTokens.toggleOn
    readonly property color primaryInk: DS.DesignTokens.primaryInk
    readonly property color primaryFill: DS.DesignTokens.primaryFill
    readonly property color scheduleFill: DS.DesignTokens.scheduleFill
    readonly property color scheduleInk: DS.DesignTokens.scheduleInk
    readonly property color pressed: DS.DesignTokens.pressed
    readonly property color success: DS.DesignTokens.success
    readonly property color separator: DS.DesignTokens.border
    readonly property string fontFamily: DS.DesignTokens.fontFamily
    readonly property int taskTitleSize: DS.DesignTokens.taskTitle
    readonly property int rowRadius: DS.DesignTokens.rowRadius
    readonly property int bodySize: DS.DesignTokens.body
    readonly property int listTitleSize: Math.round(13*scale)
    readonly property int navigationSize: DS.DesignTokens.navigation
    readonly property int captionSize: DS.DesignTokens.caption
    readonly property int sectionSize: DS.DesignTokens.section
    readonly property int headingSize: DS.DesignTokens.heading
    readonly property int titleSize: DS.DesignTokens.title
    readonly property int iconSize: Math.round(Math.min(24,18*scale))
    readonly property int gap: DS.DesignTokens.gap
    readonly property int fieldGap: DS.DesignTokens.fieldGap
    readonly property int panelPadding: DS.DesignTokens.panelPadding
    readonly property int panelRadius: DS.DesignTokens.panelRadius
    readonly property int pagePadding: DS.DesignTokens.pagePadding
}
