pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS
Dialog {
    id: root
    objectName: "recurringSchedules"
    required property TaskWorkspace workspace
    property var schedules: []
    signal createSchedule()
    signal inspectSchedule(string id)
    title: qsTr("循环日程")
    width: Math.min(560*Theme.scale,parent.width-32);height: Math.min(520*Theme.scale,parent.height-32)
    modal: false;closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    function showSchedules(){schedules=workspace.recurringEvents();open()}
    Connections {target: root.workspace;function onStateChanged(){if(root.visible)root.schedules=root.workspace.recurringEvents()}}
    contentItem: ColumnLayout {
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("会议、生日和固定安排，在日历中按日期重复；任务的重复规则另行管理。")}
        ListView {
            id: list;Layout.fillWidth: true;Layout.fillHeight: true;clip: true;cacheBuffer: 0;spacing: 4
            DS.ScrollPhysics {target: list}
            model: root.schedules;ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row;required property var modelData;width: list.width
                contentItem: RowLayout {
                    AppIcon {name: "reload";ink: Theme.scheduleInk}
                    ColumnLayout {Layout.fillWidth: true;spacing: 3
                        Label {Layout.fillWidth: true;text: row.modelData.title;textFormat: Text.PlainText;elide: Text.ElideRight;font.weight: Font.DemiBold}
                        Label {Layout.fillWidth: true;text: row.modelData.ruleLabel+" · "+row.modelData.alertLabel;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    }
                    AppIcon {name: "right"}
                }
                Accessible.name: modelData.title+", "+modelData.ruleLabel
                onClicked: {root.close();root.inspectSchedule(modelData.eventId)}
            }
            Label {anchors.centerIn: parent;width: parent.width;wrapMode: Text.Wrap;horizontalAlignment: Text.AlignHCenter;visible: list.count===0;color: Theme.secondary;text: qsTr("还没有循环日程，先添加一个固定安排。")}
        }
    }
    footer: ActionBar {
        Button {text: qsTr("关闭");onClicked: root.close()}
        ActionButton {objectName: "createRecurringSchedule";symbol: "plus";text: qsTr("新建循环日程");highlighted: true;onClicked: {root.close();root.createSchedule()}}
    }
}
