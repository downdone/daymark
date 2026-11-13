pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: page
    objectName: "settingsAboutPage"
    Layout.minimumWidth:0
    spacing: Theme.blockGap
    property string copiedContact: ""
    property var updateController: null
    signal helpRequested()
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
            Layout.fillWidth:true;Layout.minimumWidth:0;Layout.margins:16;spacing:8
            BrandLockup {objectName:"aboutBrand";Layout.preferredWidth: 208;Layout.preferredHeight: 208*72/320}
            Label {objectName: "aboutVersion";text: qsTr("版本 %1").arg(Qt.application.version);Layout.fillWidth: true;color: Theme.secondary}
            Label {text: qsTr("简约而不简单。\n本地管理任务与时间安排，登录账号可跨设备同步。");Layout.fillWidth: true;Layout.minimumWidth:0;wrapMode: Text.Wrap;color: Theme.secondary}
        }
    }
    SettingsGroup {
        heading: qsTr("版本与更新")
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth:0
            Layout.margins: 16
            spacing: 12
            Label {objectName:"updateStatus";text:page.updateController ? page.updateController.statusText : qsTr("此构建尚未提供自动更新。");wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.minimumWidth:0;color:Theme.secondary}
            ProgressBar {visible:page.updateController && page.updateController.busy;Layout.fillWidth:true;indeterminate:!page.updateController || page.updateController.progress<0;value:page.updateController ? Math.max(0,page.updateController.progress) : 0;Accessible.name:qsTr("更新下载进度")}
            GridLayout {
                Layout.fillWidth: true
                columns:width<340*Theme.scale ? 1 : 2
                columnSpacing:Theme.fieldGap;rowSpacing:Theme.fieldGap
                Button {objectName:"checkUpdates";Layout.fillWidth:true;Layout.minimumWidth:0;text:page.updateController && page.updateController.ready ? qsTr("立即重启") : qsTr("检查更新");enabled:page.updateController && page.updateController.supported && !page.updateController.busy;onClicked:{if(page.updateController.ready)page.updateController.restart();else page.updateController.check()}}
                Button {objectName:"updateReleaseNotes";Layout.fillWidth:true;Layout.minimumWidth:0;text:qsTr("发布说明");enabled:!!page.updateController;onClicked:page.updateController.openReleaseNotes()}
                Button {Layout.fillWidth:true;Layout.minimumWidth:0;text:qsTr("取消准备");visible:page.updateController && page.updateController.busy;onClicked:page.updateController.cancel()}
                Button {Layout.fillWidth:true;Layout.minimumWidth:0;text:qsTr("取消待应用更新");visible:page.updateController && page.updateController.ready && !page.updateController.busy;onClicked:page.updateController.cancelReady()}
            }
            CheckBox {Layout.fillWidth:true;Layout.minimumWidth:0;text:qsTr("每天自动检查并准备更新");enabled:page.updateController && page.updateController.supported;checked:page.updateController ? page.updateController.automatic : true;onToggled:if(page.updateController && checked!==page.updateController.automatic)page.updateController.automatic=checked}
        }
    }
    SettingsGroup {
                        heading:qsTr("帮助与说明")
                        SettingsLink {separator:false;objectName:"settingsHelp";symbol:"info";text:qsTr("使用提示");onClicked:page.helpRequested()}
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
