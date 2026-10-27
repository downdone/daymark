import QtQuick
import QtQuick.Templates as T
T.SpinBox {
    id: control
    wheelEnabled: false
    hoverEnabled: true
    implicitWidth: Math.max(112,contentItem.implicitWidth+leftPadding+rightPadding)
    implicitHeight: DesignTokens.controlHeight
    leftPadding: DesignTokens.controlHeight
    rightPadding: DesignTokens.controlHeight
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.body
    validator: IntValidator {bottom: Math.min(control.from,control.to);top: Math.max(control.from,control.to)}
    contentItem: TextInput {
        text: control.displayText
        font: control.font
        color: control.enabled ? DesignTokens.text : DesignTokens.disabledText
        selectionColor: DesignTokens.accent
        selectedTextColor: DesignTokens.accentText
        readOnly: !control.editable
        validator: control.validator
        horizontalAlignment: TextInput.AlignHCenter
        verticalAlignment: TextInput.AlignVCenter
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
    }
    up.indicator: Rectangle {
        x: control.width-width
        implicitWidth: DesignTokens.controlHeight;implicitHeight: DesignTokens.controlHeight
        color: control.up.pressed ? DesignTokens.pressed : "transparent"
        Glyph {anchors.centerIn:parent;width:16;height:16;name:"plus";ink:control.enabled && control.value<control.to ? DesignTokens.accent : DesignTokens.disabledText}
    }
    down.indicator: Rectangle {
        implicitWidth: DesignTokens.controlHeight;implicitHeight: DesignTokens.controlHeight
        color: control.down.pressed ? DesignTokens.pressed : "transparent"
        Glyph {anchors.centerIn:parent;width:16;height:16;name:"minus";ink:control.enabled && control.value>control.from ? DesignTokens.accent : DesignTokens.disabledText}
    }
    background: FieldSurface {focused: control.activeFocus;hovered: control.hovered;enabledField: control.enabled}
}
