pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import DaymarkStyle as DS

Item {
    id: root
    property bool smallSurface: false
    readonly property CalendarMetrics metrics: CalendarMetrics {smallSurface: root.smallSurface}
    required property TaskWorkspace workspace
    property string anchorDate: workspace.dateOffset(0)
    property bool scoped: true
    property string selectedDate: ""
    property string selectedTaskId: ""
    property bool showTasks: true
    property bool showEvents: true
    property bool draggingMarker: false
    property var weeks: []
    property string windowAnchor: ""
    property string visibleMonth: ""
    property string visibleFrom:""
    property string visibleThrough:""
    property bool rebuilding: false
    property bool resetPending: false
    property string pendingFocusDate: ""
    property string focusAnchor: ""
    property bool userScrolled: false
    readonly property string navigationMonth: userScrolled && visibleMonth ? visibleMonth : anchorDate
    property int refreshGeneration: 0
    property real placedScale: 0
    property real placedHeight: 0
    readonly property real textScale: workspace.textScale
    readonly property bool layoutReady: !rebuilding && !projectionRefresh.running && placedScale === textScale && placedHeight === list.height
    readonly property real weekHeight: Math.max(root.metrics.weekMinimum, root.metrics.captionSize * 4, list.height / 5.4)
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
    signal rangeNeeded(string month)
    implicitHeight: 400
    function indexForDate(date) {
        for (let i = 0; i < weeks.length; i++) {
            const key = weeks[i].key;
            if (key <= date && root.workspace.shiftDate(key, 6) >= date)
                return i;
        }
        return -1;
    }
    function refresh(reset) {
        resetPending = resetPending || reset;
        projectionRefresh.restart();
    }
    Timer {
        id: projectionRefresh
        interval: 0
        onTriggered: {
            const reset = root.resetPending;
            root.resetPending = false;
            root.performRefresh(reset);
        }
    }
    function performRefresh(reset) {
        if (!visible || !workspace)
            return;
        const capturedScale = textScale, capturedHeight = list.height;
        const effectiveReset = reset || rebuilding || placedScale !== capturedScale || placedHeight !== capturedHeight;
        const generation = ++refreshGeneration;
        const first = list.indexAt(4, list.contentY + 1), row = first >= 0 ? list.itemAtIndex(first) as WeekRow : null;
        const saved = row ? {
            date: row.modelData.key,
            offset: row.y - list.contentY
        } : null;
        rebuilding = true;
        if (effectiveReset || !windowAnchor)
            windowAnchor = anchorDate;
        const next = workspace.calendarWeeksFor(windowAnchor, 1, 2, scoped);
        const changed = JSON.stringify(next) !== JSON.stringify(weeks);
        if (!changed && !effectiveReset) {
            rebuilding = false;
            return;
        }
        if (changed)
            weeks = next;
        Qt.callLater(function () {
            if (generation !== root.refreshGeneration)
                return;
            if (capturedScale !== root.textScale || capturedHeight !== list.height) {
                root.refresh(true);
                return;
            }
            if (!root.visible) {
                root.rebuilding = false;
                return;
            }
            list.forceLayout();
            if (effectiveReset || !saved) {
                if (!root.positionAnchor()) {
                    // ListView can defer the target delegate during a scale or
                    // model change. Do not publish a ready viewport at index 0.
                    anchorPlacementRetry.restart();
                    return;
                }
            } else {
                const index = root.indexForDate(saved.date);
                if (index >= 0) {
                    list.positionViewAtIndex(index, ListView.Beginning);
                    list.forceLayout();
                    const row = list.itemAtIndex(index) as WeekRow;
                    if (row)
                        list.contentY = row.y - saved.offset;
                }
            }
            root.placedScale = capturedScale;
            root.placedHeight = capturedHeight;
            root.rebuilding = false;
            root.updateVisible();
            if (root.pendingFocusDate)
                root.applyFocus();
        });
    }
    function positionAnchor() {
        const target = focusAnchor || (textScale >= 1.5 || list.height < 280 ? anchorDate : anchorDate.slice(0, 8) + "01");
        const index = indexForDate(target);
        if (index >= 0) {
            list.forceLayout();
            list.positionViewAtIndex(index, ListView.Beginning);
            list.forceLayout();
            const row = list.itemAtIndex(index) as WeekRow;
            if (!row || row.height <= 0 || row.modelData.key !== weeks[index].key || !row.grid.cellForDate(target))
                return false;
            // Resolve against the real delegate, not a pre-incubation estimate.
            list.contentY = row.y;
            return true;
        }
        return false;
    }
    Timer {
        id: anchorPlacementRetry
        interval: 16
        onTriggered: if(root.visible)root.refresh(true)
    }
    function resetToAnchor() {
        focusAnchor = "";
        refresh(true);
    }
    function focusDate(date) {
        if(indexForDate(date)<0)focusAnchor=date;
        pendingFocusDate = date;
        applyFocus();
    }
    function applyFocus() {
        const index = indexForDate(pendingFocusDate);
        if (index < 0) {
            refresh(true);
            return;
        }
        list.positionViewAtIndex(index, ListView.Contain);
        list.forceLayout();
        const row = list.itemAtIndex(index) as WeekRow;
        const item = row ? row.grid.cellForDate(pendingFocusDate) : null;
        if (item) {
            pendingFocusDate = "";
            item.forceActiveFocus();
        }
    }
    function updateVisible() {
        if (rebuilding || !weeks.length)
            return;
        let index = list.indexAt(4, list.contentY + Math.min(30, list.height / 4));
        if (index < 0)
            index = 0;
        const month = weeks[index].month;
        if (month !== visibleMonth) {
            visibleMonth = month;
            rangeNeeded(month);
        }
        const last = list.indexAt(4, list.contentY + list.height - 1);
        const start=weeks[index].key,end=root.workspace.shiftDate(weeks[Math.max(index,last)].key,6)
        if(start!==visibleFrom || end!==visibleThrough){visibleFrom=start;visibleThrough=end;rangeNeeded(month)}
        if (index < 2 || last >= weeks.length - 3)
            recycle.restart();
    }
    function recycleWindow() {
        if (rebuilding || list.dragging || root.draggingMarker)
            return;
        const first = list.indexAt(4, list.contentY + 1), last = list.indexAt(4, list.contentY + list.height - 1);
        if (first < 2) {
            windowAnchor = workspace.shiftCalendarScale(windowAnchor, -1, "month");
            refresh(false);
        } else if (last >= weeks.length - 3) {
            windowAnchor = workspace.shiftCalendarScale(windowAnchor, 1, "month");
            refresh(false);
        }
    }
    function cellAt(index) {
        const target = root.workspace.calendarDaysFor(anchorDate, false, scoped)[index];
        if (!target)
            return null;
        const week = indexForDate(target.date);
        const row = week >= 0 ? list.itemAtIndex(week) as WeekRow : null;
        return row ? row.grid.cellForDate(target.date) : null;
    }
    function verifyGridStart() {
        const index = indexForDate(anchorDate.slice(0, 8) + "01"), row = index >= 0 ? list.itemAtIndex(index) as WeekRow : null;
        if (!row || Math.abs(row.y - list.contentY) > 1)
            return "month starts outside viewport";
        return "PASS";
    }
    function verifyDayTitles() {
        if (!layoutReady)
            return "WAIT";
        let populated = false;
        for (let i = 0; i < weeks.length; i++) {
            const row = list.itemAtIndex(i) as WeekRow;
            if (!row)
                continue;
            const result = row.grid.verifyDayTitles();
            if (result === "PASS")
                populated = true;
            else if (result !== "no populated dates")
                return result;
        }
        return populated ? "PASS" : "no populated dates";
    }
    Timer {
        id: recycle
        interval: 100
        onTriggered: root.recycleWindow()
    }
    onHeightChanged: if (visible && weeks.length)
        refresh(true)
    onTextScaleChanged: if (visible && weeks.length)
        refresh(true)
    Connections {
        target: Theme
        function onScaleChanged() {
            if (root.visible)
                root.refresh(true);
        }
    }
    onAnchorDateChanged: {
        focusAnchor = "";userScrolled=false;
        refresh(true);
    }
    onVisibleChanged: if (visible)
        refresh(true)
    onScopedChanged: refresh(true)
    Component.onCompleted: refresh(true)
    Connections {
        target: root.workspace
        function onStateChanged() {
            if (root.visible && !root.workspace.busy)
                root.refresh(false);
        }
    }
    component WeekRow: Column {
        id: row
        required property var modelData
        required property int index
        readonly property CalendarMonthGrid grid: weekGrid
        width: list.width
        spacing: 0
        Label {
            width: parent.width
            visible: !!row.modelData.title
            height: visible ? Math.max(28, root.metrics.headingSize + 12) : 0
            text: row.modelData.title
            font.pixelSize: root.metrics.headingSize
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
        }
        CalendarMonthGrid {
            id: weekGrid
            width: parent.width
            height: Math.max(root.weekHeight, implicitHeight)
            workspace: root.workspace
            smallSurface: root.smallSurface
            days: row.modelData.days
            selectedDate: root.selectedDate
            selectedTaskId: root.selectedTaskId
            showTasks: root.showTasks
            showEvents: root.showEvents
            draggingMarker: root.draggingMarker
            onDateSelected: function (date) {
                root.dateSelected(date);
            }
            onDateOpened: function(date){root.dateOpened(date)}
            onCreateRequested: function(date,source){root.createRequested(date,source)}
            onTaskSelected: function(entry,source,date){root.taskSelected(entry,source,date)}
            onContextRequested: function(entry,source,at){root.contextRequested(entry,source,at)}
            onDateActivated: function (date) {
                root.dateActivated(date);
            }
            onSelectionMoved: function (offset) {
                root.selectionMoved(offset);
            }
            onTaskRequested: function (id, source) {
                root.taskRequested(id, source);
            }
            onDragStarted: function (entry, source, origin, from, edge) {
                root.dragStarted(entry, source, origin, from, edge);
            }
            onDragMoved: function (source, position) {
                root.dragMoved(source, position);
            }
            onDragEnded: function (commit) {
                root.dragEnded(commit);
            }
            onDateDropped: function (id, version, date) {
                root.dateDropped(id, version, date);
            }
            onMoveOffered: function (entry, date) {
                root.moveOffered(entry, date);
            }
        }
    }
    ListView {
        id: list
        objectName: "continuousMonthList"
        anchors.fill: parent
        anchors.rightMargin:Theme.scrollGutter
        clip: true
        model: root.weeks
        delegate: WeekRow {}
        cacheBuffer: root.weekHeight
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {
            parent:root;x:root.width-width;y:list.y;height:list.height
            policy: ScrollBar.AsNeeded
        }
        DS.ScrollPhysics {
            target: list
        }
        onMovementStarted: {root.focusAnchor="";root.userScrolled=true}
        onContentYChanged: root.updateVisible()
        onMovementEnded: root.updateVisible()
    }
}
