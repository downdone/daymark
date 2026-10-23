pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    objectName: "calendarPanel"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    signal completeTask(string taskId,real version,string action)
    property string selectedDate: workspace.dateOffset(0)
    readonly property var days: workspace.calendarDays
    readonly property var selectedDay: days.find(day=>day.date===selectedDate) || ({date:"",entries:[],events:[]})
    readonly property bool compact: width<800*Theme.scale
    readonly property bool modalOpen: eventEditor.visible || moveDialog.visible || unscheduled.visible
    property bool showTasks: true
    property bool showEvents: true
    property var moving: ({})
    property var draggedEntry: null
    property bool draggingMarker: false
    spacing: Theme.gap
    function entries(day) {return (showTasks ? day.entries : []).concat(showEvents ? day.events : [])}
    function kindText(kind) {return kind==="both" ? qsTr("计划＋截止") : kind==="deadline" ? qsTr("截止") : qsTr("计划")}
    function offerMove(entry,date) {moving=entry;targetDate.text=date;markerKind.currentIndex=entry.kind==="deadline" ? 1 : 0;moveDialog.open()}
    function openEvent(id) {eventEditor.showEvent(id,selectedDate)}
    function protectClose(action) {if(eventEditor.visible && eventEditor.dirty){eventEditor.safely(function(){eventEditor.close();action()});return true}return false}
    function showAgenda() {const flick=body.contentItem as Flickable;if(flick)flick.contentY=Math.max(0,Math.min(flick.contentHeight-flick.height,agendaSection.y));newTitle.forceActiveFocus()}
    function focusSelected() {for(let i=0;i<days.length;i++)if(days[i].date===selectedDate){const tile=dayCells.itemAt(i);if(tile){tile.forceActiveFocus();const flick=body.contentItem as Flickable;if(flick){const y=tile.mapToItem(flick.contentItem,0,0).y;if(y<flick.contentY)flick.contentY=y;else if(y+tile.height>flick.contentY+flick.height)flick.contentY=y+tile.height-flick.height}}break}}
    function moveSelection(offset) {
        const target=workspace.shiftDate(selectedDate,offset)
        selectedDate=target
        if(!days.some(day=>day.date===target))workspace.selectCalendarDate(target)
        Qt.callLater(focusSelected)
    }
    Connections {
        target: panel.workspace
        function onStateChanged() {if(!panel.days.some(day=>day.date===panel.selectedDate)){const day=panel.days.find(day=>day.inMonth);if(day)panel.selectedDate=day.date}}
        function onOperationFinished(operation,success) {
            if(success && operation==="calendarCreate")newTitle.clear()
            if(success && operation==="calendarMove")moveDialog.close()
        }
    }
    RowLayout {
        Layout.fillWidth: true
        ToolButton {contentItem: AppIcon {name:"left"} Accessible.name: qsTr("上一页");onClicked: panel.workspace.stepCalendar(-1)}
        Label {text: panel.workspace.calendarTitle;font.pixelSize: Theme.headingSize;font.bold: true;Layout.fillWidth: true;elide: Text.ElideRight}
        Button {text: qsTr("今天");flat: true;onClicked: {panel.selectedDate=panel.workspace.dateOffset(0);panel.workspace.stepCalendar(0)}}
        ToolButton {contentItem: AppIcon {name:"right"} Accessible.name: qsTr("下一页");onClicked: panel.workspace.stepCalendar(1)}
        Button {text: panel.workspace.view==="week" ? qsTr("月") : qsTr("周");onClicked: panel.workspace.setView(panel.workspace.view==="week" ? "month" : "week")}
    }
    Flow {
        Layout.fillWidth: true
        spacing: Theme.gap
        Button {text: qsTr("新建事件");highlighted: true;enabled: panel.workspace.ready && !panel.workspace.busy;onClicked: panel.openEvent("")}
        Button {
            text: qsTr("显示…")
            onClicked: displayMenu.open()
            Menu {
                id: displayMenu
                MenuItem {text: qsTr("任务计划与截止");checkable: true;checked: panel.showTasks;onTriggered: panel.showTasks=checked}
                MenuItem {text: qsTr("事件");checkable: true;checked: panel.showEvents;onTriggered: panel.showEvents=checked}
                MenuItem {text: qsTr("已完成任务");checkable: true;checked: panel.workspace.calendarCompleted;onTriggered: panel.workspace.calendarCompleted=checked}
                MenuSeparator {}
                MenuItem {text: qsTr("周日为每周首日");checkable: true;checked: panel.workspace.sundayFirst;onTriggered: panel.workspace.sundayFirst=checked}
            }
        }
        Button {text: qsTr("未排期（%1）").arg(panel.workspace.unscheduledTasks.length);flat: true;onClicked: unscheduled.open()}
        Button {visible: panel.compact;text: qsTr("当天议程");onClicked: panel.showAgenda()}
        Button {visible: panel.workspace.canUndoEvent;text: qsTr("撤销删除事件");enabled: !panel.workspace.busy;onClicked: panel.workspace.undoEventDelete()}
    }
    ScrollView {
        id: body
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        GridLayout {
            width: body.availableWidth
            height: Math.max(body.availableHeight,implicitHeight)
            columns: panel.compact ? 1 : 2
            columnSpacing: Theme.panelPadding
            rowSpacing: Theme.fieldGap
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: !panel.compact
                Layout.preferredHeight: panel.compact ? (panel.workspace.view==="week" ? 160 : 6*Math.max(58,Theme.bodySize+Theme.captionSize+14))+28 : -1
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: panel.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
                        Label {required property string modelData;text: modelData;Layout.fillWidth: true;Layout.preferredWidth: 1;horizontalAlignment: Text.AlignHCenter;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    }
                }
                GridLayout {
                    columns: 7
                    rowSpacing: 3
                    columnSpacing: 3
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Repeater {
                        id: dayCells
                        model: panel.days
                        ItemDelegate {
                            id: dayCell
                            required property var modelData
                            readonly property var items: panel.entries(modelData)
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 1
                            Layout.minimumWidth: 32
                            Layout.minimumHeight: panel.compact ? Math.max(58,Theme.bodySize+Theme.captionSize+14) : Math.max(42,Theme.bodySize+12)
                            padding: 4
                            highlighted: panel.selectedDate===modelData.date
                            focusPolicy: highlighted ? Qt.StrongFocus : Qt.ClickFocus
                            onActiveFocusChanged: if(activeFocus)panel.selectedDate=modelData.date
                            Accessible.name: modelData.date+qsTr("，%1项").arg(items.length)
                            onClicked: panel.selectedDate=modelData.date
                            onDoubleClicked: {panel.selectedDate=modelData.date;newTitle.forceActiveFocus()}
                            Keys.onLeftPressed: function(event){panel.moveSelection(-1);event.accepted=true}
                            Keys.onRightPressed: function(event){panel.moveSelection(1);event.accepted=true}
                            Keys.onUpPressed: function(event){panel.moveSelection(-7);event.accepted=true}
                            Keys.onDownPressed: function(event){panel.moveSelection(7);event.accepted=true}
                            contentItem: Column {
                                spacing: 1
                                clip: true
                                Label {width: dayCell.availableWidth;text: dayCell.modelData.day;color: dayCell.modelData.today ? Theme.accent : dayCell.modelData.inMonth ? Theme.text : Theme.secondary;font.bold: dayCell.modelData.today}
                                Repeater {
                                    model: dayCell.items.slice(0,panel.compact ? 0 : panel.workspace.view==="week" ? 8 : 3)
                                    Label {
                                        id: marker
                                        required property var modelData
                                        width: dayCell.availableWidth
                                        text: (modelData.eventId ? "▰ " : modelData.taskStatus==="COMPLETED" ? "✓ " : modelData.kind==="deadline" ? "⚑ " : "○ ")+modelData.title
                                        font.strikeout: modelData.taskStatus==="COMPLETED"
                                        elide: Text.ElideRight
                                        font.pixelSize: Theme.captionSize
                                        color: modelData.eventId ? Theme.text : modelData.kind==="start" ? Theme.accent : Theme.danger
                                        TapHandler {onTapped: {panel.selectedDate=dayCell.modelData.date;if(marker.modelData.eventId)panel.openEvent(marker.modelData.eventId);else panel.openTask(marker.modelData.taskId)}}
                                        Drag.active: markerDrag.active
                                        Drag.source: marker
                                        DragHandler {id: markerDrag;target: null;enabled: !marker.modelData.eventId;onActiveChanged: {panel.draggingMarker=active;if(active)panel.draggedEntry=marker.modelData;else {marker.Drag.drop();panel.draggedEntry=null}}}
                                    }
                                }
                                Label {
                                    width: dayCell.availableWidth
                                    visible: panel.compact ? dayCell.items.length>0 : dayCell.items.length>(panel.workspace.view==="week" ? 8 : 3)
                                    text: panel.compact ? qsTr("%1项").arg(dayCell.items.length) : "+"+(dayCell.items.length-(panel.workspace.view==="week" ? 8 : 3))
                                    font.pixelSize: Theme.captionSize
                                    color: Theme.accent
                                    TapHandler {onTapped: {panel.selectedDate=dayCell.modelData.date;panel.showAgenda()}}
                                }
                            }
                            DropArea {anchors.fill: parent;onDropped: function(drop){if(panel.draggedEntry){panel.offerMove(panel.draggedEntry,dayCell.modelData.date);drop.acceptProposedAction()}}}
                        }
                    }
                }
            }
            ColumnLayout {
                id: agendaSection
                objectName: "calendarAgenda"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: panel.compact ? -1 : 320*Theme.scale
                Layout.minimumWidth: panel.compact ? 0 : 280*Theme.scale
                Layout.maximumWidth: panel.compact ? Infinity : 380*Theme.scale
                Layout.preferredHeight: panel.compact ? Math.max(320,Theme.bodySize*14) : -1
                Label {text: panel.selectedDate+qsTr(" · 当天议程");font.pixelSize: Theme.sectionSize;font.bold: true;Layout.fillWidth: true;wrapMode: Text.Wrap}
                TextField {id: newTitle;Layout.fillWidth: true;placeholderText: qsTr("当天新任务，Enter 添加");enabled: !!panel.selectedDate && !panel.workspace.busy;onAccepted: panel.workspace.createForDate(text,panel.selectedDate)}
                Label {text: qsTr("○ 任务 · 计划 / 截止   ▰ 事件");Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ListView {
                    DS.ScrollPhysics {target: agenda}
                    id: agenda
                    objectName: "calendarAgendaList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.entries(panel.selectedDay)
                    spacing: Theme.fieldGap
                    ScrollBar.vertical: ScrollBar {}
                    Label {anchors.centerIn: parent;width: parent.width;wrapMode: Text.Wrap;horizontalAlignment: Text.AlignHCenter;color: Theme.secondary;visible: agenda.count===0;text: qsTr("这一天没有显示中的任务或事件")}
                    delegate: Item {
                        id: agendaRow
                        required property var modelData
                        width: agenda.width
                        height: rowContent.implicitHeight+8
                        ColumnLayout {
                            id: rowContent
                            width: parent.width
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                ToolButton {
                                    objectName: "calendarComplete-"+(agendaRow.modelData.taskId || "")
                                    visible: !agendaRow.modelData.eventId
                                    contentItem: AppIcon {name: agendaRow.modelData.taskStatus==="COMPLETED" ? "check" : "circle"}
                                    Accessible.name: agendaRow.modelData.taskStatus==="COMPLETED" ? qsTr("恢复任务") : qsTr("完成任务")
                                    enabled: !panel.workspace.busy
                                    onClicked: panel.completeTask(agendaRow.modelData.taskId,agendaRow.modelData.rowVersion,agendaRow.modelData.taskStatus==="COMPLETED" ? "reopen" : "complete")
                                }
                                ItemDelegate {
                                    Layout.fillWidth: true
                                    Accessible.name: agendaRow.modelData.title
                                    implicitHeight: contentItem.implicitHeight+16
                                    contentItem: Text {text: agendaRow.modelData.title;font.pixelSize: Theme.bodySize;color: Theme.text;wrapMode: Text.Wrap;font.strikeout: agendaRow.modelData.taskStatus==="COMPLETED"}
                                    onClicked: {if(agendaRow.modelData.eventId)panel.openEvent(agendaRow.modelData.eventId);else panel.openTask(agendaRow.modelData.taskId)}
                                }
                            }
                            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: agendaRow.modelData.eventId ? qsTr("事件 · %1（本机时区）").arg(agendaRow.modelData.timeLabel) : qsTr("任务 · %1").arg(panel.kindText(agendaRow.modelData.kind))}
                            Button {visible: !agendaRow.modelData.eventId;text: qsTr("改期…");flat: true;enabled: !panel.workspace.busy;onClicked: panel.offerMove(agendaRow.modelData,panel.selectedDate)}
                            Repeater {
                                model: agendaRow.modelData.eventId ? agendaRow.modelData.links : []
                                ItemDelegate {
                                    id: associated
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Accessible.name: modelData.title
                                    contentItem: Text {text: (associated.modelData.role==="followup" ? qsTr("跟进 · ") : qsTr("准备 · "))+associated.modelData.title;font.pixelSize: Theme.captionSize;color: Theme.secondary;wrapMode: Text.Wrap;font.strikeout: associated.modelData.taskStatus==="COMPLETED"}
                                    onClicked: panel.openTask(associated.modelData.taskId)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Label {Layout.fillWidth: true;visible: panel.workspace.errorText.length>0;wrapMode: Text.Wrap;color: Theme.danger;text: panel.workspace.errorText}
    Item {
        visible: false
        EventDialog {id: eventEditor;workspace: panel.workspace;parent: Overlay.overlay;anchors.centerIn: parent;onOpenTask: function(id){panel.openTask(id)}}
        Dialog {
            id: unscheduled
            parent: Overlay.overlay
            anchors.centerIn: parent
            title: qsTr("未排期任务")
            width: Math.min(480,parent.width-40)
            height: Math.min(500,parent.height-40)
            modal: true
            standardButtons: Dialog.Close
            contentItem: ListView {
                id: motionSurface2
                DS.ScrollPhysics {target: motionSurface2}
                clip: true
                model: panel.workspace.unscheduledTasks
                delegate: ItemDelegate {required property var modelData;width: ListView.view.width;text: modelData.title;onClicked: {unscheduled.close();panel.openTask(modelData.taskId)}}
                Label {anchors.centerIn: parent;visible: panel.workspace.unscheduledTasks.length===0;text: qsTr("没有未排期的活动任务")}
            }
        }
        Dialog {
        id: moveDialog
        objectName: "calendarMoveDialog"
        parent: Overlay.overlay
        x: (parent.width-width)/2
        y: (parent.height-height)/2
        title: qsTr("调整任务日期")
        width: Math.min(440,parent.width-32)
        modal: true
        closePolicy: Popup.NoAutoClose
        ColumnLayout {
            anchors.fill: parent
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: panel.moving.title || "" }
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("只修改所选日期，另一日期保持原值。") }
            ComboBox { id: markerKind;Layout.fillWidth: true;model: [qsTr("计划开始"),qsTr("截止日期")]; enabled: panel.moving.kind === "both" }
            DateField { id: targetDate;objectName: "calendarMoveDate";workspace: panel.workspace;dateLabel: markerKind.currentIndex===0 ? qsTr("计划开始") : qsTr("截止日期");Layout.fillWidth: true }
            Label { text: panel.workspace.errorText; color: Theme.danger; Layout.maximumWidth: 300; wrapMode: Text.Wrap }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("取消"); enabled: !panel.workspace.busy; onClicked: moveDialog.close() }
            Button {objectName: "confirmCalendarMove";text: qsTr("确认改期");highlighted: true;enabled: !panel.workspace.busy; onClicked: panel.workspace.moveCalendar(panel.moving.taskId,panel.moving.rowVersion,markerKind.currentIndex === 0 ? "start" : "deadline",targetDate.text) }
        }
        }
    }
}
