import QtQuick
import QtQuick.Templates as T
T.ToolTip {
    id: control
    property bool contextualFeedback: false
    readonly property bool fitsOutsideSource: !hostWindow || implicitHeight<=Math.max(sourceOrigin.y-8,hostWindow.height-sourceOrigin.y-(parent ? parent.height : 0)-8)
    readonly property bool blocked: DesignTokens.activeMenus>0 || (!contextualFeedback && (DesignTokens.dragActive || !fitsOutsideSource))
    onAboutToShow: if(blocked)close()
    onBlockedChanged: if(blocked)close()
    delay: DesignTokens.hintDelay
    timeout: DesignTokens.hintTimeout
    readonly property var hostWindow: parent ? parent.Window.window : null
    readonly property point sourceOrigin: parent && hostWindow ? parent.mapToItem(hostWindow.contentItem,0,0) : Qt.point(0,0)
    x: hostWindow ? Math.max(8-sourceOrigin.x,Math.min((parent.width-width)/2,hostWindow.width-8-sourceOrigin.x-width)) : 0
    y: sourceOrigin.y>=height+8 ? -height-8 : (parent ? parent.height+8 : 8)
    modal: false
    focus: false
    enabled: false
    closePolicy: T.Popup.NoAutoClose
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
