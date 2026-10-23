import QtQuick
import QtQuick.Templates as T
T.DialogButtonBox {
    id: control
    implicitWidth: implicitContentWidth+leftPadding+rightPadding
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    spacing: 8
    padding: DesignTokens.panelPadding
    topPadding: 8
    alignment: Qt.AlignRight
    contentWidth: (contentItem as ListView).contentWidth
    delegate: Button {highlighted: T.DialogButtonBox.buttonRole===T.DialogButtonBox.AcceptRole || T.DialogButtonBox.buttonRole===T.DialogButtonBox.YesRole}
    contentItem: ListView {
        implicitWidth: contentWidth
        implicitHeight: DesignTokens.controlHeight
        orientation: ListView.Horizontal
        model: control.contentModel
        spacing: control.spacing
        boundsBehavior: Flickable.StopAtBounds
        clip: true
    }
}
