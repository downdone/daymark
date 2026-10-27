import QtQuick
import QtQuick.Templates as T
T.Dialog {
    id: control
    implicitWidth: Math.max(320,implicitContentWidth+leftPadding+rightPadding,implicitHeaderWidth,implicitFooterWidth)
    implicitHeight: implicitContentHeight+topPadding+bottomPadding+(header && header.visible ? header.implicitHeight+spacing : 0)+(footer && footer.visible ? footer.implicitHeight+spacing : 0)
    padding: DesignTokens.panelPadding
    spacing: 12
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.body
    background: Rectangle {color: DesignTokens.surface;radius: DesignTokens.panelRadius;border.color: DesignTokens.border}
    header: Text {
        visible: control.title.length>0
        text: control.title
        font.pixelSize: DesignTokens.heading
        font.weight: Font.DemiBold
        color: DesignTokens.text
        padding: DesignTokens.panelPadding
        bottomPadding: 0
        wrapMode: Text.Wrap
    }
    footer: DialogButtonBox {visible: count>0;standardButtons: control.standardButtons}
    T.Overlay.modal: Rectangle {color: "#26000000"}
    T.Overlay.modeless: Rectangle {color: "transparent"}
}
