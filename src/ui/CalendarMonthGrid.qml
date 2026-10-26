pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property TaskWorkspace workspace
    required property var days
    property string selectedDate: ""
    property string selectedTaskId: ""
    property bool showTasks: true
    property bool showEvents: true
    property bool draggingMarker: false
    property real minimumRowHeight: 64
    readonly property bool isWeek: false
    readonly property var bands: days.reduce((all, day) => all.concat((day.bands || []).filter(item => item.lane < 3 && (item.sourceType === "task" ? showTasks : showEvents))), [])
    function entries(day) {
        return (showTasks ? day.entries : []).concat(showEvents ? day.events : []);
    }
    function bandLanesFor(week) {
        return bands.reduce((count, band) => band.week === week ? Math.max(count, band.lane + 1) : count, 0);
    }
    function weekMinimum(week) {
        const overflow = days.some(day => day.week === week && (entries(day).filter(item => !item.multiDay).length > 1 || (entries(day).some(item => !item.multiDay) && ((showTasks && day.hiddenTaskBands > 0) || (showEvents && day.hiddenEventBands > 0)))));
        return Math.max(minimumRowHeight, Math.ceil(Math.max(28, Theme.bodySize * 1.65) + bandLanesFor(week) * (Math.ceil(Theme.captionSize * 1.5) + 2) + (overflow ? 2 : 1) * Math.ceil(Theme.captionSize * 1.5) + 10));
    }
    readonly property real gridMinimum: Array.from({
        length: Math.ceil(days.length / 7)
    }, (_, week) => weekMinimum(week)).reduce((sum, height) => sum + height, 0)
    implicitHeight: gridMinimum
    signal dateSelected(string date)
    signal dateActivated(string date)
    signal dateOpened(string date)
    signal createRequested(string date,Item source)
    signal taskSelected(var entry,Item source,string date)
    signal contextRequested(var entry,Item source,point at)
    signal selectionMoved(int offset)
    signal taskRequested(string taskId, Item source)
    signal dragStarted(var entry, Item source, point origin, string from, string edge)
    signal dragMoved(Item source, point position)
    signal dragEnded(bool commit)
    signal dateDropped(string taskId, real version, string date)
    signal moveOffered(var entry, string date)
    function inspectAt(id, item) {
        taskRequested(id, item);
    }
    function beginDrag(entry, item, point, from, edge) {
        dragStarted(entry, item, point, from, edge);
    }
    function moveDrag(item, point) {
        dragMoved(item, point);
    }
    function endDrag(commit) {
        dragEnded(commit);
    }
    function offerMove(entry, date) {
        moveOffered(entry, date);
    }
    function moveSelection(offset) {
        selectionMoved(offset);
    }
    function cellForDate(date) {
        for (let i = 0; i < days.length; i++)
            if (days[i].date === date)
                return cellAt(i);
        return null;
    }
    function cellAt(index) {
        return dayCells.itemAt(index) as CalendarDay;
    }
    function verifyDayTitles() {
        let populated = 0;
        for (let i = 0; i < dayCells.count; i++) {
            const tile = cellAt(i);
            if (!tile || !tile.items.length)
                continue;
            populated++;
            if (tile.displayedCount < 1 || tile.firstDisplayedTitle.indexOf(tile.items[0].title) < 0)
                return "missing concrete title";
            if (tile.markerBottom > tile.availableHeight + 1)
                return "clipped concrete title: " + tile.modelData.date + " bottom=" + tile.markerBottom + " available=" + tile.availableHeight + " grid=" + height + " minimum=" + implicitHeight;
        }
        for (let i = 0; i < spanBands.count; i++) {
            const bar = spanBands.itemAt(i);
            if (!bar || bar.width <= 0 || bar.y + bar.height > monthGrid.height + 1)
                return "span clipped";
        }
        return populated > 0 || spanBands.count > 0 ? "PASS" : "no populated dates";
    }
    component EntryDrag: TaskDragGesture {
        required property string fromDate
        property real dateWidth: 0
        dragDistance: root ? root.workspace.dragDistance : 10
        onDragStarted: function (entry, source, origin) {
            root.taskSelected(entry,source,fromDate);root.beginDrag(entry, source, origin, dateWidth > 0 ? root.workspace.shiftDate(fromDate, Math.floor(origin.x / dateWidth)) : fromDate, "move");
        }
        onDragMoved: function (source, position) {
            root.moveDrag(source, position);
        }
        onDragEnded: function (commit) {
            root.endDrag(commit);
        }
    }
    component CalendarDay: ItemDelegate {
        id: dayCell
        objectName: "calendarDay-" + modelData.date
        required property var modelData
        readonly property var items: root ? root.entries(modelData).filter(item => !item.multiDay) : []
        readonly property int hiddenRangeCount: (root && root.showTasks ? modelData.hiddenTaskBands || 0 : 0) + (root && root.showEvents ? modelData.hiddenEventBands || 0 : 0)
        readonly property int markerLineHeight: Math.ceil(Theme.captionSize * 1.5)
        readonly property int entryLimit: Math.max(1, Math.min(root && root.isWeek ? 8 : 3, Math.floor((availableHeight - dayNumber.implicitHeight - bandSpace.height - markerLineHeight - 6) / (markerLineHeight + 2))))
        readonly property int displayedCount: markers.count
        readonly property string firstDisplayedTitle: {
            if (markers.count < 1)
                return "";
            const label = markers.itemAt(0) as Label;
            return label ? label.text : "";
        }
        readonly property real markerBottom: dayNumber.height + bandSpace.height + 1 + markers.count * (markerLineHeight + 1) + (moreEntries.visible ? moreEntries.implicitHeight + 1 : 0)
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 32
        implicitHeight: root ? root.weekMinimum(modelData.week) : 32
        Layout.minimumHeight: implicitHeight
        padding: 4
        highlighted: root && root.selectedDate === modelData.date
        focusPolicy: highlighted ? Qt.StrongFocus : Qt.ClickFocus
        onActiveFocusChanged: if (activeFocus && (focusReason===Qt.TabFocusReason || focusReason===Qt.BacktabFocusReason))
            root.dateSelected(modelData.date)
        Accessible.name: modelData.date + qsTr("，%1项").arg(items.length)
        onClicked: root.dateSelected(modelData.date)
        onDoubleClicked: {
            root.dateSelected(modelData.date);
            root.createRequested(dayCell.modelData.date,dayCell);
        }
        Keys.onLeftPressed: function (event) {
            root.moveSelection(-1);
            event.accepted = true;
        }
        Keys.onRightPressed: function (event) {
            root.moveSelection(1);
            event.accepted = true;
        }
        Keys.onUpPressed: function (event) {
            root.moveSelection(-7);
            event.accepted = true;
        }
        Keys.onDownPressed: function (event) {
            root.moveSelection(7);
            event.accepted = true;
        }
        background: Rectangle {
            radius: 0
            color: dayCell.modelData.weekend ? Theme.grouped : Theme.content
            border.width: dayCell.visualFocus ? 2 : 0
            border.color: Theme.accent
        }
        contentItem: Column {
            spacing: 1
            clip: true
            Item {
                id: dayNumber
                objectName: "calendarDateHeader-"+dayCell.modelData.date
                MouseArea {anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;width:parent.height;onClicked:root.dateSelected(dayCell.modelData.date);onDoubleClicked:root.dateOpened(dayCell.modelData.date)}
                width: dayCell.availableWidth
                implicitHeight: Math.max(28, Math.ceil(Theme.bodySize * 1.65))
                height: implicitHeight
                Rectangle {
                    anchors.right: parent.right
                    width: parent.height
                    height: width
                    radius: width / 2
                    color: dayCell.modelData.today ? Theme.calendarToday : dayCell.highlighted ? Theme.accent : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: dayCell.modelData.day
                        font.pixelSize: Theme.bodySize
                        font.weight: dayCell.modelData.today || dayCell.highlighted ? Font.DemiBold : Font.Normal
                        color: dayCell.modelData.today ? Theme.calendarTodayText : dayCell.highlighted ? Theme.accentText : dayCell.modelData.inMonth && !dayCell.modelData.weekend ? Theme.text : Theme.secondary
                    }
                }
            }
            Item {
                id: bandSpace
                width: parent.width
                height: root ? root.bandLanesFor(dayCell.modelData.week) * (dayCell.markerLineHeight + 2) : 0
            }
            Repeater {
                id: markers
                model: dayCell.items.slice(0, dayCell.entryLimit)
                Label {
                    id: marker
                    objectName: "calendarTaskMark-" + (modelData.taskId || modelData.eventId)
                    required property var modelData
                    width: dayCell.availableWidth
                    height: dayCell.markerLineHeight
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 6
                    background: Rectangle {
                        color: root && root.selectedTaskId===(marker.modelData.taskId || marker.modelData.eventId) ? Theme.accent : Theme.primaryFill
                        radius: 4
                        Rectangle {
                            width: 3
                            height: parent.height
                            radius: 1
                            color: marker.modelData.kind === "deadline" ? Theme.danger : Theme.accent
                        }
                    }
                    text: (modelData.eventId ? (modelData.recurring ? "↻ " : "") : modelData.taskStatus === "COMPLETED" ? "✓ " : modelData.kind === "deadline" ? qsTr("结束 · ") : "○ ") + (modelData.timeLabel && !modelData.allDay ? modelData.timeLabel.split("–")[0].split(" ").pop() + " " : "") + modelData.title
                    font.strikeout: modelData.taskStatus === "COMPLETED"
                    elide: Text.ElideRight
                    font.pixelSize: Theme.captionSize
                    color: root && root.selectedTaskId===(modelData.taskId || modelData.eventId) ? Theme.accentText : Theme.primaryInk
                    activeFocusOnTab: true
                    Keys.onReturnPressed: root.inspectAt(modelData.taskId || modelData.eventId,marker)
                    Accessible.name: modelData.title
                    ToolTip.visible: !Theme.hintsBlocked && marker.visible && root && root.visible && marker.truncated && markerHover.hovered && !root.draggingMarker
                    ToolTip.delay: Theme.hintDelay
                    ToolTip.timeout: Theme.hintTimeout
                    ToolTip.text: modelData.title
                    HoverHandler {
                        id: markerHover
                    }
                    EntryDrag {
                        entry: marker.modelData
                        fromDate: dayCell.modelData.date
                        onActivated: root.taskSelected(marker.modelData,marker,dayCell.modelData.date)
                        onDoubleActivated: root.inspectAt(marker.modelData.taskId || marker.modelData.eventId,marker)
                        onContextRequested: function(at){root.contextRequested(Object.assign({date:dayCell.modelData.date},marker.modelData),marker,at)}
                    }
                }
            }
            Label {
                id: moreEntries
                objectName: "calendarMore-"+dayCell.modelData.date
                readonly property int remainingCount: Math.max(0,dayCell.items.length-dayCell.entryLimit)+dayCell.hiddenRangeCount
                readonly property bool compactOverflow: moreMetrics.width>width
                function openDay() {root.dateSelected(dayCell.modelData.date);root.dateActivated(dayCell.modelData.date)}
                width: dayCell.availableWidth
                visible: dayCell.items.length > dayCell.entryLimit || dayCell.hiddenRangeCount > 0
                text: compactOverflow ? "…" : "+"+remainingCount
                elide: Text.ElideRight
                font.pixelSize: Theme.captionSize
                color: Theme.accent
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("还有 %1 项，打开当天议程").arg(remainingCount)
                Accessible.onPressAction: openDay()
                Keys.onReturnPressed: openDay()
                Keys.onSpacePressed: openDay()
                background: Rectangle {color: "transparent";radius: Theme.rowRadius;border.width: moreEntries.activeFocus ? 1 : 0;border.color: Theme.accent}
                TextMetrics {id: moreMetrics;font: moreEntries.font;text: "+"+moreEntries.remainingCount}
                ToolTip.visible: !Theme.hintsBlocked && compactOverflow && moreHover.hovered
                ToolTip.delay: Theme.hintDelay
                ToolTip.timeout: Theme.hintTimeout
                ToolTip.text: Accessible.name
                HoverHandler {id: moreHover}
                TapHandler {
                    onTapped: moreEntries.openDay()
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.separator
        }
        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.separator
        }
        DropArea {
            anchors.fill: parent
            onEntered: function (drag) {
                drag.accepted = !!(drag.source as TaskDragPayload) && !root.workspace.busy;
            }
            Rectangle {
                anchors.fill: parent
                visible: parent.containsDrag
                color: Theme.hover
                border.color: Theme.accent
                border.width: 2
            }
            onDropped: function (drop) {
                const source = drop.source as TaskDragPayload;
                if (source && source.taskId) {
                    root.dateDropped(source.taskId, source.rowVersion, dayCell.modelData.date);
                    drop.acceptProposedAction();
                }
            }
        }
    }
    Item {
        id: monthGrid
        anchors.fill: parent
        GridLayout {
            anchors.fill: parent
            columns: 7
            rowSpacing: 0
            columnSpacing: 0
            Repeater {
                id: dayCells
                model: root.days
                delegate: CalendarDay {}
            }
        }
        Rectangle {
            width: parent.width
            height: 1
            color: Theme.separator
        }
        Rectangle {
            width: 1
            height: parent.height
            color: Theme.separator
        }
        Repeater {
            id: spanBands
            model: root.bands
            Rectangle {
                id: band
                objectName: taskRange ? "calendarTaskRange-" + modelData.taskId : "calendarSpanBand"
                required property var modelData
                readonly property bool taskRange: modelData.sourceType === "task"
                function inspect() {
                    root.inspectAt(modelData.taskId || modelData.eventId, band);
                }
                readonly property real cellWidth: monthGrid.width / 7
                readonly property Item firstDay: {
                    const count = dayCells.count;
                    return count > modelData.week * 7 ? dayCells.itemAt(modelData.week * 7) : null;
                }
                x: modelData.column * cellWidth + (modelData.startsHere ? 4 : 0)
                y: (firstDay ? firstDay.y : 0) + 4 + Math.max(28, Math.ceil(Theme.bodySize * 1.65)) + 1 + modelData.lane * (Math.ceil(Theme.captionSize * 1.5) + 2)
                width: modelData.span * cellWidth - (modelData.startsHere ? 4 : 0) - (modelData.endsHere ? 4 : 0)
                height: Math.ceil(Theme.captionSize * 1.5)
                topLeftRadius: modelData.startsHere ? 5 : 0
                bottomLeftRadius: topLeftRadius
                topRightRadius: modelData.endsHere ? 5 : 0
                bottomRightRadius: topRightRadius
                color: root && root.selectedTaskId===(modelData.taskId || modelData.eventId) ? Theme.accent : Theme.primaryFill
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                border.width: activeFocus ? 2 : 0
                border.color: Theme.accent
                Keys.onReturnPressed: inspect()
                Keys.onSpacePressed: inspect()
                Keys.onPressed: function (event) {
                    if (taskRange && (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))) {
                        rangeMenu.popup();
                        event.accepted = true;
                    }
                }
                Accessible.name: (taskRange ? qsTr("任务计划范围：") : "") + modelData.title + ", " + modelData.firstVisibleDate + " — " + modelData.lastVisibleDate
                Text {
                    id: bandTitle
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 4
                    textFormat: Text.PlainText
                    text: (band.taskRange ? "○ " : band.modelData.recurring ? "↻ " : "") + band.modelData.title
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.captionSize
                    color: root && root.selectedTaskId===(band.modelData.taskId || band.modelData.eventId) ? Theme.accentText : Theme.primaryInk
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                ToolTip.visible: !Theme.hintsBlocked && band.visible && root && root.visible && bandTitle.truncated && bandHover.hovered && !root.draggingMarker
                ToolTip.delay: Theme.hintDelay
                ToolTip.timeout: Theme.hintTimeout
                ToolTip.text: Accessible.name
                HoverHandler {
                    id: bandHover
                }
                EntryDrag {
                    entry: band.modelData
                    dateWidth: band.cellWidth
                    fromDate: root.days[band.modelData.week * 7 + band.modelData.column].date
                    onActivated: root.taskSelected(band.modelData,band,band.modelData.firstVisibleDate)
                    onDoubleActivated: band.inspect()
                    onContextRequested: function(at){root.contextRequested(Object.assign({date:band.modelData.firstVisibleDate},band.modelData),band,at)}
                }
                Repeater {
                    model: ["start", "deadline"]
                    MouseArea {
                        required property string modelData
                        width: 6
                        height: band.height
                        x: modelData === "start" ? 0 : band.width - width
                        enabled: modelData === "start" ? band.modelData.startsHere : band.modelData.endsHere
                        cursorShape: Qt.SizeHorCursor
                        onPressed: function (mouse) {
                            root.beginDrag(band.modelData, band, Qt.point(x + mouse.x, mouse.y), modelData === "start" ? band.modelData.firstVisibleDate : band.modelData.lastVisibleDate, modelData);
                        }
                        onPositionChanged: function (mouse) {
                            if (pressed)
                                root.moveDrag(band, Qt.point(x + mouse.x, mouse.y));
                        }
                        onReleased: root.endDrag(true)
                        onCanceled: root.endDrag(false)
                    }
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    enabled: band.taskRange
                    onTapped: rangeMenu.popup()
                }
                Menu {
                    id: rangeMenu
                    MenuItem {
                        text: qsTr("调整开始日期…")
                        onTriggered: root.offerMove(Object.assign({}, band.modelData, {
                            kind: "start"
                        }), band.modelData.startOn)
                    }
                    MenuItem {
                        text: qsTr("调整结束日期…")
                        onTriggered: root.offerMove(Object.assign({}, band.modelData, {
                            kind: "deadline"
                        }), band.modelData.deadlineOn)
                    }
                }
            }
        }
    }
}
