pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string objectPrefix: ""
    objectName: objectPrefix + "calendarTimeline"
    property bool rangeSelectionMode: false
    readonly property var dateTaskToSchedule: {
        if(rangeSelectionMode || !selectedId)return null;
        for(const day of timeline.days)for(const entry of day.entries || [])
            if((entry.taskId || entry.eventId)===selectedId)return Object.assign({date:day.date},entry);
        return null;
    }
    signal rangeAssignmentRequested(string taskId,real version,string date,int first,int last)
    signal rangePickerRequested(var entry,Item source)
    signal selectionCleared()
    property string creationLabel: qsTr("新任务 · 未保存")
    property real initialMinute: -1
    property bool initialPositionPending: true
    function revealMinute(minute) {
        const flick=scroll.contentItem as Flickable;
        if(flick)flick.contentY=Math.max(0,Math.min(minute*minuteHeight-scroll.availableHeight/3,Math.max(0,grid.height-scroll.availableHeight)));
    }
    function positionInitially() {
        if(!initialPositionPending || !visible || timeline.days.length===0 || scroll.availableHeight<=0)return;
        const today=timeline.days.find(day=>day.today);
        revealMinute(initialMinute>=0 ? initialMinute : today ? today.nowMinute : 480);
        initialPositionPending=false;
    }
    onTimelineChanged: if(initialPositionPending)Qt.callLater(positionInitially)
    Layout.minimumWidth: 0
    implicitWidth: 640
    implicitHeight: 480
    property bool smallSurface: false
    readonly property CalendarMetrics metrics: CalendarMetrics {smallSurface: root.smallSurface}
    required property TaskWorkspace workspace
    property var timeline: ({
            days: [],
            blocks: [],
            minutes: 1440
        })
    property string selectedId: ""
    property var selectedSlot: ({})
    property bool slotSelectionActive: false
    signal taskSelected(var entry,Item source)
    signal slotSelected(string date,int minute,point at)
    function chooseSlot(date,minute,at){selectedSlot={date:date,minute:minute,at:at};slotSelected(date,minute,at)}
    function revealNow(){const day=timeline.days.find(value=>value.today);const flick=scroll.contentItem as Flickable;if(day && flick)flick.contentY=Math.max(0,Math.min(day.nowMinute*minuteHeight-scroll.availableHeight/3,grid.height-scroll.availableHeight))}
    property var preview: ({})
    property var creationSelection: ({})
    property var activeCluster: null
    readonly property int maximumLanes: Math.max(2, Math.floor(grid.dayWidth / (80 * Theme.scale)))
    readonly property bool dragging: !!preview.taskId
    readonly property real minuteHeight: Math.max(root.smallSurface ? 0.65 : 0.8, root.metrics.captionSize / (root.smallSurface ? 17 : 15))
    property real previousMinuteHeight: 0
    Component.onCompleted: previousMinuteHeight=minuteHeight
    onMinuteHeightChanged: {
        const flick=scroll.contentItem as Flickable
        const minute=flick && previousMinuteHeight>0 ? flick.contentY/previousMinuteHeight : -1
        previousMinuteHeight=minuteHeight
        if(minute>=0)Qt.callLater(function(){if(!root || !scroll)return;const view=scroll.contentItem as Flickable;if(view)view.contentY=Math.max(0,Math.min(minute*root.minuteHeight,Math.max(0,view.contentHeight-view.height)))})
    }
    readonly property real gutter: Math.max(root.smallSurface ? 44 : 56, (root.smallSurface ? 36 : 44) * Theme.scale)
    property point taskAnchor: Qt.point(0, 0)
    function inspectAt(id, item) {
        taskAnchor = item.mapToItem(root.Window.window.contentItem, item.width, item.height/2);
        openTask(id);
    }
    signal timeMoveRequested(string taskId, real version, string date, int minute, string edge)
    signal openTask(string taskId)
    signal createRequested(string date, int minute, real x, real y, int endMinute)
    signal dayRequested(string date)
    signal agendaRequested(string date)
    signal contextRequested(var task, real x, real y)
    spacing: 0
    function beginPreview(task, edge, at) {
        preview = {
            taskId: task.taskId,
            version: task.rowVersion,
            date: task.date,
            minute: Math.round(task.startMinute / 15) * 15,
            edge: edge,
            pointerX: at.x,
            pointerY: at.y,
            origin: at,
            grabOffset: edge === "move" ? at.y / minuteHeight - task.startMinute : 0,
            moved: false
        };
    }
    function updatePreview(at) {
        if (!dragging)
            return;
        if (!preview.moved && Math.abs(at.x - preview.origin.x) + Math.abs(at.y - preview.origin.y) < workspace.dragDistance)
            return;
        const column = Math.max(0, Math.min(timeline.days.length - 1, Math.floor((at.x - gutter) / grid.dayWidth)));
        const day = timeline.days[column];
        preview = Object.assign({}, preview, {
            pointerX: at.x,
            pointerY: at.y,
            date: day.date,
            minute: Math.max(0, Math.min(day.minutes, Math.round((at.y / minuteHeight - preview.grabOffset) / 15) * 15)),
            moved: true
        });
    }
    function finishPreview() {
        const value = preview;
        preview = ({});
        if (!value.taskId)
            return;
        if (value.edge === "create") {
            const first = Math.min(value.startMinute, value.minute), last = Math.max(value.startMinute, value.minute);
            const at = grid.mapToItem(root, value.pointerX, value.pointerY);
            if(value.moved && value.assignTaskId){rangeAssignmentRequested(value.assignTaskId,value.assignVersion,value.date,first,Math.max(first+15,last));return;}
            chooseSlot(value.date,first,at);
            if(value.moved)createRequested(value.date,first,at.x,at.y,Math.max(first+15,last));
        } else if (value.moved)
            timeMoveRequested(value.taskId, value.version, value.date, value.minute, value.edge);
    }
    function cancelPreview() {
        preview = ({});
    }
    Keys.onEscapePressed: cancelPreview()
    Timer {
        interval: 16
        repeat: true
        running: root.dragging && !!root.preview.moved
        onTriggered: {
            const flick = scroll.contentItem as Flickable;
            if (!flick)
                return;
            const y = root.preview.pointerY || 0;
            const delta = y - flick.contentY < 32 ? -12 : y - flick.contentY > scroll.availableHeight - 32 ? 12 : 0;
            if (delta) {
                const before = flick.contentY;
                flick.contentY = Math.max(0, Math.min(grid.height - scroll.availableHeight, before + delta));
                if (flick.contentY !== before)
                    root.updatePreview(Qt.point(root.preview.pointerX || 0, y + flick.contentY - before));
            }
        }
    }
    RowLayout {
        visible: !root.rangeSelectionMode
        Layout.fillWidth: true
        Layout.rightMargin: scroll.rightPadding
        Layout.minimumWidth: 0
        spacing: 0
        Label {
            Layout.preferredWidth: root.gutter
            text: root.gutter<4*root.metrics.captionSize ? qsTr("日期") : qsTr("日期任务")
            elide: Text.ElideRight
            Accessible.name: qsTr("仅设置日期的任务")
            ToolTip.visible: !Theme.hintsBlocked && dateTasksHover.hovered && text!==qsTr("日期任务");ToolTip.text: qsTr("只设置日期、未指定具体时刻的任务")
            ToolTip.delay: Theme.hintDelay
            ToolTip.timeout: Theme.hintTimeout
            HoverHandler {id: dateTasksHover}
            font.pixelSize: root.metrics.captionSize
            color: Theme.secondary
        }
        Repeater {
            model: root.timeline.days
            ColumnLayout {
                id: dayHeader
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredWidth: 1
                Layout.minimumWidth: 0
                spacing: 2
                Label {
                    visible: root.timeline.days.length>1
                    Layout.fillWidth: true;Layout.minimumWidth: 0
                    text: dayHeader.width<80*Theme.scale ? dayHeader.modelData.title.slice(-1) : dayHeader.modelData.title.split(" ").pop()
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: root.metrics.captionSize;color: Theme.secondary;elide: Text.ElideRight
                }
                Button {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    objectName: root.objectPrefix+"calendarDayHeader-"+dayHeader.modelData.date
                    text: root.timeline.days.length>1 ? Number(dayHeader.modelData.date.slice(8)) : dayHeader.modelData.title
                    horizontalPadding: root.timeline.days.length>1 ? 0 : 8
                    font.pixelSize: root.timeline.days.length>1 ? root.metrics.headingSize : root.metrics.bodySize
                    Accessible.name: dayHeader.modelData.title
                    flat: true
                    palette.buttonText: dayHeader.modelData.today ? Theme.danger : Theme.text
                    font.weight: dayHeader.modelData.today ? Font.DemiBold : Font.Normal
                    onClicked: root.slotSelected(dayHeader.modelData.date,-1,Qt.point(0,0))
                    onDoubleClicked: root.dayRequested(dayHeader.modelData.date)
                }
                Repeater {
                    model: dayHeader.modelData.entries.slice(0, 3)
                    Button {
                        id: dateTaskButton
                        required property var modelData
                        objectName:root.objectPrefix+"calendarDateTask-"+(modelData.taskId || modelData.eventId)
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        implicitHeight: Math.max(24, root.metrics.captionSize + 8)
                        padding: 3
                        font.pixelSize: root.metrics.captionSize
                        text: modelData.title
                        Accessible.name: modelData.title + qsTr("，日期任务")
                        background: Rectangle {
                            radius: Theme.eventRadius
                            color: root.selectedId===(dateTaskButton.modelData.taskId || dateTaskButton.modelData.eventId) ? Theme.selected : Theme.primaryFill
                            border.width: root.selectedId===(dateTaskButton.modelData.taskId || dateTaskButton.modelData.eventId) ? 1 : 0;border.color:Theme.accent
                        }
                        onClicked: root.taskSelected(Object.assign({date:dayHeader.modelData.date},modelData),this)
                        onDoubleClicked: root.inspectAt(modelData.taskId || modelData.eventId,this)
                        Keys.onReturnPressed: root.inspectAt(modelData.taskId || modelData.eventId,this)
                    }
                }
                Button {
                    visible: dayHeader.modelData.entries.length > 3
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    flat: true
                    text: qsTr("另有 %1 项").arg(dayHeader.modelData.entries.length - 3)
                    onClicked: root.activeCluster = {
                        title: qsTr("日期任务"),
                        dateTasks: true,date:dayHeader.modelData.date,
                        items: dayHeader.modelData.entries
                    }
                }
                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
    RowLayout {
        visible: !!root.dateTaskToSchedule;Layout.fillWidth:true
        Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:root.metrics.captionSize;color:Theme.accent;text:root.dateTaskToSchedule ? qsTr("为“%1”拖选时段").arg(root.dateTaskToSchedule.title) : ""}
        ActionButton {objectName:root.objectPrefix+"dateTaskChooseRange";symbol:"clock";text:qsTr("选择时段…");onClicked:root.rangePickerRequested(root.dateTaskToSchedule,this)}
        WidgetAction {symbol:"close";description:qsTr("取消安排此任务");onClicked:root.selectionCleared()}
    }
    ScrollView {
        id: scroll
        implicitHeight: 320
        implicitWidth: 400
        objectName: root.objectPrefix+"calendarTimelineScroll"
        visible: !root.activeCluster
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        contentHeight: grid.height
        Component.onCompleted: Qt.callLater(root.positionInitially)
        onAvailableHeightChanged: if(root.initialPositionPending)Qt.callLater(root.positionInitially)
        Item {
            id: grid
            width: scroll.availableWidth
            height: (root.timeline.minutes || 1440) * root.minuteHeight
            readonly property real dayWidth: Math.max(1, (width - root.gutter) / Math.max(1, root.timeline.days.length))
            Repeater {
                model: root.timeline.days
                Rectangle {
                    id: day
                    required property var modelData
                    required property int index
                    objectName: root.objectPrefix+"calendarTimelineDay-" + modelData.date
                    x: root.gutter + index * grid.dayWidth
                    width: grid.dayWidth
                    height: modelData.minutes * root.minuteHeight
                    color: Theme.content
                    Rectangle {anchors.right:parent.right;width:1;height:parent.height;color:Theme.separator}
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        preventStealing: true
                        onPressed: function (mouse) {
                            root.forceActiveFocus();
                            const at = mapToItem(grid, mouse.x, mouse.y), minute = Math.max(0, Math.min(day.modelData.minutes - 15, Math.round(mouse.y / root.minuteHeight / 15) * 15));
                            root.preview = {
                                taskId: "__create",
                                assignTaskId: root.dateTaskToSchedule ? root.dateTaskToSchedule.taskId : "",
                                assignVersion: root.dateTaskToSchedule ? root.dateTaskToSchedule.rowVersion : 0,
                                edge: "create",
                                date: day.modelData.date,
                                minute: minute,
                                startMinute: minute,
                                origin: at,
                                pointerX: at.x,
                                pointerY: at.y,
                                grabOffset: 0,
                                moved: false
                            };
                        }
                        onPositionChanged: function (mouse) {
                            if (pressed)
                                root.updatePreview(mapToItem(grid, mouse.x, mouse.y));
                        }
                        onReleased: root.finishPreview()
                        onCanceled: root.cancelPreview()
                        onDoubleClicked: function(mouse){
                            root.cancelPreview();const minute=Math.max(0,Math.min(day.modelData.minutes-15,Math.round(mouse.y/root.minuteHeight/15)*15));
                            const at=mapToItem(root,mouse.x,mouse.y);root.chooseSlot(day.modelData.date,minute,at);root.createRequested(day.modelData.date,minute,at.x,at.y,minute+60)
                        }
                    }
                    DropArea {
                        anchors.fill: parent
                        onDropped: function (drop) {
                            const task = drop.source as TaskDragPayload;
                            if (task && task.taskId) {
                                root.taskSelected(Object.assign({date:day.modelData.date},root.workspace.task(task.taskId)),day);
                                root.timeMoveRequested(task.taskId, task.rowVersion, day.modelData.date, Math.round(drop.y / root.minuteHeight / 15) * 15, "move");
                                drop.acceptProposedAction();
                            }
                        }
                    }
                    Rectangle {
                        visible: day.modelData.today
                        width: parent.width
                        height: 1
                        color: Theme.danger
                        y: day.modelData.nowMinute * root.minuteHeight
                    }
                }
            }
            Repeater {
                model: root.timeline.days.length > 0 ? root.timeline.days[0].ticks : []
                Item {
                    required property var modelData
                    y: modelData.minute * root.minuteHeight
                    width: grid.width
                    height: 1
                    Label {
                        width: root.gutter - 6
                        horizontalAlignment: Text.AlignRight
                        text: parent.modelData.label
                        font.pixelSize: root.metrics.footnoteSize
                        color: Theme.secondary
                        y: -implicitHeight / 2
                        elide: Text.ElideRight
                    }
                    Rectangle {
                        x: root.gutter
                        width: grid.width - root.gutter
                        height: 1
                        color: Theme.separator
                    }
                }
            }
            Repeater {
                model: root.timeline.blocks
                Rectangle {
                    id: block
                    required property var modelData
                    visible: modelData.laneCount <= root.maximumLanes
                    objectName: root.objectPrefix+"calendarTimeBlock-" + modelData.taskId
                    enabled: !root.rangeSelectionMode
                    x: root.gutter + modelData.day * grid.dayWidth + modelData.lane * grid.dayWidth / modelData.laneCount + 2
                    y: modelData.startMinute * root.minuteHeight
                    width: Math.max(12, grid.dayWidth / modelData.laneCount - 4)
                    height: Math.max(24, (modelData.endMinute - modelData.startMinute) * root.minuteHeight)
                    radius: Theme.eventRadius
                    color: root.selectedId === modelData.taskId ? Theme.accent : Theme.primaryFill
                    border.width: activeFocus ? 1 : 0
                    border.color: Theme.accent
                    Rectangle {anchors.left:parent.left;anchors.top:parent.top;anchors.bottom:parent.bottom;width:3;radius:2;color:Theme.accent}
                    focus: false
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.title + "，" + modelData.timeLabel
                    Keys.onReturnPressed: root.inspectAt(modelData.taskId, block)
                    Keys.onSpacePressed: root.inspectAt(modelData.taskId, block)
                    Keys.onPressed: function (event) {
                        if (event.key === Qt.Key_Menu || (event.key === Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))) {
                            const at = block.mapToItem(root, block.width, 0);
                            root.contextRequested(block.modelData, at.x, at.y);
                            event.accepted = true;
                        }
                    }
                    Keys.onEscapePressed: root.cancelPreview()
                    Column {
                        anchors.fill: parent
                        anchors.margins: 5
                        clip: true
                        spacing: 2
                        Label {
                            width: parent.width
                            text: block.modelData.title
                            font.pixelSize: root.metrics.captionSize
                            font.weight: Font.DemiBold
                            font.strikeout: block.modelData.taskStatus === "COMPLETED"
                            color: root.selectedId===block.modelData.taskId ? Theme.accentText : Theme.primaryInk
                            elide: Text.ElideRight
                        }
                        Label {
                            width: parent.width
                            text: block.modelData.shortTimeLabel
                            font.pixelSize: root.metrics.footnoteSize
                            color: root.selectedId===block.modelData.taskId ? Theme.accentText : Theme.primaryInk
                            elide: Text.ElideRight
                            visible: block.height > 40
                        }
                    }
                    MouseArea {
                        id: hit
                        anchors.fill: parent
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        preventStealing: true
                        cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                        onPressed: function (mouse) {
                            block.forceActiveFocus();
                            root.taskSelected(block.modelData,block);
                            if (mouse.button === Qt.RightButton || (Qt.platform.os === "osx" && (mouse.modifiers & Qt.MetaModifier))) {
                                const at = mapToItem(root, mouse.x, mouse.y);
                                root.contextRequested(block.modelData, at.x, at.y);
                            } else
                                root.beginPreview(block.modelData, "move", mapToItem(grid, mouse.x, mouse.y));
                        }
                        onPositionChanged: function (mouse) {
                            if (pressed)
                                root.updatePreview(mapToItem(grid, mouse.x, mouse.y));
                        }
                        onReleased: root.finishPreview()
                        onCanceled: root.cancelPreview()
                        onDoubleClicked: root.inspectAt(block.modelData.taskId, block)
                    }
                    Repeater {
                        model: ["start", "end"]
                        MouseArea {
                            required property string modelData
                            anchors.left: block.left
                            anchors.right: block.right
                            y: modelData === "start" ? 0 : block.height - height
                            height: 6
                            visible: root.selectedId===block.modelData.taskId || block.activeFocus
                            preventStealing: true
                            cursorShape: Qt.SizeVerCursor
                            onPressed: function (mouse) {
                                root.beginPreview(block.modelData, modelData, mapToItem(grid, mouse.x, mouse.y));
                            }
                            onPositionChanged: function (mouse) {
                                if (pressed)
                                    root.updatePreview(mapToItem(grid, mouse.x, mouse.y));
                            }
                            onReleased: root.finishPreview()
                            onCanceled: root.cancelPreview()
                        }
                    }
                    WidgetAction {
                        anchors.right: parent.right
                        anchors.top: parent.top
                        symbol: "info"
                        description: qsTr("任务详情：%1").arg(block.modelData.title)
                        visible: block.width > 100 && (root.selectedId===block.modelData.taskId || block.activeFocus)
                        onClicked: root.inspectAt(block.modelData.taskId, block)
                    }
                }
            }
            Repeater {
                model: root.timeline.clusters || []
                Button {
                    required property var modelData
                    objectName: root.objectPrefix+"calendarOverlap-" + modelData.key
                    enabled: !root.rangeSelectionMode
                    visible: modelData.laneCount > root.maximumLanes
                    x: root.gutter + modelData.day * grid.dayWidth + 2
                    y: modelData.startMinute * root.minuteHeight
                    width: grid.dayWidth - 4
                    height: Math.max(32, (modelData.endMinute - modelData.startMinute) * root.minuteHeight)
                    text: qsTr("重叠 %1 项").arg(modelData.items.length)
                    Accessible.description: qsTr("打开此时段的任务列表")
                    onClicked: root.activeCluster = modelData
                }
            }
            Rectangle {
                visible: root.slotSelectionActive && !!root.selectedSlot.date && !root.selectedId && !root.creationSelection.date && !root.dragging
                x: root.gutter+Math.max(0,root.timeline.days.findIndex(day=>day.date===root.selectedSlot.date))*grid.dayWidth+2
                y: (root.selectedSlot.minute || 0)*root.minuteHeight
                width: grid.dayWidth-4;height:60*root.minuteHeight
                color:Theme.hover;border.color:Theme.separator;radius:Theme.eventRadius
            }
            Rectangle {
                visible:!!root.creationSelection.date
                x:root.gutter+Math.max(0,root.timeline.days.findIndex(day=>day.date===root.creationSelection.date))*grid.dayWidth+2
                y:(root.creationSelection.minute || 0)*root.minuteHeight
                width:grid.dayWidth-4
                height:Math.max(32,(root.creationSelection.duration || 60)*root.minuteHeight)
                color:Theme.selected;border.color:Theme.accent;radius:Theme.eventRadius
                Label {anchors.fill:parent;anchors.margins:6;text:root.creationLabel;color:Theme.accent;font.pixelSize:root.metrics.captionSize;elide:Text.ElideRight}
            }
            Rectangle {
                visible: root.preview.edge === "create" && !!root.preview.moved
                x: root.gutter + Math.max(0, root.timeline.days.findIndex(day => day.date === root.preview.date)) * grid.dayWidth
                width: grid.dayWidth
                y: Math.min(root.preview.startMinute || 0, root.preview.minute || 0) * root.minuteHeight
                height: Math.max(15, Math.abs((root.preview.startMinute || 0) - (root.preview.minute || 0))) * root.minuteHeight
                color: Theme.selected
                border.color: Theme.accent
                border.width: 2
            }
            Rectangle {
                visible: root.preview.moved || false
                x: root.gutter
                width: grid.width - root.gutter
                height: 2
                color: Theme.accent
                y: (root.preview.minute || 0) * root.minuteHeight
                Label {
                    anchors.right: parent.right
                    anchors.bottom: parent.top
                    padding: 4
                    text: (root.preview.date || "") + " · " + root.workspace.minuteLabel(root.preview.date || "", root.preview.minute || 0, root.timeline.zone || "")
                    background: Rectangle {
                        color: Theme.content
                        border.color: Theme.accent
                    }
                    font.pixelSize: root.metrics.captionSize
                }
            }
        }
    }
    ColumnLayout {
        visible: !!root.activeCluster
        Layout.fillWidth: true
        Layout.fillHeight: true
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: qsTr("返回时间轴")
                flat: true
                onClicked: root.activeCluster = null
            }
            Label {
                Layout.fillWidth: true
                text: root.activeCluster ? (root.activeCluster.title || qsTr("此时段重叠 %1 项").arg(root.activeCluster.items.length)) : ""
                font.weight: Font.DemiBold
            }
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.activeCluster ? root.activeCluster.items : []
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                text: modelData.title + (modelData.shortTimeLabel ? " · " + modelData.shortTimeLabel : "")
                onClicked: {
                    if(root.activeCluster.dateTasks){const entry=Object.assign({date:root.activeCluster.date},modelData);root.activeCluster=null;root.taskSelected(entry,root);root.forceActiveFocus();}
                    else root.inspectAt(modelData.taskId,this);
                }
            }
        }
    }
}
