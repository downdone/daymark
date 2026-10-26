import QtQuick
import QtQuick.Templates as T
T.ToolTip {
    id: control
    property bool contextualFeedback: false
    readonly property bool blocked: DesignTokens.activeMenus>0 || (!contextualFeedback && DesignTokens.dragActive)
    onAboutToShow: if(blocked)close()
    onBlockedChanged: if(blocked)close()
    delay: DesignTokens.hintDelay
    timeout: DesignTokens.hintTimeout
    readonly property var hostWindow: parent ? parent.Window.window : null
    implicitWidth: Math.min(400*DesignTokens.scale,hostWindow ? hostWindow.width-32 : 800,implicitContentWidth+leftPadding+rightPadding)
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    padding: 8
    margins: 8
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.caption
    font.weight: Font.Normal
    contentItem: Text {
        text: control.text
        font: control.font
        color: DesignTokens.text
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
        maximumLineCount: 6
        elide: Text.ElideRight
    }
    background: Rectangle {color: DesignTokens.grouped;radius: DesignTokens.radius;border.color: DesignTokens.border}
}
