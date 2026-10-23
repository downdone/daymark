import QtQuick
import QtQuick.Templates as T
T.Slider {
    id: control
    implicitWidth: 180
    implicitHeight: Math.max(28,DesignTokens.controlHeight)
    padding: 10
    hoverEnabled: true
    background: Rectangle {
        x: control.leftPadding;y: (control.height-height)/2
        width: control.availableWidth;height: 4
        radius: 2;color: DesignTokens.border
        Rectangle {width: control.position*parent.width;height: parent.height;radius: 2;color: DesignTokens.accent}
    }
    handle: Rectangle {
        x: control.leftPadding+control.visualPosition*(control.availableWidth-width)
        y: (control.height-height)/2
        implicitWidth: 20;implicitHeight: 20
        radius: 10
        color: control.pressed ? DesignTokens.selected : DesignTokens.surface
        border.width: control.visualFocus ? 3 : 2
        border.color: control.enabled ? DesignTokens.accent : DesignTokens.border
    }
}
