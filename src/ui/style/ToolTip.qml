import QtQuick
import QtQuick.Templates as T
T.ToolTip {
    id: control
    readonly property var hostWindow: parent ? parent.Window.window : null
    implicitWidth: Math.min(400*DesignTokens.scale,hostWindow ? hostWindow.width-32 : 800,implicitContentWidth+leftPadding+rightPadding)
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    padding: 8
    margins: 8
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.caption
    contentItem: Text {
        text: control.text
        font: control.font
        color: DesignTokens.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }
    background: Rectangle {color: DesignTokens.grouped;radius: DesignTokens.radius;border.color: DesignTokens.border}
}
