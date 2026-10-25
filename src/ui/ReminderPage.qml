pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
Page {
    id: root
    objectName: "reminderPage"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    signal openEvent(string eventId)
    title: qsTr("提醒记录")
    padding: 0
    background: Rectangle {color: Theme.content}
    signal backRequested()
    function close() {backRequested()}
    contentItem: ColumnLayout {
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("最近50条触发记录 · 应用运行时检查提醒");font.pixelSize: Theme.captionSize;color: Theme.secondary}
        ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
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
                onClicked: {if(modelData.eventId)root.openEvent(modelData.eventId);else root.openTask(modelData.taskId)}
                Accessible.name: modelData.title
                contentItem: ColumnLayout {
                    spacing: 4
                    Label {Layout.fillWidth: true;text: record.modelData.title;elide: Text.ElideRight}
                    Label {Layout.fillWidth: true;text: root.workspace.localReminder(record.modelData.firedAt);font.pixelSize: Theme.captionSize;color: Theme.secondary}
                }
            }
            EmptyState {anchors.centerIn: parent;width: Math.min(parent.width,400*Theme.scale);visible: reminders.count===0;symbol: "today";heading: qsTr("暂无触发提醒");detail: qsTr("为任务或日历日程设置提醒后，触发记录会显示在这里。")}
        }
    }
}
