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
    signal editEvent(string eventId)
    signal openTask(string taskId)
    title: qsTr("事件详情")
    width: Math.min(520*Theme.scale,parent.width-32)
    height: Math.min(implicitHeight,parent.height-32)
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    function refresh() {value=workspace.calendarEventPreview(eventId,occurrenceKey)}
    function showEvent(id,key) {eventId=id;occurrenceKey=key || "";refresh();open()}
    Connections {target: root.workspace;function onStateChanged(){if(root.visible)root.refresh()}}
    contentItem: ScrollView {
        ColumnLayout {
            width: root.availableWidth-12
            spacing: Theme.fieldGap
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.taskTitleSize;font.weight: Font.DemiBold;textFormat: Text.PlainText;text: root.value.title || qsTr("此安排已变更或删除")}
            Label {visible: !root.value.eventId;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary;text: qsTr("关闭后重新选择日期中的事件查看最新安排。")}
            Label {visible: !!root.value.eventId;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.value.contextLabel || ""}
            Label {objectName: "previewDate";Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.value.dateLabel || ""}
            Label {visible: !!root.value.notes;Layout.fillWidth: true;wrapMode: Text.Wrap;textFormat: Text.PlainText;text: root.value.notes || ""}
            FormGroup {
                visible: !!root.value.eventId
                compact: true
                heading: qsTr("循环与提醒")
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.value.ruleLabel || ""}
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.value.alertLabel || ""}
                Label {objectName: "previewScope";Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.value.scopeLabel || ""}
            }
            Repeater {
                model: root.value.links || []
                ItemDelegate {
                    id: linked
                    required property var modelData
                    Layout.fillWidth: true
                    Accessible.name: modelData.title
                    contentItem: Text {text: (linked.modelData.role==="followup"?qsTr("跟进 · "):qsTr("准备 · "))+linked.modelData.title;font.pixelSize: Theme.bodySize;color: Theme.text;textFormat: Text.PlainText;wrapMode: Text.Wrap}
                    onClicked: {root.close();root.openTask(modelData.taskId)}
                }
            }
        }
    }
    footer: ActionBar {
        Button {text: qsTr("关闭");onClicked: root.close()}
        Button {objectName: "editPreviewEvent";enabled: !!root.value.eventId && !root.workspace.busy;text: root.value.recurring?qsTr("编辑整个系列…"):qsTr("编辑事件…");onClicked: {const id=root.eventId;root.close();root.editEvent(id)}}
    }
}
