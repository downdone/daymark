pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
Item {
    id: root
    required property WindowChrome chrome
    required property ApplicationWindow targetWindow
    property bool fullControls: false
    property alias buttonStrip: controls
    property alias maximizeButton: maximizeControl
    implicitHeight: Math.max(36, Theme.captionSize + 16)
    implicitWidth: fullControls ? 138 : 46
    property color surface: Theme.content
    Accessible.role: Accessible.TitleBar
    Accessible.name: targetWindow.title

    Row {
        id: controls; anchors.right: parent.right; height: parent.height
        CaptionButton {
            objectName: "captionMinimize"; visible: root.fullControls
            captionAction: "minimize"; accessibleText: qsTr("最小化")
            onClicked: root.targetWindow.showMinimized()
        }
        CaptionButton {
            id: maximizeControl; objectName: "captionMaximize"; visible: root.fullControls
            captionAction: root.targetWindow.visibility === Window.Maximized ? "restore" : "maximize"
            accessibleText: root.targetWindow.visibility === Window.Maximized ? qsTr("还原") : qsTr("最大化")
            nativeHovered: root.chrome.nativeMaximizeHovered; nativePressed: root.chrome.nativeMaximizePressed
            onClicked: root.chrome.toggleMaximize()
        }
        CaptionButton {
            objectName: "captionClose"; captionAction: "close"; accessibleText: qsTr("关闭")
            onClicked: root.targetWindow.close()
        }
    }
    component CaptionButton: AbstractButton {
        id: control
        required property string captionAction
        required property string accessibleText
        property bool nativeHovered: false
        property bool nativePressed: false
        readonly property bool emphasized: hovered || nativeHovered || down || nativePressed
        width: 46; height: root.height
        hoverEnabled: true; focusPolicy: Qt.StrongFocus
        Accessible.name: accessibleText
        Keys.onReturnPressed: clicked()
        Keys.onEnterPressed: clicked()
        background: Item {
            Rectangle {
                objectName: "captionFeedback"
                anchors.centerIn: parent
                width: Math.min(36,parent.width)
                height: Math.min(28,parent.height)
                radius: Theme.controlRadius
                color: !control.emphasized ? "transparent"
                    : control.captionAction === "close" ? (control.down ? "#a72319" : "#c42b1c")
                    : control.down || control.nativePressed ? Theme.pressed : Theme.hover
                border.width: control.visualFocus ? 1 : 0
                border.color: control.captionAction === "close" && control.emphasized ? "white" : Theme.accent
            }
        }
        contentItem: Item {
            id: drawing
            readonly property color ink: control.captionAction === "close" && control.emphasized ? "white" : root.targetWindow.active ? Theme.text : Theme.secondary
            Rectangle {visible: control.captionAction === "minimize"; anchors.centerIn: parent; width: 10; height: 1; color: drawing.ink}
            Rectangle {
                visible: control.captionAction === "maximize" || control.captionAction === "restore"
                anchors.centerIn: parent; anchors.horizontalCenterOffset: control.captionAction === "restore" ? 1 : 0
                anchors.verticalCenterOffset: control.captionAction === "restore" ? -1 : 0
                width: 10; height: 10; color: "transparent"; border.color: drawing.ink; border.width: 1
            }
            Rectangle {
                visible: control.captionAction === "restore"; anchors.centerIn: parent
                anchors.horizontalCenterOffset: -1; anchors.verticalCenterOffset: 1
                width: 10; height: 10; color: control.emphasized ? Theme.hover : root.surface; border.color: drawing.ink; border.width: 1
            }
            Item {
                visible: control.captionAction === "close"; anchors.centerIn: parent; width: 12; height: 12
                Rectangle {anchors.centerIn: parent; width: 13; height: 1; rotation: 45; color: drawing.ink}
                Rectangle {anchors.centerIn: parent; width: 13; height: 1; rotation: -45; color: drawing.ink}
            }
        }
    }
}
