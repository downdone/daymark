import QtQuick
import QtQuick.Templates as T
T.CheckBox {
    id: control
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(DesignTokens.controlHeight,contentItem.implicitHeight+(DesignTokens.compact ? 8 : 12))
    padding: DesignTokens.compact ? 4 : 6
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
        color: !control.enabled ? DesignTokens.border : control.checked || control.checkState===Qt.PartiallyChecked ? DesignTokens.accent : control.hovered ? DesignTokens.hover : DesignTokens.surface
        border.color: control.visualFocus ? DesignTokens.accent : DesignTokens.outline
        border.width: control.visualFocus ? 2 : 1
        Glyph {anchors.centerIn:parent;width:parent.width-4;height:width;name:control.checkState===Qt.PartiallyChecked ? "minus" : "check";visible:control.checked || control.checkState===Qt.PartiallyChecked;ink:DesignTokens.accentText}
    }
    contentItem: Text {text: control.text;font: control.font;color: control.enabled ? DesignTokens.text : DesignTokens.disabledText;leftPadding: control.indicator.width+control.spacing;verticalAlignment: Text.AlignVCenter;wrapMode: Text.Wrap}
}
