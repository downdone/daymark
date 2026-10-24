pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    objectName: "eventPreview"
    required property TaskWorkspace workspace
    property string eventId: ""
    property string occurrenceKey: ""
    property var value: ({})
    readonly property bool hasTimeline: !!value.timeline && value.timeline.start!==undefined
    signal editEvent(string eventId)
    signal openTask(string taskId)
    title: qsTr("日程详情")
    width: Math.min(520*Theme.scale,parent.width-32)
    height: Math.min(implicitHeight,parent.height-32)
    modal: false;closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle {color: Theme.sidebar;radius: 20;border.color: Theme.separator}
    function refresh(){value=workspace.calendarEventPreview(eventId,occurrenceKey)}
    function showEvent(id,key){eventId=id;occurrenceKey=key || "";refresh();open()}
    Connections {target: root.workspace;function onStateChanged(){if(root.visible)root.refresh()} function onOperationFinished(operation,success){if(root.visible && operation==="eventDelete" && success)root.close()}}
    header: RowLayout {
        spacing: 8
        IconToolButton {contentItem: AppIcon {name: "close"} Accessible.name: qsTr("关闭日程详情");onClicked: root.close()}
        Label {text: root.title;Layout.fillWidth: true;font.pixelSize: Theme.bodySize;color: Theme.secondary}
        Button {objectName: "editPreviewEvent";text: qsTr("编辑");enabled: !!root.value.eventId && !root.workspace.busy;flat: true;palette.buttonText: Theme.accent;onClicked: {const id=root.eventId;root.close();root.editEvent(id)}}
    }
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: root.availableWidth-12;spacing: 16
            RowLayout {
                Layout.fillWidth: true;spacing: 12
                Rectangle {Layout.fillHeight: true;Layout.preferredWidth: 4;radius: 2;color: Theme.accent}
                ColumnLayout {Layout.fillWidth: true;spacing: 4
                    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.taskTitleSize;font.weight: Font.DemiBold;textFormat: Text.PlainText;text: root.value.title || qsTr("此安排已变更或删除")}
                    Label {visible: !root.value.eventId;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary;text: qsTr("关闭后重新选择日期中的日程查看最新安排。")}
                    Label {objectName: "previewDate";Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.value.dateLabel || ""}
                    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary;text: (root.value.recurring ? "↻ " : "")+(root.value.ruleLabel || "")}
                }
            }
            Control {
                visible: root.hasTimeline
                Layout.fillWidth: true;implicitHeight: 168*Theme.scale;padding: 12
                background: Rectangle {radius: 16;color: Theme.content}
                contentItem: Item {
                    Repeater {model: root.value.timeline ? root.value.timeline.labels || [] : []
                        Item {required property var modelData;y: modelData.position*(parent.height-18);width: parent.width;height: 18
                            Text {text: parent.modelData.label;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                            Rectangle {x: 52;width: Math.max(0,parent.width-52);height: 1;anchors.verticalCenter: parent.verticalCenter;color: Theme.separator}
                        }
                    }
                    Rectangle {
                        x: 56;width: Math.max(0,parent.width-60)
                        y: root.hasTimeline ? root.value.timeline.start*(parent.height-18)+9 : 0
                        height: Math.max(22,root.hasTimeline ? (root.value.timeline.end-root.value.timeline.start)*(parent.height-18) : 0)
                        radius: 6;color: Theme.selected
                        Text {anchors.fill: parent;anchors.margins: 4;text: root.value.timeLabel || "";font.pixelSize: Theme.captionSize;color: Theme.accent;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
                    }
                }
            }
            FormGroup {
                visible: !!root.value.eventId;heading: qsTr("日程选项");compact: true
                RowLayout {Layout.fillWidth: true;AppIcon {name: "calendar"} Label {text: qsTr("日历");Layout.fillWidth: true} Label {text: qsTr("本地日历");color: Theme.secondary}}
                RowLayout {Layout.fillWidth: true;AppIcon {name: "bell"} Label {text: qsTr("提醒");Layout.fillWidth: true} Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.value.alertLabel || "";horizontalAlignment: Text.AlignRight;color: Theme.secondary}}
                Label {objectName: "previewScope";Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.value.scopeLabel || ""}
            }
            Label {visible: !!root.value.notes;Layout.fillWidth: true;wrapMode: Text.Wrap;textFormat: Text.PlainText;text: root.value.notes || ""}
            Repeater {model: root.value.links || []
                ItemDelegate {id: linked;required property var modelData;Layout.fillWidth: true;Accessible.name: modelData.title
                    contentItem: Text {text: (linked.modelData.role==="followup" ? qsTr("跟进 · ") : qsTr("准备 · "))+linked.modelData.title;font.pixelSize: Theme.bodySize;color: Theme.text;wrapMode: Text.Wrap}
                    onClicked: {root.close();root.openTask(modelData.taskId)}
                }
            }
            Button {Layout.fillWidth: true;visible: !!root.value.eventId;enabled: !root.workspace.busy;text: root.value.recurring ? qsTr("删除整个循环日程…") : qsTr("删除日程…");palette.buttonText: Theme.danger;onClicked: confirmDelete.open()}
            Label {visible: root.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.workspace.errorText;color: Theme.danger}
        }
    }
    Dialog {id: confirmDelete;parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);title: root.value.recurring ? qsTr("删除整个系列？关联任务会保留。") : qsTr("删除日程？关联任务会保留。");modal: true;footer: ActionBar {Button {text: qsTr("取消");onClicked: confirmDelete.reject()} Button {text: qsTr("删除");palette.buttonText: Theme.danger;enabled: !root.workspace.busy;onClicked: confirmDelete.accept()}} onAccepted: root.workspace.deleteEvent(root.eventId,root.value.rowVersion)}
}
