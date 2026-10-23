import QtQuick
import QtQuick.Templates as T
T.TextArea {
    id: control
    implicitWidth: 240
    implicitHeight: Math.max(96,contentHeight+20)
    padding: 10
    font.pixelSize: DesignTokens.body
    color: enabled ? DesignTokens.text : DesignTokens.disabledText
    placeholderTextColor: DesignTokens.secondary
    selectionColor: DesignTokens.accent
    selectedTextColor: DesignTokens.accentText
    selectByMouse: true
    wrapMode: TextEdit.Wrap
    Text {
        x: control.leftPadding
        y: control.topPadding
        width: control.width-control.leftPadding-control.rightPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        wrapMode: Text.Wrap
        visible: control.length===0 && !control.preeditText
    }
    background: Rectangle {radius: DesignTokens.radius;color: DesignTokens.surface;border.color: control.activeFocus ? DesignTokens.accent : DesignTokens.outline;border.width: control.activeFocus ? 2 : 1}
}
