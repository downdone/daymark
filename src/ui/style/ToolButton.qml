import QtQuick
import QtQuick.Templates as T
T.ToolButton {
    id: control
    implicitWidth: Math.max(32,contentItem.implicitWidth+12)
    implicitHeight: Math.max(32,contentItem.implicitHeight+12)
    padding: 6
    hoverEnabled: true
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.body
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.secondary : DesignTokens.disabledText;horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    background: Rectangle {radius: Math.min(width,height)/2;color: control.down ? DesignTokens.pressed : control.checked ? DesignTokens.selected : control.hovered ? DesignTokens.hover : "transparent";border.width: control.visualFocus ? 2 : 0;border.color: DesignTokens.accent}
}
