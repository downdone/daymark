import QtQuick
Rectangle {
    id: surface
    property bool focused: false
    property bool hovered: false
    property bool enabledField: true
    y: DesignTokens.controlInset
    height: parent ? Math.max(0,parent.height-2*DesignTokens.controlInset) : DesignTokens.controlVisualHeight
    radius: DesignTokens.radius
    color: !enabledField ? DesignTokens.sidebar : hovered ? DesignTokens.hover : DesignTokens.surface
    border.width: 1
    border.color: focused ? DesignTokens.accent : hovered ? DesignTokens.secondary : DesignTokens.fieldLine
}
