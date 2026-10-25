pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: panel
    objectName: "calendarPanel"
    required property TaskWorkspace workspace
    signal desktopRequested()
    property bool independent: false
    property bool weekMode: false
    property string anchorDate: workspace.dateOffset(0)
    property var independentDays: []
    readonly property bool isWeek: independent ? weekMode : workspace.view==="week"
    function refreshIndependent() {if(independent)independentDays=workspace.calendarDaysFor(anchorDate,weekMode)}
    onAnchorDateChanged: refreshIndependent()
    onWeekModeChanged: refreshIndependent()
    Component.onCompleted: refreshIndependent()
    function step(offset) {if(independent){anchorDate=workspace.shiftCalendar(anchorDate,offset,weekMode);selectedDate=anchorDate}else workspace.stepCalendar(offset)}
    signal openTask(string taskId)
    signal completeTask(string taskId,real version,string action)
    property string selectedDate: workspace.dateOffset(0)
    readonly property var days: independent ? independentDays : workspace.calendarDays
    readonly property var selectedDay: days.find(day=>day.date===selectedDate) || ({date:"",entries:[],events:[]})
    readonly property bool compact: width<800*Theme.scale
    readonly property var bands: days.reduce((all,day)=>all.concat((day.bands || []).filter(item=>item.lane<3 && (item.sourceType==="task" ? showTasks : showEvents))),[])
    readonly property int visibleBandLanes: bands.reduce((count,band)=>Math.max(count,band.lane+1),0)
    function bandLanesFor(week) {return bands.reduce((count,band)=>band.week===week ? Math.max(count,band.lane+1) : count,0)}
    function weekMinimum(week) {
        const needsOverflow=days.some(day=>day.week===week && (entries(day).filter(item=>!item.multiDay).length>1 || (entries(day).some(item=>!item.multiDay) && ((showTasks && day.hiddenTaskBands>0) || (showEvents && day.hiddenEventBands>0)))))
        return Math.max(64,Math.ceil(Math.max(28,Theme.bodySize*1.65)+bandLanesFor(week)*(Math.ceil(Theme.captionSize*1.5)+2)+(needsOverflow ? 2 : 1)*Math.ceil(Theme.captionSize*1.5)+10))
    }
    readonly property int rowMinimum: Math.max(64,...Array.from({length:Math.ceil(days.length/7)},(_,week)=>weekMinimum(week)))
    readonly property int gridMinimum: Array.from({length:Math.ceil(days.length/7)},(_,week)=>weekMinimum(week)).reduce((sum,height)=>sum+height,0)
    readonly property bool modalOpen: jumpDialog.visible || moveDialog.visible || unscheduled.visible
    property bool agendaVisible: false
    onAgendaVisibleChanged: if(!agendaVisible)Qt.callLater(resetGridScroll)
    function resetGridScroll(){const flick=body.contentItem as Flickable;if(flick){flick.cancelFlick();flick.contentY=flick.originY}}
    function verifyGridStart() {
        const flick=body.contentItem as Flickable,first=dayCells.itemAt(0)
        if(!flick || !first || Math.abs(flick.contentY-flick.originY)>1 || first.mapToItem(flick,0,0).y<0)return "month starts outside viewport"
        return "PASS"
    }
    property bool showTasks: true
    property bool showEvents: true
    property var moving: ({})
    property var draggedEntry: null
    property bool draggingMarker: false
    spacing: Theme.gap
    function entries(day) {return (showTasks ? day.entries : []).concat(showEvents ? day.events : [])}
    function verifyDayTitles() {
        let populated=0
        for(let i=0;i<dayCells.count;i++) {
            const tile=dayCells.itemAt(i) as CalendarDay
            if(!tile || tile.items.length===0)continue
            populated++
            if(tile.displayedCount<1 || tile.firstDisplayedTitle.indexOf(tile.items[0].title)<0)return "missing concrete title"
            if(tile.markerBottom>tile.availableHeight+1)return "clipped concrete title"
        }
        for(let i=0;i<spanBands.count;i++){const bar=spanBands.itemAt(i);if(!bar || bar.width<=0 || bar.y+bar.height>monthGrid.height+1)return "span clipped"}
        return populated>0 || spanBands.count>0 ? "PASS" : "no populated dates"
    }
    function kindText(kind) {return kind==="range" ? qsTr("任务时段") : kind==="both" ? qsTr("开始与结束") : kind==="deadline" ? qsTr("结束") : qsTr("开始")}
    function offerMove(entry,date) {moving=entry;targetDate.text=date;markerKind.currentIndex=entry.kind==="deadline" ? 1 : 0;moveDialog.open()}
    function openEvent(id) {if(id)openTask(id);else {editAfterCreate=true;showAgenda()}}
    function inspectEvent(id,key) {openTask(id)}
    property bool editAfterCreate: false
    property string requestedRange: ""
    function ensureRange(){if(!visible)return;const anchor=days.length>10 ? days[10].date : anchorDate;if(anchor && anchor!==requestedRange && workspace.requestCalendarRange(anchor))requestedRange=anchor}
    Timer {id: rangeRequest;interval: 100;onTriggered: panel.ensureRange()}
    onDaysChanged: rangeRequest.restart()
    function openDateJump() {jumpDate.text=selectedDate;jumpError.text="";jumpDialog.open();jumpDate.forceActiveFocus()}
    function jumpToDate(value) {
        const date=workspace.shiftDate(value,0)
        if(!date){jumpError.text=qsTr("请输入有效日期，例如2028-02-29。");return false}
        selectedDate=date
        if(independent)anchorDate=date;else workspace.selectCalendarDate(date)
        jumpDialog.close();Qt.callLater(focusSelected);return true
    }
    onVisibleChanged: {if(!visible && jumpDialog)jumpDialog.close();else rangeRequest.restart()}
    function protectClose(action) {jumpDialog.close();return false}
    function showAgenda() {
        agendaVisible=true
        const flick=body.contentItem as Flickable
        if(compact && flick)flick.contentY=flick.originY
        Qt.callLater(function(){if(panel.compact && flick)flick.contentY=flick.originY;newTitle.forceActiveFocus()})
    }
    function focusSelected() {for(let i=0;i<days.length;i++)if(days[i].date===selectedDate){const tile=dayCells.itemAt(i) as CalendarDay;if(tile){tile.forceActiveFocus();const flick=body.contentItem as Flickable;if(flick){const y=tile.mapToItem(flick.contentItem,0,0).y;if(y<flick.contentY || tile.height>flick.height)flick.contentY=y;else if(y+tile.height>flick.contentY+flick.height)flick.contentY=y+tile.height-flick.height}}break}}
    function moveSelection(offset) {
        const target=workspace.shiftDate(selectedDate,offset)
        selectedDate=target
        if(!days.some(day=>day.date===target)){if(independent)anchorDate=target;else workspace.selectCalendarDate(target)}
        Qt.callLater(focusSelected)
    }
    component CalendarDay: ItemDelegate {
        id: dayCell
        required property var modelData
        readonly property var items: panel.entries(modelData).filter(item=>!item.multiDay)
        readonly property int hiddenRangeCount: (panel.showTasks ? modelData.hiddenTaskBands || 0 : 0)+(panel.showEvents ? modelData.hiddenEventBands || 0 : 0)
        readonly property int markerLineHeight: Math.ceil(Theme.captionSize*1.5)
        readonly property int entryLimit: Math.max(1,Math.min(panel.isWeek ? 8 : 3,Math.floor((availableHeight-dayNumber.implicitHeight-bandSpace.height-markerLineHeight-6)/(markerLineHeight+2))))
        readonly property int displayedCount: markers.count
        readonly property string firstDisplayedTitle: {if(markers.count<1)return "";const label=markers.itemAt(0) as Label;return label ? label.text : ""}
        readonly property real markerBottom: dayNumber.height+bandSpace.height+1+markers.count*(markerLineHeight+1)+(moreEntries.visible ? moreEntries.implicitHeight+1 : 0)
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 32
        Layout.minimumHeight: panel.weekMinimum(modelData.week)
        padding: 4
        highlighted: panel.selectedDate===modelData.date
        focusPolicy: highlighted ? Qt.StrongFocus : Qt.ClickFocus
        onActiveFocusChanged: if(activeFocus)panel.selectedDate=modelData.date
        Accessible.name: modelData.date+qsTr("，%1项").arg(items.length)
        onClicked: panel.selectedDate=modelData.date
        onDoubleClicked: {panel.selectedDate=modelData.date;panel.showAgenda()}
        Keys.onLeftPressed: function(event){panel.moveSelection(-1);event.accepted=true}
        Keys.onRightPressed: function(event){panel.moveSelection(1);event.accepted=true}
        Keys.onUpPressed: function(event){panel.moveSelection(-7);event.accepted=true}
        Keys.onDownPressed: function(event){panel.moveSelection(7);event.accepted=true}
        background: Rectangle {
            radius: 0
            color: dayCell.highlighted ? Theme.selected : dayCell.modelData.weekend ? Theme.grouped : Theme.content
            border.width: dayCell.visualFocus ? 2 : 0;border.color: Theme.accent
        }
        contentItem: Column {
            spacing: 1
            clip: true
            Item {
                id: dayNumber
                width: dayCell.availableWidth;implicitHeight: Math.max(28,Math.ceil(Theme.bodySize*1.65));height: implicitHeight
                Rectangle {anchors.right: parent.right;width: parent.height;height: width;radius: width/2;color: dayCell.modelData.today ? Theme.calendarToday : "transparent"
                    Text {anchors.centerIn: parent;text: dayCell.modelData.day;font.pixelSize: Theme.bodySize;font.weight: dayCell.modelData.today || dayCell.highlighted ? Font.DemiBold : Font.Normal;color: dayCell.modelData.today ? Theme.calendarTodayText : dayCell.modelData.inMonth && !dayCell.modelData.weekend ? Theme.text : Theme.secondary}
                }
            }
            Item {id: bandSpace;width: parent.width;height: panel.bandLanesFor(dayCell.modelData.week)*(dayCell.markerLineHeight+2)}
            Repeater {
                id: markers
                model: dayCell.items.slice(0,dayCell.entryLimit)
                Label {
                    id: marker
                    objectName: "calendarTaskMark-"+(modelData.taskId || modelData.eventId)
                    required property var modelData
                    width: dayCell.availableWidth
                    height: dayCell.markerLineHeight
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 6
                    background: Rectangle {color: marker.modelData.eventId ? Theme.scheduleFill : Theme.primaryFill;radius: 4;Rectangle {width: 3;height: parent.height;radius: 1;color: marker.modelData.kind==="deadline" ? Theme.danger : Theme.accent}}
                    text: (modelData.eventId ? (modelData.recurring ? "↻ " : "") : modelData.taskStatus==="COMPLETED" ? "✓ " : modelData.kind==="deadline" ? "⚑ " : "○ ")+modelData.title
                    font.strikeout: modelData.taskStatus==="COMPLETED"
                    elide: Text.ElideRight
                    font.pixelSize: Theme.captionSize
                    color: modelData.eventId ? Theme.scheduleInk : modelData.kind==="deadline" ? Theme.danger : Theme.primaryInk
                    Accessible.name: modelData.title
                    ToolTip.visible: markerHover.hovered
                    ToolTip.text: modelData.title
                    HoverHandler {id: markerHover}
                    TapHandler {onTapped: {panel.selectedDate=dayCell.modelData.date;if(marker.modelData.eventId)panel.inspectEvent(marker.modelData.eventId,marker.modelData.occurrenceKey);else panel.openTask(marker.modelData.taskId)}}
                    Drag.active: markerDrag.active
                    Drag.source: marker
                    DragHandler {id: markerDrag;target: null;enabled: !marker.modelData.eventId;onActiveChanged: {panel.draggingMarker=active;if(active)panel.draggedEntry=marker.modelData;else {marker.Drag.drop();panel.draggedEntry=null}}}
                }
            }
            Label {
                id: moreEntries
                width: dayCell.availableWidth
                visible: dayCell.items.length>dayCell.entryLimit || dayCell.hiddenRangeCount>0
                text: "+"+(Math.max(0,dayCell.items.length-dayCell.entryLimit)+dayCell.hiddenRangeCount)
                font.pixelSize: Theme.captionSize
                color: Theme.accent
                TapHandler {onTapped: {panel.selectedDate=dayCell.modelData.date;panel.showAgenda()}}
            }
        }
        Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;color: Theme.separator}
        Rectangle {anchors.right: parent.right;width: 1;height: parent.height;color: Theme.separator}
        DropArea {anchors.fill: parent;onDropped: function(drop){if(panel.draggedEntry){panel.offerMove(panel.draggedEntry,dayCell.modelData.date);drop.acceptProposedAction()}}}
    }
    Connections {
        target: panel.workspace
        function onStateChanged() {panel.refreshIndependent();if(panel.visible && !panel.workspace.busy)rangeRequest.restart();if(!panel.days.some(day=>day.date===panel.selectedDate)){const day=panel.days.find(day=>day.inMonth);if(day)panel.selectedDate=day.date}}
        function onCalendarTaskCreated(id){if(panel.editAfterCreate){panel.editAfterCreate=false;panel.openTask(id)}}
        function onOperationFinished(operation,success) {
            if(success && operation==="calendarCreate")newTitle.clear()
            if(success && operation==="calendarMove")moveDialog.close()
        }
    }
    Flow {
        Layout.fillWidth: true;spacing: 6
        ActionButton {symbol: "plus";text: qsTr("新建任务");enabled: panel.workspace.ready && !panel.workspace.busy;onClicked: panel.openEvent("")}
        SegmentedPicker {width: 112*Theme.scale;selectionIsExternal: true;model: [qsTr("月"),qsTr("周")];currentIndex: panel.isWeek ? 1 : 0;onActivated: function(index){if(panel.independent)panel.weekMode=index===1;else panel.workspace.setView(index===1 ? "week" : "month")}}
        ActionButton {symbol: "calendar";objectName: "calendarJump";text: qsTr("跳转");flat: true;onClicked: panel.openDateJump()}
        ActionButton {symbol: "inbox";text: qsTr("未排期 %1").arg(panel.workspace.unscheduledTasks.length);flat: true;onClicked: unscheduled.open()}
        ActionButton {symbol: "list";text: qsTr("议程");flat: true;checked: panel.agendaVisible;checkable: true;onClicked: {if(checked)panel.showAgenda();else panel.agendaVisible=false}}
        WidgetAction {
            objectName: "calendarDisplayAction";symbol: "filter";description: qsTr("日历显示选项");onClicked: displayMenu.open()
            Menu {
                id: displayMenu
                MenuItem {text: qsTr("显示任务");checkable: true;checked: panel.showTasks;onTriggered: {panel.showTasks=checked;panel.showEvents=checked}}
                MenuItem {text: qsTr("已完成任务");checkable: true;checked: panel.workspace.calendarCompleted;onTriggered: panel.workspace.calendarCompleted=checked}
                MenuSeparator {}
                MenuItem {text: qsTr("周日为每周首日");checkable: true;checked: panel.workspace.sundayFirst;onTriggered: panel.workspace.sundayFirst=checked}
                MenuItem {text: qsTr("桌面日历");visible: !panel.independent;onTriggered: panel.desktopRequested()}
            }
        }
        ActionButton {symbol: "undo";visible: panel.workspace.canUndoEvent;text: qsTr("撤销删除");enabled: !panel.workspace.busy;onClicked: panel.workspace.undoEventDelete()}
    }
    RowLayout {
        Layout.fillWidth: true;spacing: 2
        Label {text: panel.independent ? panel.workspace.calendarTitleFor(panel.anchorDate,panel.weekMode) : panel.workspace.calendarTitle;font.pixelSize: Theme.titleSize;font.bold: true;Layout.fillWidth: true;elide: Text.ElideRight}
        IconToolButton {contentItem: AppIcon {name:"left"} Accessible.name: qsTr("上一页");onClicked: panel.step(-1)}
        Button {text: qsTr("今天");onClicked: {panel.selectedDate=panel.workspace.dateOffset(0);panel.step(0)}}
        IconToolButton {contentItem: AppIcon {name:"right"} Accessible.name: qsTr("下一页");onClicked: panel.step(1)}
    }
    ScrollView {
        id: body
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        Dialog {
            id: jumpDialog
            objectName: "calendarJumpDialog"
            parent: Overlay.overlay
            anchors.centerIn: parent
            width: Math.min(400*Theme.scale,parent.width-32)
            title: qsTr("跳转日期")
            modal: true
            closePolicy: Popup.NoAutoClose
            ColumnLayout {
                width: jumpDialog.availableWidth
                Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary;text: qsTr("选择或输入日期，定位到所在月份或周。")}
                DateField {id: jumpDate;objectName: "calendarJumpDate";workspace: panel.workspace;dateLabel: qsTr("跳转目标日期");Layout.fillWidth: true;onAccepted: panel.jumpToDate(jumpDate.text)}
                Label {id: jumpError;Layout.fillWidth: true;wrapMode: Text.Wrap;visible: text.length>0;color: Theme.danger}
            }
            Shortcut {sequence: "Escape";enabled: jumpDialog.visible && !jumpDate.pickerVisible;onActivated: jumpDialog.close()}
            onClosed: jumpDate.closePicker()
            footer: ActionBar {
                Button {text: qsTr("取消");onClicked: jumpDialog.close()}
                Button {objectName: "confirmCalendarJump";text: qsTr("前往");highlighted: true;onClicked: panel.jumpToDate(jumpDate.text)}
            }
        }
        GridLayout {
            width: body.availableWidth
            height: Math.max(body.availableHeight,implicitHeight)
            columns: panel.compact ? 1 : 2
            columnSpacing: Theme.panelPadding
            rowSpacing: Theme.fieldGap
            ColumnLayout {
                Layout.row: panel.compact && panel.agendaVisible ? 1 : 0
                Layout.column: 0
                Layout.fillWidth: true
                Layout.fillHeight: !panel.compact
                Layout.preferredHeight: panel.compact ? (panel.isWeek ? Math.max(160,panel.rowMinimum) : panel.gridMinimum)+28 : -1
                RowLayout {
                    Layout.fillWidth: true
                    Repeater {
                        model: panel.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
                        Label {required property string modelData;text: modelData;Layout.fillWidth: true;Layout.preferredWidth: 1;horizontalAlignment: Text.AlignRight;rightPadding: 8;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    }
                }
                Item {
                    id: monthGrid
                    objectName: "calendarMonthGrid"
                    Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: implicitHeight
                    implicitHeight: panel.gridMinimum
                    GridLayout {
                        anchors.fill: parent;columns: 7;rowSpacing: 0;columnSpacing: 0
                        Repeater {id: dayCells;model: panel.days;delegate: CalendarDay {}}
                    }
                    Rectangle {width: parent.width;height: 1;color: Theme.separator}
                    Rectangle {width: 1;height: parent.height;color: Theme.separator}
                    Repeater {
                        id: spanBands
                        model: panel.bands
                        Rectangle {
                            id: band
                            objectName: taskRange ? "calendarTaskRange-"+modelData.taskId : "calendarSpanBand"
                            required property var modelData
                            readonly property bool taskRange: modelData.sourceType==="task"
                            function inspect(){if(taskRange)panel.openTask(modelData.taskId);else panel.inspectEvent(modelData.eventId,modelData.occurrenceKey)}
                            readonly property real cellWidth: monthGrid.width/7
                            readonly property Item firstDay: {const count=dayCells.count;return count>modelData.week*7 ? dayCells.itemAt(modelData.week*7) : null}
                            x: modelData.column*cellWidth+(modelData.startsHere ? 4 : 0)
                            y: (firstDay ? firstDay.y : 0)+4+Math.max(28,Math.ceil(Theme.bodySize*1.65))+1+modelData.lane*(Math.ceil(Theme.captionSize*1.5)+2)
                            width: modelData.span*cellWidth-(modelData.startsHere ? 4 : 0)-(modelData.endsHere ? 4 : 0)
                            height: Math.ceil(Theme.captionSize*1.5)
                            topLeftRadius: modelData.startsHere ? 5 : 0;bottomLeftRadius: topLeftRadius
                            topRightRadius: modelData.endsHere ? 5 : 0;bottomRightRadius: topRightRadius
                            color: taskRange ? Theme.primaryFill : modelData.allDay ? Theme.scheduleFill : Theme.selected
                            activeFocusOnTab: true;Accessible.role: Accessible.Button
                            border.width: activeFocus ? 2 : 0;border.color: Theme.accent
                            Keys.onReturnPressed: inspect()
                            Keys.onSpacePressed: inspect()
                            Keys.onPressed: function(event){if(taskRange && (event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))){rangeMenu.popup();event.accepted=true}}
                            Accessible.name: (taskRange ? qsTr("任务计划范围：") : "")+modelData.title+", "+modelData.firstVisibleDate+" — "+modelData.lastVisibleDate
                            Text {anchors.fill: parent;anchors.leftMargin: 6;anchors.rightMargin: 4;textFormat: Text.PlainText;text: (band.taskRange ? "○ " : band.modelData.recurring ? "↻ " : "")+band.modelData.title;font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;color: band.taskRange ? Theme.primaryInk : band.modelData.allDay ? Theme.scheduleInk : Theme.accent;verticalAlignment: Text.AlignVCenter;elide: Text.ElideRight}
                            ToolTip.visible: bandHover.hovered;ToolTip.text: Accessible.name
                            HoverHandler {id: bandHover}
                            TapHandler {onTapped: band.inspect()}
                            TapHandler {acceptedButtons: Qt.RightButton;enabled: band.taskRange;onTapped: rangeMenu.popup()}
                            Menu {
                                id: rangeMenu
                                MenuItem {text: qsTr("调整开始日期…");onTriggered: panel.offerMove(Object.assign({},band.modelData,{kind:"start"}),band.modelData.startOn)}
                                MenuItem {text: qsTr("调整结束日期…");onTriggered: panel.offerMove(Object.assign({},band.modelData,{kind:"deadline"}),band.modelData.deadlineOn)}
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                visible: panel.agendaVisible
                Layout.row: 0
                Layout.column: panel.compact ? 0 : 1
                id: agendaSection
                objectName: "calendarAgenda"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: panel.compact ? -1 : 280*Theme.scale
                Layout.minimumWidth: panel.compact ? 0 : 240*Theme.scale
                Layout.maximumWidth: panel.compact ? Infinity : 300*Theme.scale
                Layout.preferredHeight: panel.compact ? Math.max(320,Theme.bodySize*14) : -1
                Label {text: panel.selectedDate+qsTr(" · 当天议程");font.pixelSize: Theme.sectionSize;font.bold: true;Layout.fillWidth: true;wrapMode: Text.Wrap}
                TextField {id: newTitle;Layout.fillWidth: true;placeholderText: qsTr("当天新任务，Enter 添加");enabled: !!panel.selectedDate && !panel.workspace.busy;onAccepted: panel.workspace.createForDate(text,panel.selectedDate)}
                ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
                    DS.ScrollPhysics {target: agenda}
                    id: agenda
                    objectName: "calendarAgendaList"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: panel.entries(panel.selectedDay)
                    spacing: Theme.fieldGap
                    ScrollBar.vertical: ScrollBar {}
                    Label {anchors.centerIn: parent;width: parent.width;wrapMode: Text.Wrap;horizontalAlignment: Text.AlignHCenter;color: Theme.secondary;visible: agenda.count===0;text: qsTr("这一天没有任务")}
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
                                IconToolButton {
                                    objectName: "calendarComplete-"+(agendaRow.modelData.taskId || "")
                                    contentItem: AppIcon {name: agendaRow.modelData.taskStatus==="COMPLETED" ? "check" : "circle"}
                                    Accessible.name: agendaRow.modelData.taskStatus==="COMPLETED" ? qsTr("恢复任务") : qsTr("完成任务")
                                    enabled: !panel.workspace.busy
                                    onClicked: panel.completeTask(agendaRow.modelData.taskId,agendaRow.modelData.rowVersion,agendaRow.modelData.taskStatus==="COMPLETED" ? "reopen" : "complete")
                                }
                                ItemDelegate {
                                    Layout.fillWidth: true
                                    Accessible.name: agendaRow.modelData.title
                                    implicitHeight: contentItem.implicitHeight+12
                                    contentItem: Text {text: agendaRow.modelData.title;font.pixelSize: Theme.bodySize;color: Theme.text;wrapMode: Text.Wrap;font.strikeout: agendaRow.modelData.taskStatus==="COMPLETED"}
                                    onClicked: {if(agendaRow.modelData.eventId)panel.inspectEvent(agendaRow.modelData.eventId,agendaRow.modelData.occurrenceKey);else panel.openTask(agendaRow.modelData.taskId)}
                                }
                                WidgetAction {visible: !agendaRow.modelData.eventId;symbol: "calendar";description: qsTr("调整任务日期");enabled: !panel.workspace.busy;onClicked: panel.offerMove(agendaRow.modelData,panel.selectedDate)}
                            }
                            Label {Layout.fillWidth: true;Layout.leftMargin: 42;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: agendaRow.modelData.eventId ? qsTr("%1（本机时区）").arg(agendaRow.modelData.timeLabel) : qsTr("任务 · %1").arg(panel.kindText(agendaRow.modelData.kind))}
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
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
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
            ComboBox { id: markerKind;Layout.fillWidth: true;model: [qsTr("开始日期"),qsTr("结束日期")]; enabled: panel.moving.kind === "both" }
            DateField { id: targetDate;objectName: "calendarMoveDate";workspace: panel.workspace;dateLabel: markerKind.currentIndex===0 ? qsTr("开始日期") : qsTr("结束日期");Layout.fillWidth: true }
            Label { text: panel.workspace.errorText; color: Theme.danger; Layout.maximumWidth: 300; wrapMode: Text.Wrap }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("取消"); enabled: !panel.workspace.busy; onClicked: moveDialog.close() }
            Button {objectName: "confirmCalendarMove";text: qsTr("确认改期");highlighted: true;enabled: !panel.workspace.busy; onClicked: panel.workspace.moveCalendar(panel.moving.taskId,panel.moving.rowVersion,markerKind.currentIndex === 0 ? "start" : "deadline",targetDate.text) }
        }
        }
    }
}
