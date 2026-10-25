import QtQuick
Rectangle {
    id: surface
    property bool focused: false
    property bool hovered: false
    property bool enabledField: true
    radius: DesignTokens.radius
    color: !enabledField ? DesignTokens.sidebar : DesignTokens.surface
    border.width: focused ? 2 : 1
    border.color: focused ? DesignTokens.accent : DesignTokens.fieldLine
}
