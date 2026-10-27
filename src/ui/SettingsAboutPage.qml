pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    objectName: "settingsAboutPage"
    spacing: Theme.blockGap
    property string copiedContact: ""
    function copyContact(field, name) {
        field.selectAll()
        field.copy()
        field.deselect()
        copiedContact=name
        resetCopy.restart()
    }
    Timer {id: resetCopy;interval: 2000;onTriggered: page.copiedContact=""}
    SettingsGroup {
        heading: qsTr("Daymark")
        ColumnLayout {
            Layout.fillWidth:true;Layout.margins:16;spacing:8
            BrandWordmark {Layout.preferredWidth: 128;Layout.preferredHeight: 128*36/164}
            Label {objectName: "aboutVersion";text: qsTr("版本 %1").arg(Qt.application.version);Layout.fillWidth: true;color: Theme.secondary}
            Label {text: qsTr("简约而不简单。\n本地任务与时间安排，无需账号。");Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary}
        }
    }
    SettingsGroup {
        heading: qsTr("交流与反馈")
        SettingsRow {
            label: qsTr("QQ 交流群")
            RowLayout {
                Layout.fillWidth: true
                TextField {id: community;objectName: "aboutCommunity";Layout.fillWidth: true;Layout.minimumWidth: 0;text: "1057825113";readOnly: true;selectByMouse: true;Accessible.name: qsTr("QQ 交流群")}
                Button {objectName: "copyAboutCommunity";text: page.copiedContact==="community" ? qsTr("已复制") : qsTr("复制");Accessible.name: qsTr("复制 QQ 群号");onClicked: page.copyContact(community,"community")}
            }
        }
        SettingsRow {
            separator:false
            label: qsTr("反馈邮箱")
            RowLayout {
                Layout.fillWidth: true
                TextField {id: feedback;objectName: "aboutFeedback";Layout.fillWidth: true;Layout.minimumWidth: 0;text: "409962@qq.com";readOnly: true;selectByMouse: true;Accessible.name: qsTr("反馈邮箱")}
                Button {objectName: "copyAboutFeedback";text: page.copiedContact==="feedback" ? qsTr("已复制") : qsTr("复制");Accessible.name: qsTr("复制反馈邮箱");onClicked: page.copyContact(feedback,"feedback")}
            }
        }
    }
}
