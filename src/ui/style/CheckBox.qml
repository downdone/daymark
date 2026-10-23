import QtQuick
import QtQuick.Templates as T
T.CheckBox {
    id: control
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(32,contentItem.implicitHeight+12)
    padding: 6
    spacing: 8
    hoverEnabled: true
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.body
    indicator: Rectangle {
        implicitWidth: Math.max(18,18*DesignTokens.scale)
        implicitHeight: implicitWidth
        x: control.leftPadding
        y: (control.height-height)/2
        radius: 4
        color: control.checked || control.checkState===Qt.PartiallyChecked ? DesignTokens.accent : control.hovered ? DesignTokens.hover : DesignTokens.surface
        border.color: control.visualFocus ? DesignTokens.accent : DesignTokens.outline
        border.width: control.visualFocus ? 2 : 1
        Text {anchors.centerIn: parent;text: control.checkState===Qt.PartiallyChecked ? "−" : "✓";visible: control.checked || control.checkState===Qt.PartiallyChecked;color: DesignTokens.accentText;font.pixelSize: Math.round(14*DesignTokens.scale);font.bold: true}
    }
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.text : DesignTokens.disabledText;leftPadding: control.indicator.width+control.spacing;verticalAlignment: Text.AlignVCenter;wrapMode: Text.Wrap}
}
