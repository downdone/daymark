import QtQuick
import QtQuick.Templates as T
T.SpinBox {
    id: control
    wheelEnabled: false
    implicitWidth: Math.max(112,contentItem.implicitWidth+leftPadding+rightPadding)
    implicitHeight: DesignTokens.controlHeight
    leftPadding: DesignTokens.controlHeight
    rightPadding: DesignTokens.controlHeight
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
        Text {anchors.centerIn: parent;text: "+";font.pixelSize: DesignTokens.heading;color: control.enabled && control.value<control.to ? DesignTokens.accent : DesignTokens.disabledText}
    }
    down.indicator: Rectangle {
        implicitWidth: DesignTokens.controlHeight;implicitHeight: DesignTokens.controlHeight
        color: control.down.pressed ? DesignTokens.pressed : "transparent"
        Text {anchors.centerIn: parent;text: "−";font.pixelSize: DesignTokens.heading;color: control.enabled && control.value>control.from ? DesignTokens.accent : DesignTokens.disabledText}
    }
    background: Rectangle {radius: DesignTokens.radius;color: DesignTokens.surface;border.width: control.visualFocus ? 2 : 1;border.color: control.visualFocus ? DesignTokens.accent : DesignTokens.border}
}
