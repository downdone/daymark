pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Dialog {
    id: root
    modal: true
    closePolicy: Popup.NoAutoClose
    width: Math.min(440,parent.width-32)
    contentItem: Label {text: qsTr("尚未保存的输入会丢失。");wrapMode: Text.Wrap}
    footer: ActionBar {
        Button {text: qsTr("继续编辑");onClicked: root.reject()}
        Button {text: qsTr("放弃修改");palette.buttonText: Theme.danger;onClicked: root.accept()}
    }
    Shortcut {sequence: "Escape";enabled: root.visible;onActivated: root.reject()}
}
