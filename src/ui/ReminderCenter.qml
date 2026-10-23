pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    objectName: "reminderCenter"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    title: qsTr("提醒中心")
    width: Math.min(560*Theme.scale,parent.width-32)
    height: Math.min(600,parent.height-32)
    modal: true
    standardButtons: Dialog.Close
    contentItem: ColumnLayout {
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("最近50条触发记录 · 应用运行时检查提醒");font.pixelSize: Theme.captionSize;color: Theme.secondary}
        ListView {
            id: reminders
            DS.ScrollPhysics {target: reminders}
            Layout.fillWidth: true;Layout.fillHeight: true
            clip: true
            model: root.workspace.notifications
            spacing: 4
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: record
                required property var modelData
                width: reminders.width
                onClicked: {root.close();root.openTask(modelData.taskId)}
                Accessible.name: modelData.title
                contentItem: ColumnLayout {
                    spacing: 4
                    Label {Layout.fillWidth: true;text: record.modelData.title;elide: Text.ElideRight}
                    Label {Layout.fillWidth: true;text: root.workspace.localReminder(record.modelData.firedAt);font.pixelSize: Theme.captionSize;color: Theme.secondary}
                }
            }
            EmptyState {anchors.centerIn: parent;width: Math.min(parent.width,400*Theme.scale);visible: reminders.count===0;symbol: "today";heading: qsTr("暂无触发提醒");detail: qsTr("为任务设置提醒后，触发记录会显示在这里。")}
        }
    }
}
