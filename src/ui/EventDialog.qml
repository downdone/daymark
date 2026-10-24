pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    background: Rectangle {color: Theme.sidebar;radius: 16;border.color: Theme.separator}
    objectName: "eventDialog"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    property string eventId: ""
    property real version: 1
    property var links: []
    property var choices: []
    property bool dirty: false
    property var pendingAction: null
    readonly property var frequencies: ["NONE","DAILY","WEEKLY","MONTHLY","YEARLY"]
    readonly property var alertValues: [-1,0,5,15,30,60,1440]
    title: eventId ? qsTr("编辑日程") : qsTr("新建日程")
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
        frequency.currentIndex=frequencies.indexOf(e.repeatFrequency || "NONE");repeatInterval.value=e.repeatInterval || 1;repeatUntil.text=e.repeatUntil || "";alert.currentIndex=alertValues.indexOf(e.alertMinutes===undefined ? -1 : e.alertMinutes);
        links=e.links || [];search.clear();choices=workspace.eventTaskChoices("");dirty=false;open();eventTitle.forceActiveFocus();const viewport=formBody.contentItem as Flickable;if(viewport){viewport.cancelFlick();viewport.contentY=0}
    }
    function showRecurring(date){showEvent("",date);frequency.currentIndex=2;repeatInterval.value=1;dirty=true}
    function safely(action){if(workspace.busy)return;if(dirty){pendingAction=action;discard.open()}else action()}
    function save() {workspace.saveEvent({eventId:eventId,rowVersion:version,title:eventTitle.text,notes:notes.text,allDay:allDay.checked,startDate:startDate.text,endDate:endDate.text,startTime:startTime.text,endTime:endTime.text,zone:zone.text,links:links,repeatFrequency:frequencies[frequency.currentIndex],repeatInterval:repeatInterval.value,repeatUntil:frequency.currentIndex>0 ? repeatUntil.text : "",alertMinutes:alertValues[alert.currentIndex]})}
    function showSeriesForVerification() {
        const viewport=formBody.contentItem as Flickable
        if(!viewport)return false
        viewport.cancelFlick()
        viewport.contentY=Math.max(0,Math.min(seriesGroup.mapToItem(viewport.contentItem,0,0).y,viewport.contentHeight-viewport.height))
        const top=seriesGroup.mapToItem(formBody,0,0).y
        return top>=-1 && top<40
    }
    function addLink(task) {if(links.some(item=>item.taskId===task.taskId))return;links=links.concat([{taskId:task.taskId,title:task.title,role:"preparation"}]);dirty=true}
    Shortcut {sequence: "Escape";enabled: root.visible && !discard.visible && !confirmDelete.visible && !startDate.pickerVisible && !endDate.pickerVisible && !repeatUntil.pickerVisible && !startTime.pickerVisible && !endTime.pickerVisible && !frequency.popup.visible && !alert.popup.visible;onActivated: root.safely(function(){root.close()})}
    contentItem: ScrollView {
        id: formBody
        clip: true
        ColumnLayout {
            width: root.availableWidth-12
            spacing: Theme.fieldGap
            enabled: !root.workspace.busy || root.workspace.backgroundBusy
            FormGroup {
                heading: qsTr("日程内容")
            Label {text: qsTr("标题")}
            TextField {id: eventTitle;Accessible.name: qsTr("日程标题");objectName: "eventTitle";Layout.fillWidth: true;placeholderText: qsTr("例如：产品评审");onTextEdited: root.dirty=true}
            Label {text: qsTr("备注")}
            TextArea {id: notes;Accessible.name: qsTr("日程备注");Layout.fillWidth: true;wrapMode: Text.Wrap;placeholderText: qsTr("地点、准备说明等");onTextChanged: if(activeFocus)root.dirty=true}
            }
            FormGroup {
                heading: qsTr("时间安排")
            SettingSwitch {id: allDay;objectName: "eventAllDay";symbol: "calendar";text: qsTr("全天日程");onClicked: root.dirty=true}
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
                        DateField {id: startDate;objectName: "eventStartDate";onCalendarFocusRequested: startDateCalendar.focusSelectedDay();inlineMode: true;allowClear: false;onInlineExpandedChanged: if(inlineExpanded)endDate.closePicker();workspace: root.workspace;dateLabel: qsTr("日程开始日期");Layout.fillWidth: true;onTextEdited: root.dirty=true;onDateChosen: root.dirty=true}
                    }
                    CalendarPicker {id: startDateCalendar;onDismissRequested: {startDate.closePicker();startDate.forceActiveFocus()} Layout.fillWidth: true;visible: startDate.inlineExpanded;workspace: root.workspace;month: startDate.month;selectedDate: startDate.text;allowClear: false;onDateSelected: function(value){startDate.chooseDate(value)};onMoveMonth: function(offset){startDate.month=root.workspace.datePicker(startDate.month.anchor,offset)}}
                    TimeField {id: startTime;objectName: "eventStartTime";timeLabel: qsTr("开始时间");onTimeChosen: root.dirty=true;visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "HH:mm";Accessible.name: qsTr("开始时间");onTextEdited: root.dirty=true}
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label {text: allDay.checked ? qsTr("结束日期（含当天）") : qsTr("结束日期")}
                    RowLayout {
                        Layout.fillWidth: true
                        DateField {id: endDate;objectName: "eventEndDate";onCalendarFocusRequested: endDateCalendar.focusSelectedDay();inlineMode: true;allowClear: false;onInlineExpandedChanged: if(inlineExpanded)startDate.closePicker();workspace: root.workspace;dateLabel: qsTr("日程结束日期");Layout.fillWidth: true;onTextEdited: root.dirty=true;onDateChosen: root.dirty=true}
                    }
                    CalendarPicker {id: endDateCalendar;onDismissRequested: {endDate.closePicker();endDate.forceActiveFocus()} Layout.fillWidth: true;visible: endDate.inlineExpanded;workspace: root.workspace;month: endDate.month;selectedDate: endDate.text;allowClear: false;onDateSelected: function(value){endDate.chooseDate(value)};onMoveMonth: function(offset){endDate.month=root.workspace.datePicker(endDate.month.anchor,offset)}}
                    TimeField {id: endTime;objectName: "eventEndTime";timeLabel: qsTr("结束时间");onTimeChosen: root.dirty=true;visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "HH:mm";Accessible.name: qsTr("结束时间");onTextEdited: root.dirty=true}
                }
            }
            Label {visible: !allDay.checked;text: qsTr("输入时间所属时区")}
            TextField {id: zone;Accessible.name: qsTr("日程时区");visible: !allDay.checked;Layout.fillWidth: true;placeholderText: "Asia/Shanghai";onTextEdited: root.dirty=true}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("日程按本机时区显示在日历。修改日程日期不会自动移动关联任务的计划或截止日期。")}
            }
            FormGroup {
                id: seriesGroup
                heading: qsTr("循环与提醒")
                RowLayout {Layout.fillWidth: true
                    Label {text: qsTr("重复");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {id: frequency;objectName: "eventFrequency";Layout.fillWidth: true;Accessible.name: qsTr("日程重复周期");model: [qsTr("不重复"),qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年")];onActivated: root.dirty=true}
                }
                RowLayout {visible: frequency.currentIndex>0;Label {text: qsTr("每隔")} SpinBox {id: repeatInterval;from: 1;to: 365;editable: true;Accessible.name: qsTr("日程重复间隔");onValueModified: root.dirty=true} Label {text: ["",qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年")][frequency.currentIndex]}}
                DateField {id: repeatUntil;Layout.fillWidth: true;visible: frequency.currentIndex>0;workspace: root.workspace;dateLabel: qsTr("循环结束日期");placeholderText: qsTr("结束日期 · 留空持续重复");onTextEdited: root.dirty=true;onDateChosen: root.dirty=true}
                Label {visible: frequency.currentIndex>0;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存和删除作用于整个系列。无效月日及夏令时歧义实例跳过；当前不支持修改单次。")}
                RowLayout {Layout.fillWidth: true
                    Label {text: qsTr("提醒");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {id: alert;objectName: "eventAlert";Layout.fillWidth: true;Accessible.name: qsTr("日程提醒");model: [qsTr("不提醒"),qsTr("开始时"),qsTr("提前5分钟"),qsTr("提前15分钟"),qsTr("提前30分钟"),qsTr("提前1小时"),qsTr("提前1天")];onActivated: root.dirty=true}
                }
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: allDay.checked ? qsTr("全天日程按当天09:00计算。应用运行时检查；启动补报最近24小时到期提醒。") : qsTr("按日程所属时区的开始时间计算。应用运行时检查，退出期间没有后台提醒。")}
            }
            FormGroup {
                heading: qsTr("关联任务")
                detail: qsTr("关联准备或跟进任务；日程改期不会改动任务日期。")
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
                        IconToolButton {contentItem: AppIcon {name: "close"} Accessible.name: qsTr("解除关联，保留任务");onClicked: {const next=root.links.slice();next.splice(linked.index,1);root.links=next;root.dirty=true}}
                    }
                }
            }
            TextField {id: search;Accessible.name: qsTr("搜索关联任务");Layout.fillWidth: true;placeholderText: qsTr("搜索要关联的任务（最多显示200项）");onTextChanged: searchDelay.restart()}
            Timer {id: searchDelay;interval: 250;onTriggered: root.choices=root.workspace.eventTaskChoices(search.text)}
            ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
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
            }
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText}
        }
    }
    footer: ActionBar {
        ActionButton {symbol: "trash";visible: !!root.eventId;text: qsTr("删除日程…");flat: true;palette.buttonText: Theme.danger;enabled: !root.workspace.busy;onClicked: confirmDelete.open()}
        WidgetAction {symbol: "reload";description: qsTr("重新载入日程，放弃修改前会确认");visible: !!root.eventId;enabled: !root.workspace.busy;onClicked: root.safely(function(){root.showEvent(root.eventId,"")})}
        Button {objectName: "returnFromEvent";text: qsTr("返回");enabled: !root.workspace.busy;onClicked: root.safely(function(){root.close()})}
        Button {objectName: "saveEvent";text: qsTr("保存日程");highlighted: true;enabled: !root.workspace.busy;onClicked: root.save()}
    }
    DiscardChangesDialog {id: discard;objectName: "eventDiscard";parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);title: qsTr("放弃未保存的日程修改？");modal: true;onRejected: root.pendingAction=null;onAccepted: {root.dirty=false;const action=root.pendingAction;root.pendingAction=null;if(action)action()}}
    Dialog {id: confirmDelete;parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);title: frequency.currentIndex>0 ? qsTr("删除整个循环系列？关联任务会保留。") : qsTr("删除日程？关联任务会保留。");modal: true;onRejected: root.pendingAction=null;onAccepted: root.workspace.deleteEvent(root.eventId,root.version)}
    Connections {target: root.workspace;function onOperationFinished(operation,success){if(root.visible && success && (operation==="eventSave" || operation==="eventDelete")){root.dirty=false;root.close()}}}
}
