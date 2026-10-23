import QtQuick
import QtQuick.Templates as T
T.ItemDelegate {
    id: control
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(36,contentItem.implicitHeight+16)
    padding: 8
    horizontalPadding: 12
    hoverEnabled: true
    font.pixelSize: DesignTokens.body
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.text : DesignTokens.disabledText;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
    background: Rectangle {radius: DesignTokens.radius;color: control.highlighted ? DesignTokens.selected : control.down ? DesignTokens.pressed : control.hovered ? DesignTokens.hover : "transparent";border.width: control.visualFocus ? 2 : 0;border.color: DesignTokens.accent}
}
