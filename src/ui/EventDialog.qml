pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    objectName: "eventDialog"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    property string eventId: ""
    property real version: 1
    property var links: []
    property var choices: []
    property bool dirty: false
    property var pendingAction: null
    title: eventId ? qsTr("编辑事件") : qsTr("新建事件")
    width: Math.min(700,parent.width-32)
    height: Math.min(680,parent.height-32)
    modal: true
    closePolicy: Popup.NoAutoClose
    function showEvent(id,date) {
        const e=id ? workspace.calendarEvent(id) : ({})
        eventId=id;version=e.rowVersion || 1
        eventTitle.text=e.title || "";notes.text=e.notes || ""
        allDay.checked=e.allDay===undefined ? true : e.allDay
        startDate.text=e.startDate || date || workspace.dateOffset(0);endDate.text=e.endDate || startDate.text
        startTime.text=e.startTime || "09:00";endTime.text=e.endTime || "10:00";zone.text=e.zone || workspace.systemTimeZone()
        links=e.links || [];search.clear();choices=workspace.eventTaskChoices("");dirty=false;open();eventTitle.forceActiveFocus()
    }
    function safely(action){if(workspace.busy)return;if(dirty){pendingAction=action;discard.open()}else action()}
    function save() {workspace.saveEvent({eventId:eventId,rowVersion:version,title:eventTitle.text,notes:notes.text,allDay:allDay.checked,startDate:startDate.text,endDate:endDate.text,startTime:startTime.text,endTime:endTime.text,zone:zone.text,links:links})}
    function addLink(task) {if(links.some(item=>item.taskId===task.taskId))return;links=links.concat([{taskId:task.taskId,title:task.title,role:"preparation"}]);dirty=true}
    Shortcut {sequence: "Escape";enabled: root.visible && !discard.visible && !confirmDelete.visible && !startDate.pickerVisible && !endDate.pickerVisible;onActivated: root.safely(function(){root.close()})}
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: root.availableWidth-12
            spacing: Theme.fieldGap
            enabled: !root.workspace.busy || root.workspace.backgroundBusy
            Label {text: qsTr("标题")}
            TextField {id: eventTitle;objectName: "eventTitle";Layout.fillWidth: true;placeholderText: qsTr("例如：产品评审");onTextEdited: root.dirty=true}
            Label {text: qsTr("备注")}
            TextArea {id: notes;Layout.fillWidth: true;wrapMode: Text.Wrap;placeholderText: qsTr("地点、准备说明等");onTextChanged: if(activeFocus)root.dirty=true}
            CheckBox {id: allDay;text: qsTr("全天事件");onClicked: root.dirty=true}
            GridLayout {
                Layout.fillWidth: true
                columns: root.availableWidth>=560*Theme.scale ? 2 : 1
                columnSpacing: Theme.fieldGap
                rowSpacing: Theme.fieldGap
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {text: qsTr("开始日期")}
                    RowLayout {
                        Layout.fillWidth: true
                        DateField {id: startDate;workspace: root.workspace;dateLabel: qsTr("事件开始日期");Layout.fillWidth: true;onTextEdited: root.dirty=true;onDateChosen: root.dirty=true}
                    }
                    TextField {id: startTime;visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "HH:mm";Accessible.name: qsTr("开始时间");onTextEdited: root.dirty=true}
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {text: allDay.checked ? qsTr("结束日期（含当天）") : qsTr("结束日期")}
                    RowLayout {
                        Layout.fillWidth: true
                        DateField {id: endDate;workspace: root.workspace;dateLabel: qsTr("事件结束日期");Layout.fillWidth: true;onTextEdited: root.dirty=true;onDateChosen: root.dirty=true}
                    }
                    TextField {id: endTime;visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "HH:mm";Accessible.name: qsTr("结束时间");onTextEdited: root.dirty=true}
                }
            }
            Label {visible: !allDay.checked;text: qsTr("输入时间所属时区")}
            TextField {id: zone;visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "Asia/Shanghai";onTextEdited: root.dirty=true}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("事件按本机时区显示在日历。修改事件日期不会自动移动关联任务的计划或截止日期。")}
            SectionLabel {text: qsTr("关联任务")}
            Repeater {
                model: root.links
                ColumnLayout {
                    id: linked
                    required property int index
                    required property var modelData
                    Layout.fillWidth: true
                    ItemDelegate {Layout.fillWidth: true;text: linked.modelData.title || linked.modelData.taskId;onClicked: {const id=linked.modelData.taskId;root.safely(function(){root.close();root.openTask(id)})}}
                    RowLayout {
                        Layout.fillWidth: true
                        ComboBox {Layout.fillWidth: true;model: [qsTr("准备任务"),qsTr("跟进任务")];currentIndex: linked.modelData.role==="followup" ? 1 : 0;onActivated: {const next=root.links.slice();next[linked.index]=Object.assign({},next[linked.index],{role:currentIndex===1 ? "followup" : "preparation"});root.links=next;root.dirty=true}}
                        ToolButton {contentItem: AppIcon {name: "close"} Accessible.name: qsTr("解除关联，保留任务");onClicked: {const next=root.links.slice();next.splice(linked.index,1);root.links=next;root.dirty=true}}
                    }
                }
            }
            TextField {id: search;Layout.fillWidth: true;placeholderText: qsTr("搜索要关联的任务（最多显示200项）");onTextChanged: searchDelay.restart()}
            Timer {id: searchDelay;interval: 250;onTriggered: root.choices=root.workspace.eventTaskChoices(search.text)}
            ListView {
                id: motionSurface1
                DS.ScrollPhysics {target: motionSurface1}
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(180,contentHeight)
                clip: true
                model: root.choices
                ScrollBar.vertical: ScrollBar {}
                delegate: ItemDelegate {
                    id: candidate
                    required property var modelData
                    width: ListView.view.width
                    text: root.links.some(item=>item.taskId===modelData.taskId) ? qsTr("已关联 · %1").arg(modelData.title) : modelData.title
                    enabled: !root.links.some(item=>item.taskId===modelData.taskId)
                    onClicked: root.addLink(candidate.modelData)
                }
            }
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText}
        }
    }
    footer: ActionBar {
        Button {visible: !!root.eventId;text: qsTr("删除事件…");flat: true;palette.buttonText: Theme.danger;enabled: !root.workspace.busy;onClicked: confirmDelete.open()}
        Button {visible: !!root.eventId;text: qsTr("重新载入");enabled: !root.workspace.busy;onClicked: root.safely(function(){root.showEvent(root.eventId,"")})}
        Button {text: qsTr("返回");enabled: !root.workspace.busy;onClicked: root.safely(function(){root.close()})}
        Button {objectName: "saveEvent";text: qsTr("保存事件");highlighted: true;enabled: !root.workspace.busy;onClicked: root.save()}
    }
    Dialog {id: discard;objectName: "eventDiscard";parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);title: qsTr("放弃未保存的事件修改？");modal: true;standardButtons: Dialog.Ok|Dialog.Cancel;onAccepted: {root.dirty=false;const action=root.pendingAction;root.pendingAction=null;if(action)action()}}
    Dialog {id: confirmDelete;parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);title: qsTr("删除事件？关联任务会保留。");modal: true;standardButtons: Dialog.Ok|Dialog.Cancel;onAccepted: root.workspace.deleteEvent(root.eventId,root.version)}
    Connections {target: root.workspace;function onOperationFinished(operation,success){if(root.visible && success && (operation==="eventSave" || operation==="eventDelete")){root.dirty=false;root.close()}}}
}
