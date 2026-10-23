pragma Singleton
import QtQuick
import DaymarkStyle as DS
QtObject {
    property int mode: 0
    property real scale: 1.0
    property bool compact: false
    onModeChanged: DS.DesignTokens.mode=mode
    onScaleChanged: DS.DesignTokens.scale=scale
    onCompactChanged: DS.DesignTokens.compact=compact
    function catalogColor(key) {
        const light={neutral:"#626976",blue:"#315db5",green:"#216b46",orange:"#9a4b00",purple:"#7541a5"}
        const night={neutral:"#b3b8c2",blue:"#93b6ff",green:"#8bd5ad",orange:"#ffc48c",purple:"#d5b5ff"}
        return (dark ? night : light)[key] || secondary
    }
    property SystemPalette systemPalette: SystemPalette { colorGroup: SystemPalette.Active }
    readonly property bool dark: DS.DesignTokens.dark
    readonly property color content: DS.DesignTokens.surface
    readonly property color sidebar: DS.DesignTokens.sidebar
    readonly property color hover: DS.DesignTokens.hover
    readonly property color selected: DS.DesignTokens.selected
    readonly property color text: DS.DesignTokens.text
    readonly property color secondary: DS.DesignTokens.secondary
    readonly property color accent: DS.DesignTokens.accent
    readonly property color danger: DS.DesignTokens.danger
    readonly property color success: DS.DesignTokens.success
    readonly property color separator: DS.DesignTokens.border
    readonly property int bodySize: DS.DesignTokens.body
    readonly property int captionSize: DS.DesignTokens.caption
    readonly property int sectionSize: DS.DesignTokens.section
    readonly property int headingSize: DS.DesignTokens.heading
    readonly property int titleSize: DS.DesignTokens.title
    readonly property int iconSize: Math.round(18*scale)
    readonly property int gap: DS.DesignTokens.gap
    readonly property int fieldGap: DS.DesignTokens.fieldGap
    readonly property int panelPadding: DS.DesignTokens.panelPadding
    readonly property int panelRadius: DS.DesignTokens.panelRadius
    readonly property int pagePadding: DS.DesignTokens.pagePadding
}
