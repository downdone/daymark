import QtQuick
Rectangle {
    id: surface
    property bool focused: false
    property bool hovered: false
    property bool enabledField: true
    // Inspectors opt in through their visual content root; ordinary forms keep
    // explicit field outlines. Visual ancestry also follows reparented fields.
    readonly property bool understated: {
        let owner=parent ? parent.parent : null;
        while(owner){if(owner.objectName==="popoverFieldGroup")return true;owner=owner.parent;}
        return false;
    }
    y: DesignTokens.controlInset
    height: parent ? Math.max(0,parent.height-2*DesignTokens.controlInset) : DesignTokens.controlVisualHeight
    radius: DesignTokens.radius
    color: !enabledField ? DesignTokens.sidebar : hovered || (understated && focused) ? DesignTokens.hover : understated ? "transparent" : DesignTokens.fieldFill
    border.width: focused || DesignTokens.highContrast || (!understated && hovered) ? 1 : 0
    border.color: focused ? DesignTokens.accent : hovered ? DesignTokens.secondary : DesignTokens.fieldLine
}
