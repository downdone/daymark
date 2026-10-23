import QtQuick
import QtQuick.Templates as T
T.MenuItem {
    id: control
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(34,contentItem.implicitHeight+14)
    leftPadding: checkable ? 32 : 12
    rightPadding: 12
    hoverEnabled: true
    font.pixelSize: DesignTokens.body
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.text : DesignTokens.disabledText;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
    indicator: Text {visible: control.checked;x: 10;y: (control.height-height)/2;text: "✓";color: DesignTokens.accent;font.pixelSize: DesignTokens.body}
    background: Rectangle {radius: 4;color: control.down ? DesignTokens.pressed : control.highlighted || control.hovered ? DesignTokens.selected : "transparent";border.width: control.visualFocus ? 2 : 0;border.color: DesignTokens.accent}
}
