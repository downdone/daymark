pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    objectName: "calendarPanel"
    implicitWidth: 640
    implicitHeight: 480
    Layout.minimumWidth: 0
    required property TaskWorkspace workspace
    signal desktopRequested
    signal scaleRequested(string scale)
    signal stepRequested(int offset)
    signal dateRequested(string date)
    signal presentationRequested(string date, string scale)
    signal timeMoveRequested(string taskId, real version, string date, int minute, string edge)
    signal dateMoveRequested(string taskId, real version, string from, string date, string edge)
    signal creationRequested(string date, int minute, int duration, point anchor)
    property string filterIdentity: ""
    readonly property string selectionContext: (independent ? anchorDate : workspace.scope+"/"+workspace.calendarDate+"/"+filterIdentity)+"/"+calendarScale
    onSelectionContextChanged: clearSelection()
    function pruneTimelineSelection() {
        if(!selectedTaskId || workspace.busy || (calendarScale!=="day" && calendarScale!=="week"))return
        const found=timelineData.blocks.some(value=>value.taskId===selectedTaskId) || timelineData.days.some(day=>day.entries.some(value=>(value.taskId || value.eventId)===selectedTaskId))
        if(!found)clearSelection()
    }
    onTimelineDataChanged: pruneTimelineSelection()
    property bool editingBlocked: false
    property string selectedTaskId: ""
    property var selectedEntry: ({})
    property int selectedMinute: -1
    property point selectedAnchor: Qt.point(-1,-1)
    readonly property var hostWindow: panel.Window.window
    readonly property bool commandsAvailable: visible && !editingBlocked && !modalOpen && !workspace.busy && !!hostWindow && hostWindow.active && !(hostWindow.activeFocusItem instanceof TextInput) && !(hostWindow.activeFocusItem instanceof TextEdit)
    readonly property bool canvasFocused: {
        let item=hostWindow ? hostWindow.activeFocusItem : null
        while(item){if(item===panel || item===monthGrid || item===timelineView || item===yearLoader.item)return true;item=item.parent}
        return false
    }
    readonly property bool canvasReturn: !!hostWindow && !!hostWindow.activeFocusItem && (hostWindow.activeFocusItem===panel || hostWindow.activeFocusItem===timelineView || hostWindow.activeFocusItem.objectName.indexOf("calendarDay-")===0)
    property string previousScale: "month"
    function selectEntry(entry,source,date) {
        selectedTaskId=entry.taskId || entry.eventId || "";selectedEntry=entry;selectedMinute=-1
        if(date)selectedDate=date
        if(source){selectedAnchor=source.mapToItem(panel.Window.window.contentItem,source.width,0);source.forceActiveFocus()}
    }
    function clearSelection() {selectedTaskId="";selectedEntry=({});selectedMinute=-1}
    function editSelected() {if(selectedTaskId){taskAnchor=selectedAnchor;openTask(selectedTaskId)}}
    function requestNew() {if(modalOpen)return;const minute=selectedMinute>=0 ? selectedMinute : (calendarScale==="day" || calendarScale==="week" ? 540 : -1);selectedTaskId="";openCreate(selectedDate,minute,-1,selectedAnchor.x>=0 ? selectedAnchor : undefined)}
    function command(action) {
        if(modalOpen)return
        if(action==="new"){requestNew();return}
        if(action==="calendarToday"){step(0);Qt.callLater(function(){if(panel.timelineView)panel.timelineView.revealNow()});return}
        if(action==="calendarNext" || action==="calendarPrevious"){step(action==="calendarNext" ? 1 : -1);return}
        if(action==="calendarGoTo"){openDateJump();return}
        if(action==="calendarEdit"){editSelected();return}
        if(["day","week","month","year"].indexOf(action)>=0)setScale(action)
    }
    Shortcut {sequence:"Ctrl+T";enabled:panel.commandsAvailable;onActivated:panel.command("calendarToday")}
    Shortcut {sequence:"Ctrl+Shift+T";enabled:panel.commandsAvailable;onActivated:panel.command("calendarGoTo")}
    Shortcut {sequence:"Ctrl+Right";enabled:panel.commandsAvailable;onActivated:panel.command("calendarNext")}
    Shortcut {sequence:"Ctrl+Left";enabled:panel.commandsAvailable;onActivated:panel.command("calendarPrevious")}
    Shortcut {sequence:"Ctrl+E";enabled:panel.commandsAvailable && !!panel.selectedTaskId;onActivated:panel.editSelected()}
    Shortcut {sequences: Qt.platform.os==="osx" ? ["Backspace",StandardKey.Delete] : [StandardKey.Delete];enabled:panel.commandsAvailable && !!panel.selectedTaskId;onActivated:panel.taskAction(panel.selectedTaskId,"delete")}
    Shortcut {sequence:"Return";enabled:panel.commandsAvailable && panel.canvasReturn;onActivated:panel.selectedTaskId ? panel.editSelected() : panel.requestNew()}
    Shortcut {sequence:"Escape";enabled:panel.commandsAvailable && (!!panel.selectedTaskId || panel.selectedMinute>=0) && !panel.timelineDragging;onActivated:panel.clearSelection()}
    Shortcut {sequences:[StandardKey.New];enabled:panel.independent && panel.commandsAvailable;onActivated:panel.requestNew()}
    Shortcut {sequence:"Ctrl+1";enabled:panel.independent && panel.commandsAvailable;onActivated:panel.setScale("day")}
    Shortcut {sequence:"Ctrl+2";enabled:panel.independent && panel.commandsAvailable;onActivated:panel.setScale("week")}
    Shortcut {sequence:"Ctrl+3";enabled:panel.independent && panel.commandsAvailable;onActivated:panel.setScale("month")}
    Shortcut {sequence:"Ctrl+4";enabled:panel.independent && panel.commandsAvailable;onActivated:panel.setScale("year")}
    property bool independent: false
    property bool weekMode: false
    property string independentScale: "month"
    readonly property string calendarScale: independent ? independentScale : workspace.calendarScale
    property var timelineData: ({
            days: [],
            blocks: [],
            minutes: 1440
        })
    readonly property CalendarTimeline timelineView: timelineLoader.item as CalendarTimeline
    readonly property bool timelineDragging: draggingMarker || (timelineView ? timelineView.dragging : false)
    signal taskAction(string taskId, string action)
    property var contextTask: ({})
    function refreshTimeline() {
        if (!visible)
            return;
        if (calendarScale === "day" || calendarScale === "week")
            timelineData = workspace.timelineFor(independent ? anchorDate : workspace.calendarDate, calendarScale === "week", !independent);
    }
    onCalendarScaleChanged: {
        refreshIndependent();
        refreshTimeline();
    }
    function setScale(value) {
        if(value==="year" && calendarScale!=="year")previousScale=calendarScale
        clearSelection()
        if(independent){if(!workspace.setPreference("desktopCalendarScale",value))return;if(calendarScale==="month" && value!=="month")anchorDate=selectedDate;independentScale=value;weekMode=value==="week"}
        else if(calendarScale==="month" && value!=="month")presentationRequested(selectedDate,value)
        else scaleRequested(value)
    }
    property string anchorDate: workspace.dateOffset(0)
    property var independentDays: []
    readonly property bool isWeek: calendarScale === "week"
    function refreshIndependent() {
        if (independent)
            independentDays = workspace.calendarDaysFor(anchorDate, weekMode);
    }
    onAnchorDateChanged: {
        refreshIndependent();
        refreshTimeline();
    }
    onWeekModeChanged: {
        if (weekMode)
            independentScale = "week";
        refreshIndependent();
        refreshTimeline();
    }
    Component.onCompleted: {
        if (independent)
            independentScale = workspace.preferences.desktopCalendarScale || "month";
        refreshIndependent();
        refreshTimeline();
        refreshUnscheduled();
    }
    function step(offset) {
        if (calendarScale === "month" && monthGrid.visibleMonth) {
            const base=monthGrid.navigationMonth || (independent ? anchorDate : workspace.calendarDate);
            const date = offset === 0 ? workspace.dateOffset(0) : workspace.shiftCalendarScale(base, offset, "month");
            jumpToDate(date);
        } else if (independent) {
            anchorDate = workspace.shiftCalendarScale(anchorDate, offset, calendarScale);
            selectedDate = anchorDate;
        } else
            stepRequested(offset);
    }
    property point taskAnchor: Qt.point(0, 0)
    function inspectAt(id, item) {
        taskAnchor = item.mapToItem(panel.Window.window.contentItem, item.width, 0);
        openTask(id);
    }
    signal openTask(string taskId)
    signal completeTask(string taskId, real version, string action)
    property string selectedDate: workspace.dateOffset(0)
    readonly property var days: independent ? independentDays : workspace.calendarDays
    readonly property var selectedDay: days.find(day => day.date === selectedDate) || workspace.calendarDaysFor(selectedDate, true, !independent).find(day => day.date === selectedDate) || ({
            date: "",
            entries: [],
            events: []
        })
    readonly property bool compact: width < 800 * Theme.scale
    readonly property bool modalOpen: jumpDialog.visible || moveDialog.visible || unscheduled.visible || createPopup.visible || taskMenu.visible || displayMenu.visible
    property bool agendaVisible: false
    onAgendaVisibleChanged: if (!agendaVisible)
        Qt.callLater(resetGridScroll)
    function resetGridScroll() {
        const flick = body.contentItem as Flickable;
        if (flick) {
            flick.cancelFlick();
            flick.contentY = flick.originY;
        }
        monthGrid.resetToAnchor();
    }
    function verifyGridStart() {
        return monthGrid.verifyGridStart();
    }
    property bool showTasks: true
    property bool showEvents: true
    property var moving: ({})
    property var draggedEntry: null
    property bool draggingMarker: false
    property string dragFrom: ""
    property string dragEdge: "move"
    property var unscheduledItems: []
    function refreshUnscheduled() {
        unscheduledItems = workspace.unscheduledFor(!independent);
    }
    function beginDrag(entry, item, point, from, edge) {
        draggedEntry = entry;
        dragFrom = from;
        dragEdge = edge || "move";
        moveDrag(item, point);
        draggingMarker = true;
    }
    function moveDrag(item, point) {
        const at = item.mapToItem(calendarDrag.parent, point.x, point.y);
        calendarDrag.x = at.x - 8;
        calendarDrag.y = at.y - 8;
    }
    function endDrag(commit) {
        if (commit)
            calendarDrag.Drag.drop();
        else
            calendarDrag.Drag.cancel();
        draggingMarker = false;
        draggedEntry = null;
    }
    Shortcut {
        sequence: "Esc"
        enabled: panel.draggingMarker
        onActivated: panel.endDrag(false)
    }
    Item {
        visible: false
        TaskDragPayload {
            id: calendarDrag
            parent: panel.Window.window ? panel.Window.window.contentItem : panel
            z: 1000001
            visible: panel.draggingMarker
            taskId: panel.draggedEntry ? panel.draggedEntry.taskId || panel.draggedEntry.eventId : ""
            rowVersion: panel.draggedEntry ? panel.draggedEntry.rowVersion : 0
            title: panel.draggedEntry ? panel.draggedEntry.title : ""
            Drag.active: panel.draggingMarker
            Drag.source: calendarDrag
            Drag.hotSpot.x: 8
            Drag.hotSpot.y: 8
            Drag.supportedActions: Qt.MoveAction
        }
    }
    Connections {
        target: panel.workspace
        function onStateChanged() {
            panel.refreshTimeline();
            panel.refreshUnscheduled();
        }
        function onOperationFinished(operation, success) {
            if (operation === "calendarCreate" && panel.awaitingCreated) {
                panel.creating = false;
                if (success)
                    createPopup.close();
                else {
                    panel.awaitingCreated = false;
                    panel.creationCloseAction=null;
                }
            }
        }
    }
    spacing: Theme.gap
    function entries(day) {
        return (showTasks ? day.entries : []).concat(showEvents ? day.events : []);
    }
    function verifyDayTitles() {
        return monthGrid.verifyDayTitles();
    }
    function kindText(kind) {
        return kind === "range" ? qsTr("任务时段") : kind === "both" ? qsTr("开始与结束") : kind === "deadline" ? qsTr("结束") : qsTr("开始");
    }
    function offerMove(entry, date) {
        moving = entry;
        moveDialog.offer(entry, date);
    }
    function openEvent(id) {
        if (id)
            openTask(id);
        else {
            editAfterCreate = true;
            showAgenda();
        }
    }
    function inspectEvent(id, key) {
        openTask(id);
    }
    property bool editAfterCreate: false
    property string requestedRange: ""
    function ensureRange() {
        if(!visible || calendarScale==="year" || workspace.interactionActive || workspace.busy)return
        const rules=workspace.recurrenceRules.map(rule=>rule.id+":"+rule.rowVersion+":"+rule.active).join("|")
        if(calendarScale==="month" && monthGrid.visibleFrom && monthGrid.visibleThrough){
            const key=monthGrid.visibleFrom+"/"+monthGrid.visibleThrough+"/"+rules
            if(key!==requestedRange && workspace.requestCalendarSpan(monthGrid.visibleFrom,monthGrid.visibleThrough))requestedRange=key
        }else {
            const anchor=independent ? anchorDate : workspace.calendarDate,key=anchor+"/"+rules
            if(key!==requestedRange && workspace.requestCalendarRange(anchor))requestedRange=key
        }
    }
    Timer {
        id: rangeRequest
        interval: 100
        onTriggered: panel.ensureRange()
    }
    onDaysChanged: rangeRequest.restart()
    function openDateJump() {
        jumpDate.text = selectedDate;
        jumpError.text = "";
        jumpDialog.open();
        jumpDate.forceActiveFocus();
    }
    function jumpToDate(value) {
        const date = workspace.shiftDate(value, 0);
        if (!date) {
            jumpError.text = qsTr("请输入有效日期，例如2028-02-29。");
            return false;
        }
        if (independent) {
            anchorDate = date;
            finishDateSelection(date);
        } else
            dateRequested(date);
        return true;
    }
    function finishDateSelection(date) {
        clearSelection();selectedDate = date;
        jumpDialog.close();
        if (calendarScale === "month")
            monthGrid.resetToAnchor();
        Qt.callLater(focusSelected);
    }
    function navigateTo(date, scale) {
        if (independent) {
            jumpToDate(date);
            setScale(scale);
        } else
            presentationRequested(date, scale);
    }
    onVisibleChanged: {
        if (!visible && jumpDialog) {
            jumpDialog.close();
            unscheduled.close();
            endDrag(false);
        } else {
            refreshTimeline();
            rangeRequest.restart();
        }
    }
    function protectClose(action) {
        if(createPopup.visible){
            if(!createPopup.finish())return createPopup.visible;
            creationCloseAction=action;return true;
        }
        jumpDialog.close();
        return false;
    }
    function showAgenda() {
        agendaVisible = true;
        const flick = body.contentItem as Flickable;
        if (compact && flick)
            flick.contentY = flick.originY;
        Qt.callLater(function () {
            if (panel.compact && flick)
                flick.contentY = flick.originY;
            if (panel.editAfterCreate)
                newTitle.forceActiveFocus();
        });
    }
    function focusSelected() {
        if (calendarScale === "month")
            monthGrid.focusDate(selectedDate);
    }
    function moveSelection(offset) {
        jumpToDate(workspace.shiftDate(selectedDate, offset));
    }

    Connections {
        target: panel.workspace
        function onStateChanged() {
            panel.refreshIndependent();
            if (panel.visible && !panel.workspace.busy)
                rangeRequest.restart();
            if (panel.calendarScale !== "month" && !panel.days.some(day => day.date === panel.selectedDate)) {
                const day = panel.days.find(day => day.inMonth);
                if (day)
                    panel.selectedDate = day.date;
            }
        }
        function onCalendarTaskCreated(id) {
            if (!panel.awaitingCreated)
                return;
            panel.awaitingCreated = false;
            panel.creating = false;
            const afterClose=panel.creationCloseAction;
            panel.creationCloseAction=null;
            createPopup.close();
            if (panel.editAfterCreate) {
                panel.editAfterCreate = false;
                panel.openTask(id);
            }
            if(afterClose)afterClose();
        }
        function onOperationFinished(operation, success) {
            if (operation === "calendarCreate" && panel.agendaCreating) {
                panel.agendaCreating = false;
                if (success)
                    newTitle.clear();
            }
            if (success && operation === "calendarMove")
                moveDialog.close();
        }
    }
    function openCreate(date, minute, endMinute, at) {
        const anchor = at || panel.mapToItem(Overlay.overlay, panel.width / 2, 32);
        const duration = endMinute > minute ? endMinute - minute : 60;
        if (independent)
            beginCreate(date, minute, duration, anchor);
        else
            creationRequested(date, minute, duration, anchor);
    }
    function beginCreate(date, minute, duration, at) {
        creationAnchor = at;
        newDate = date;
        newMinute = minute;
        newDuration = duration;
        workspace.clearError();
        createPopup.open();
    }
    property string newDate: ""
    property int newMinute: -1
    property int newDuration: 60
    property point creationAnchor: Qt.point(-1, -1)
    property bool creating: false
    property bool awaitingCreated: false
    property bool agendaCreating: false
    property var creationCloseAction:null
    Label {
        visible: panel.compact && panel.calendarScale!=="month"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: panel.workspace.titleForScale(panel.calendarScale === "month" && monthGrid.visibleMonth ? monthGrid.visibleMonth : panel.independent ? panel.anchorDate : panel.workspace.calendarDate, panel.calendarScale)
        font.pixelSize: Theme.headingSize
        font.bold: true
        elide: Text.ElideRight
    }
    GridLayout {
        objectName: "calendarToolbar"
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        columns: panel.width < 160*Theme.scale+5*Theme.controlHeight+40 ? 1 : 2
        columnSpacing: 8;rowSpacing: Theme.fieldGap
        RowLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 4
            Label {
                visible: !panel.compact && panel.calendarScale!=="month"
                Layout.minimumWidth: 0
                text: panel.workspace.titleForScale(panel.calendarScale === "month" && monthGrid.visibleMonth ? monthGrid.visibleMonth : panel.independent ? panel.anchorDate : panel.workspace.calendarDate, panel.calendarScale)
                font.pixelSize: Theme.headingSize
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            SegmentedPicker {
                objectName: "calendarScalePicker"
                Layout.fillWidth: false
                Layout.preferredWidth: 160 * Theme.scale
                Layout.minimumWidth: 4 * Math.max(32, Theme.bodySize + 12)
                selectionIsExternal: true
                model: [qsTr("日"), qsTr("周"), qsTr("月"), qsTr("年")]
                currentIndex: ["day", "week", "month", "year"].indexOf(panel.calendarScale)
                onActivated: function (index) {
                    panel.setScale(["day", "week", "month", "year"][index]);
                }
            }
            Item {Layout.fillWidth: true;visible: panel.compact || panel.calendarScale==="month"}
        }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 2
            IconToolButton {
                contentItem: AppIcon {
                    name: "left"
                }
                Accessible.name: qsTr("上一页")
                onClicked: panel.step(-1)
            }
            Button {
                text: qsTr("今天")
                onClicked: panel.command("calendarToday")
            }
            IconToolButton {
                contentItem: AppIcon {
                    name: "right"
                }
                Accessible.name: qsTr("下一页")
                onClicked: panel.step(1)
            }
            WidgetAction {
                symbol: "plus"
                description: qsTr("新建任务")
                onClicked: panel.requestNew()
            }
            WidgetAction {
                objectName: "calendarDisplayAction"
                symbol: "more"
                description: qsTr("日历显示与操作")
                onClicked: displayMenu.open()
                Menu {
                    id: displayMenu
                    MenuItem {
                        objectName: "calendarJump"
                        text: qsTr("跳转日期…")
                        onTriggered: panel.openDateJump()
                    }
                    MenuItem {
                        text: qsTr("未安排任务（%1）").arg(panel.unscheduledItems.length)
                        onTriggered: unscheduled.open()
                    }
                    MenuItem {
                        visible: panel.calendarScale === "month"
                        text: panel.agendaVisible ? qsTr("收起当天议程") : qsTr("当天议程")
                        onTriggered: {
                            if (panel.agendaVisible)
                                panel.agendaVisible = false;
                            else
                                panel.showAgenda();
                        }
                    }
                    MenuSeparator {}
                    MenuItem {
                        visible: panel.calendarScale !== "year"
                        text: qsTr("已完成任务")
                        checkable: true
                        checked: panel.workspace.calendarCompleted
                        onTriggered: panel.workspace.calendarCompleted = checked
                    }
                    MenuItem {
                        text: qsTr("周日为每周首日")
                        checkable: true
                        checked: panel.workspace.sundayFirst
                        onTriggered: panel.workspace.sundayFirst = checked
                    }
                    MenuItem {
                        text: qsTr("桌面日历")
                        visible: !panel.independent
                        onTriggered: panel.desktopRequested()
                    }
                }
            }
        }
    }
    Loader {
        id: timelineLoader
        active: panel.visible && (panel.calendarScale === "day" || panel.calendarScale === "week")
        visible: active
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: CalendarTimeline {
            workspace: panel.workspace
            timeline: panel.timelineData
            creationSelection:createPopup.visible && panel.newMinute>=0 ? ({date:panel.newDate,minute:panel.newMinute,duration:panel.newDuration}) : ({})
            selectedId: panel.selectedTaskId
            slotSelectionActive: panel.selectedMinute>=0
            onTaskSelected: function(entry,source){panel.selectEntry(entry,source,entry.date)}
            onSlotSelected: function(date,minute,at){panel.clearSelection();panel.selectedDate=date;panel.selectedMinute=minute;panel.selectedAnchor=panel.timelineView.mapToItem(panel.Window.window.contentItem,at.x,at.y)}
            onOpenTask: function (id) {
                panel.taskAnchor = panel.timelineView.taskAnchor;
                panel.openTask(id);
            }
            onTimeMoveRequested: function (id, version, date, minute, edge) {
                panel.timeMoveRequested(id, version, date, minute, edge);
            }
            onDayRequested: function (date) {
                panel.navigateTo(date, "day");
            }
            onAgendaRequested: function (date) {
                panel.selectedDate = date;
                panel.showAgenda();
                panel.setScale("month");
            }
            onCreateRequested: function (date, minute, x, y, endMinute) {
                panel.openCreate(date, minute, endMinute, panel.timelineView.mapToItem(Overlay.overlay, x, y));
            }
            onContextRequested: function (task, x, y) {
                panel.contextTask = task;
                const at = mapToItem(panel, x, y);
                taskMenu.popup(panel, at.x, at.y);
            }
        }
    }
    Loader {
        id: yearLoader
        active: panel.visible && panel.calendarScale === "year"
        visible: active
        Layout.fillWidth: true
        Layout.fillHeight: true
        sourceComponent: YearCalendar {
            workspace: panel.workspace
            onAnchorRequested: function (date) {
                panel.jumpToDate(date);
            }
            anchorDate: panel.independent ? panel.anchorDate : panel.workspace.calendarDate
            onDateHighlighted: function(date){panel.selectedDate=date}
            onDaySelected: function (date) {
                panel.navigateTo(date, panel.previousScale);
            }
            onMonthSelected: function (date) {
                panel.navigateTo(date, "month");
            }
        }
    }
    Menu {
        id: taskMenu
        MenuItem {
            text: qsTr("任务详情…")
            onTriggered: panel.openTask(panel.contextTask.taskId)
        }
        MenuItem {
            text: panel.contextTask.pinned ? qsTr("取消置顶") : qsTr("置顶")
            onTriggered: panel.taskAction(panel.contextTask.taskId, "pin")
        }
        MenuItem {
            text: panel.contextTask.important ? qsTr("取消特别关注") : qsTr("特别关注")
            onTriggered: panel.taskAction(panel.contextTask.taskId, "important")
        }
        MenuItem {
            text: qsTr("取消提醒")
            visible: !!panel.contextTask.remindAtUtc
            onTriggered: panel.taskAction(panel.contextTask.taskId, "reminderClear")
        }
        MenuItem {
            text: panel.contextTask.taskStatus === "OPEN" ? qsTr("完成任务") : qsTr("重新打开")
            onTriggered: panel.taskAction(panel.contextTask.taskId, panel.contextTask.taskStatus === "OPEN" ? "complete" : "reopen")
        }
        MenuSeparator {}
        DS.MenuItem {
            text: qsTr("删除任务…")
            destructive: true
            onTriggered: panel.taskAction(panel.contextTask.taskId, "delete")
        }
    }
    ScrollView {
        id: body
        contentHeight: panel.compact && panel.agendaVisible ? calendarColumns.implicitHeight : availableHeight
        visible: panel.calendarScale === "month"
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        Dialog {
            id: jumpDialog
            objectName: "calendarJumpDialog"
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Math.min(400 * Theme.scale, parent.width - 32)
            title: qsTr("跳转日期")
            modal: true
            closePolicy: Popup.NoAutoClose
            ColumnLayout {
                width: jumpDialog.availableWidth
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    color: Theme.secondary
                    text: qsTr("选择或输入日期，定位到所在月份或周。")
                }
                DateField {
                    id: jumpDate
                    objectName: "calendarJumpDate"
                    workspace: panel.workspace
                    dateLabel: qsTr("跳转目标日期")
                    Layout.fillWidth: true
                    onAccepted: panel.jumpToDate(jumpDate.text)
                }
                Label {
                    id: jumpError
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                    color: Theme.danger
                }
            }
            Shortcut {
                sequence: "Escape"
                enabled: jumpDialog.visible && !jumpDate.pickerVisible
                onActivated: jumpDialog.close()
            }
            onClosed: jumpDate.closePicker()
            footer: ActionBar {
                Button {
                    text: qsTr("取消")
                    onClicked: jumpDialog.close()
                }
                Button {
                    objectName: "confirmCalendarJump"
                    text: qsTr("前往")
                    highlighted: true
                    onClicked: panel.jumpToDate(jumpDate.text)
                }
            }
        }
        GridLayout {
            id: calendarColumns
            width: body.availableWidth
            height: panel.compact && panel.agendaVisible ? Math.max(body.availableHeight, implicitHeight) : body.availableHeight
            columns: panel.compact ? 1 : 2
            columnSpacing: Theme.panelPadding
            rowSpacing: Theme.fieldGap
            ColumnLayout {
                Layout.row: panel.compact && panel.agendaVisible ? 1 : 0
                Layout.column: 0
                Layout.fillWidth: true
                Layout.fillHeight: !panel.compact || !panel.agendaVisible
                Layout.preferredHeight: panel.compact && panel.agendaVisible ? Math.max(360, Theme.bodySize * 20) : -1
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: panel.workspace.sundayFirst ? [qsTr("日"), qsTr("一"), qsTr("二"), qsTr("三"), qsTr("四"), qsTr("五"), qsTr("六")] : [qsTr("一"), qsTr("二"), qsTr("三"), qsTr("四"), qsTr("五"), qsTr("六"), qsTr("日")]
                        Label {
                            required property string modelData
                            text: modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            horizontalAlignment: Text.AlignRight
                            rightPadding: 8
                            font.pixelSize: Theme.captionSize
                            color: Theme.secondary
                        }
                    }
                }
                CalendarMonthStream {
                    id: monthGrid
                    objectName: "calendarMonthGrid"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 0
                    anchorDate: panel.independent ? panel.anchorDate : panel.workspace.calendarDate
                    scoped: !panel.independent
                    onRangeNeeded: function (month) {
                        rangeRequest.restart();
                    }
                    workspace: panel.workspace
                    selectedTaskId: panel.selectedTaskId
                    selectedDate: panel.selectedDate
                    showTasks: panel.showTasks
                    showEvents: panel.showEvents
                    draggingMarker: panel.draggingMarker
                    onDateSelected: function (date) {
                        panel.clearSelection();panel.selectedDate = date;
                    }
                    onDateOpened: function(date){panel.navigateTo(date,"day")}
                    onCreateRequested: function(date,source){panel.selectedDate=date;panel.openCreate(date,-1,-1,source.mapToItem(panel.Window.window.contentItem,source.width/2,source.height/2))}
                    onTaskSelected: function(entry,source,date){panel.selectEntry(entry,source,date)}
                    onContextRequested: function(entry,source,at){panel.selectEntry(entry,source,entry.date);panel.contextTask=entry;const point=source.mapToItem(panel,at.x,at.y);taskMenu.popup(panel,point.x,point.y)}
                    onDateActivated: function (date) {
                        panel.selectedDate = date;
                        panel.showAgenda();
                    }
                    onSelectionMoved: function (offset) {
                        panel.moveSelection(offset);
                    }
                    onTaskRequested: function (id, source) {
                        panel.inspectAt(id, source);
                    }
                    onDragStarted: function (entry, source, origin, from, edge) {
                        panel.beginDrag(entry, source, origin, from, edge);
                    }
                    onDragMoved: function (source, position) {
                        panel.moveDrag(source, position);
                    }
                    onDragEnded: function (commit) {
                        panel.endDrag(commit);
                    }
                    onDateDropped: function (id, version, date) {
                        panel.dateMoveRequested(id, version, panel.draggedEntry ? panel.dragFrom : date, date, panel.dragEdge);
                    }
                    onMoveOffered: function (entry, date) {
                        panel.offerMove(entry, date);
                    }
                }
            }
            ColumnLayout {
                id: agendaSection
                visible: panel.agendaVisible
                Layout.row: 0
                Layout.column: panel.compact ? 0 : 1
                objectName: "calendarAgenda"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: panel.compact ? -1 : 280 * Theme.scale
                Layout.minimumWidth: panel.compact ? 0 : 240 * Theme.scale
                Layout.maximumWidth: panel.compact ? Infinity : 300 * Theme.scale
                Layout.preferredHeight: panel.compact ? Math.max(320, Theme.bodySize * 14) : -1
                Label {
                    text: panel.selectedDate + qsTr(" · 当天议程")
                    font.pixelSize: Theme.sectionSize
                    font.bold: true
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                TextField {
                    id: newTitle
                    Layout.fillWidth: true
                    placeholderText: qsTr("当天新任务，Enter 添加")
                    enabled: !!panel.selectedDate && !panel.workspace.busy
                    onAccepted: {
                        panel.agendaCreating = panel.workspace.createForDate(text, panel.selectedDate, panel.independent);
                        panel.awaitingCreated = panel.agendaCreating;
                    }
                }
                ListView {
                    id: agenda
                    // Avoid background row incubation across model resets on Qt 6.10.2.
                    cacheBuffer: 0
                    DS.ScrollPhysics {
                        target: agenda
                    }
                    objectName: "calendarAgendaList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.entries(panel.selectedDay)
                    spacing: Theme.fieldGap
                    ScrollBar.vertical: ScrollBar {}
                    Label {
                        anchors.centerIn: parent
                        width: parent.width
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.secondary
                        visible: agenda.count === 0
                        text: qsTr("这一天没有任务")
                    }
                    delegate: Item {
                        id: agendaRow
                        required property var modelData
                        width: agenda.width
                        height: rowContent.implicitHeight + 8
                        ColumnLayout {
                            id: rowContent
                            width: parent.width
                            spacing: 4
                            RowLayout {
                                Layout.fillWidth: true
                                IconToolButton {
                                    objectName: "calendarComplete-" + (agendaRow.modelData.taskId || "")
                                    contentItem: AppIcon {
                                        name: agendaRow.modelData.taskStatus === "COMPLETED" ? "check" : "circle"
                                    }
                                    Accessible.name: agendaRow.modelData.taskStatus === "COMPLETED" ? qsTr("恢复任务") : qsTr("完成任务")
                                    enabled: !panel.workspace.busy
                                    onClicked: panel.completeTask(agendaRow.modelData.taskId, agendaRow.modelData.rowVersion, agendaRow.modelData.taskStatus === "COMPLETED" ? "reopen" : "complete")
                                }
                                ItemDelegate {
                                    Layout.fillWidth: true
                                    Accessible.name: agendaRow.modelData.title
                                    implicitHeight: contentItem.implicitHeight + 12
                                    contentItem: Text {
                                        text: agendaRow.modelData.title
                                        font.pixelSize: Theme.bodySize
                                        color: Theme.text
                                        wrapMode: Text.Wrap
                                        font.strikeout: agendaRow.modelData.taskStatus === "COMPLETED"
                                    }
                                    onClicked: {
                                        if (agendaRow.modelData.eventId)
                                            panel.inspectEvent(agendaRow.modelData.eventId, agendaRow.modelData.occurrenceKey);
                                        else
                                            panel.openTask(agendaRow.modelData.taskId);
                                    }
                                }
                                WidgetAction {
                                    visible: !agendaRow.modelData.eventId
                                    symbol: "calendar"
                                    description: qsTr("调整任务日期")
                                    enabled: !panel.workspace.busy
                                    onClicked: panel.offerMove(agendaRow.modelData, panel.selectedDate)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                Layout.leftMargin: 42
                                wrapMode: Text.Wrap
                                font.pixelSize: Theme.captionSize
                                color: Theme.secondary
                                text: agendaRow.modelData.eventId ? qsTr("%1（本机时区）").arg(agendaRow.modelData.timeLabel) : qsTr("任务 · %1").arg(panel.kindText(agendaRow.modelData.kind))
                            }
                            Repeater {
                                model: agendaRow.modelData.eventId ? agendaRow.modelData.links : []
                                ItemDelegate {
                                    id: associated
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Accessible.name: modelData.title
                                    contentItem: Text {
                                        text: (associated.modelData.role === "followup" ? qsTr("跟进 · ") : qsTr("准备 · ")) + associated.modelData.title
                                        font.pixelSize: Theme.captionSize
                                        color: Theme.secondary
                                        wrapMode: Text.Wrap
                                        font.strikeout: associated.modelData.taskStatus === "COMPLETED"
                                    }
                                    onClicked: panel.openTask(associated.modelData.taskId)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    Label {
        Layout.fillWidth: true
        visible: panel.workspace.errorText.length > 0
        wrapMode: Text.Wrap
        color: Theme.danger
        text: panel.workspace.errorText
    }
    Item {
        visible: false
        CalendarCreatePopup {
            id: createPopup
            parent: Overlay.overlay
            workspace: panel.workspace
            date: panel.newDate
            minute: panel.newMinute
            duration: panel.newDuration
            creating: panel.creating
            globalScope:panel.independent
            anchor: panel.creationAnchor
            onClosed: {if(!panel.awaitingCreated)panel.editAfterCreate=false;Qt.callLater(function(){if(panel.visible && panel.hostWindow && panel.hostWindow.active)panel.forceActiveFocus()})}
            onSubmitted: function (title) {
                panel.editAfterCreate = false;
                panel.creating = panel.workspace.createTaskWithDetails(title,createPopup.notes,createPopup.startOn,createPopup.endOn,createPopup.detailsPatch());
                panel.awaitingCreated = panel.creating;
            }
        }
        UnscheduledTasksPanel {
            id: unscheduled
            parent: Overlay.overlay
            workspace: panel.workspace
            tasks: panel.unscheduledItems
            workspaceWidth: panel.width
            top: panel.mapToItem(Overlay.overlay, 0, 0).y
            onOpenTask: function (id) {
                panel.openTask(id);
            }
            onDragStarted: function (entry, source, origin) {
                panel.beginDrag(entry, source, origin, panel.selectedDate, "move");
            }
            onDragMoved: function (source, position) {
                panel.moveDrag(source, position);
            }
            onDragEnded: function (commit) {
                panel.endDrag(commit);
            }
        }
        CalendarDateMoveDialog {
            id: moveDialog
            parent: Overlay.overlay
            workspace: panel.workspace
            onMoveRequested: function (id, version, kind, date) {
                panel.dateMoveRequested(id, version, date, date, kind);
            }
        }
    }
}
