import QtQuick
import QtQuick.Templates as T
T.MenuItem {
    id: control
    TextMetrics {id: labelMetrics;text: control.text;font: control.font}
    implicitWidth: labelMetrics.advanceWidth+leftPadding+rightPadding
    implicitHeight: Math.max(32,contentItem.implicitHeight+12)
    height: visible ? implicitHeight : 0
    leftPadding: (checkable ? 32 : 12)*DesignTokens.scale
    rightPadding: 12*DesignTokens.scale
    hoverEnabled: true
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.body
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.text : DesignTokens.disabledText;verticalAlignment: Text.AlignVCenter;wrapMode: Text.Wrap}
    indicator: Text {visible: control.checked;x: 10*DesignTokens.scale;y: (control.height-height)/2;text: "✓";color: DesignTokens.accent;font.pixelSize: DesignTokens.body}
    background: Rectangle {radius: 4;color: control.down ? DesignTokens.pressed : control.highlighted || control.hovered ? DesignTokens.selected : "transparent";border.width: control.visualFocus ? 2 : 0;border.color: DesignTokens.accent}
}
