import QtQuick
import QtQuick.Templates as T
T.TextField {
    id: control
    implicitWidth: 180
    implicitHeight: Math.max(DesignTokens.controlHeight,contentHeight+12)
    padding: 6
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: DesignTokens.body
    font.family: DesignTokens.fontFamily
    color: enabled ? DesignTokens.text : DesignTokens.disabledText
    placeholderTextColor: DesignTokens.secondary
    selectionColor: DesignTokens.accent
    selectedTextColor: DesignTokens.accentText
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    Text {
        x: control.leftPadding
        y: control.topPadding
        width: control.width-control.leftPadding-control.rightPadding
        height: control.height-control.topPadding-control.bottomPadding
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        verticalAlignment: control.verticalAlignment
        horizontalAlignment: control.horizontalAlignment
        elide: Text.ElideRight
        visible: control.length===0 && !control.preeditText
    }
    background: FieldSurface {focused: control.activeFocus;hovered: control.hovered;enabledField: control.enabled}
}
