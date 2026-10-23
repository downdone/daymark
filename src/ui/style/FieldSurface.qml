import QtQuick
Rectangle {
    id: surface
    property bool focused: false
    property bool hovered: false
    property bool enabledField: true
    radius: DesignTokens.radius
    color: !enabledField ? DesignTokens.sidebar : hovered ? DesignTokens.hover : DesignTokens.fieldFill
    border.width: focused ? 2 : 0
    border.color: DesignTokens.accent
    Rectangle {
        visible: !surface.focused
        x: surface.radius
        width: Math.max(0,parent.width-2*x)
        height: 1
        anchors.bottom: parent.bottom
        color: surface.enabledField ? DesignTokens.fieldLine : DesignTokens.border
    }
}
