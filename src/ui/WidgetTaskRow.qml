pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS

ItemDelegate {
    id: row
    required property TaskWorkspace workspace
    required property int index
    required property string taskId
    required property string title
    required property real rowVersion
    required property string arrangementSummary
    required property string parentTitle
    required property int subtaskTotal
    required property bool isOverdue
    required property bool important
    required property bool pinned
    property Item dragLayer
    property bool dragging: false
    property point dragPosition: Qt.point(0,0)
    property bool reduceMotion: false
    property bool completing: false
    property bool removing: false
    signal openRequested(string id)
    signal actionStarted()
    signal errorReported(string message)
    signal contextRequested(string id,Item source,point position)
    signal dragStateChanged(bool active,point position)
    signal reorderStarted()
    Component.onDestruction: if(dragging)dragStateChanged(false,Qt.point(0,0))
    function finishDrag(commit) {
        if(!dragging)return
        if(commit)dragProxy.Drag.drop();else dragProxy.Drag.cancel()
        dragging=false;dragProxy.parent=row;dragStateChanged(false,Qt.point(0,0))
    }
    objectName: "widgetTask-"+taskId
    readonly property string summary: [parentTitle, arrangementSummary].filter(value => value.length>0).join(" · ")
    function complete() {
        if(!workspace.ready || workspace.busy || completing)return
        actionStarted()
        completing=workspace.changeState(taskId,rowVersion,"complete")
        if(!completing && workspace.errorText.length)errorReported(workspace.errorText)
    }
    ListView.onRemove: {
        if(completing && !reduceMotion) {
            removing=true
            ListView.delayRemove=true
            removal.start()
        }
    }
    SequentialAnimation {
        id: removal
        PauseAnimation {duration: 110}
        ParallelAnimation {
            NumberAnimation {target: row;property: "opacity";to: 0;duration: 80;easing.type: Easing.OutQuad}
            NumberAnimation {target: row;property: "height";to: 0;duration: 80;easing.type: Easing.OutQuad}
        }
        ScriptAction {script: row.ListView.delayRemove=false}
    }
    Connections {
        target: row.workspace
        function onOperationFinished(operation,success) {
            if(operation==="complete" && row.completing && !success){row.completing=false;row.errorReported(row.workspace.errorText)}
        }
    }
    implicitHeight: Math.max(Theme.taskRowHeight,body.implicitHeight+8)
    leftPadding: 0;rightPadding: 6;topPadding: 4;bottomPadding: 4
    Accessible.name: title+(summary.length ? ", "+summary : "")
    onClicked: openRequested(taskId)
    Keys.onSpacePressed: complete()
    Keys.onEscapePressed: function(event){if(dragging)finishDrag(false);else event.accepted=false}
    Keys.onPressed: function(event){if(event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))){contextRequested(taskId,row,Qt.point(width/2,height/2));event.accepted=true}}
    TaskDragGesture {
        objectName: "widgetTaskGesture-"+row.taskId
        anchors.leftMargin: 36
        enabled: row.workspace.ready && !row.workspace.busy
        entry: ({taskId:row.taskId})
        dragDistance: row.workspace.dragDistance
        contextOnRelease: true
        onActivated: row.openRequested(row.taskId)
        onContextRequested: function(position){row.contextRequested(row.taskId,row,mapToItem(row,position.x,position.y))}
        onDragStarted: function(entry,source,origin){
            row.forceActiveFocus();row.dragging=true;dragProxy.parent=row.dragLayer || row
            const at=source.mapToItem(dragProxy.parent,origin.x,origin.y);dragProxy.x=at.x-12;dragProxy.y=at.y-row.height/2
            row.dragStateChanged(true,source.mapToItem(row.dragLayer || row,origin.x,origin.y))
        }
        onDragMoved: function(source,position){
            if(!row.dragging)return
            const at=source.mapToItem(dragProxy.parent,position.x,position.y);dragProxy.x=at.x-12;dragProxy.y=at.y-row.height/2
            row.dragStateChanged(true,at)
        }
        onDragEnded: function(commit){row.finishDrag(commit)}
    }
    Item {
        id: dragProxy;z:1000;width:row.width;height:row.height
        Drag.active:row.dragging;Drag.source:row;Drag.keys:["daymark-widget-task"];Drag.hotSpot.x:12;Drag.hotSpot.y:row.height/2;Drag.supportedActions:Qt.MoveAction
        Rectangle {
            anchors.fill:parent;visible:row.dragging;color:Theme.content;radius:Theme.rowRadius;border.color:Theme.accent
            Label {anchors.fill:parent;anchors.margins:8;text:row.title;textFormat:Text.PlainText;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
        }
    }
    DropArea {
        id:dropZone;anchors.fill:parent;keys:["daymark-widget-task"]
        property bool allowed:false
        readonly property WidgetTaskRow sourceTask:drag.source as WidgetTaskRow
        onEntered:function(drag){const source=drag.source as WidgetTaskRow;allowed=!!source && row.workspace.canDropTodayTask(source.taskId,row.taskId)}
        onExited:allowed=false
        onDropped:function(drop){const source=drop.source as WidgetTaskRow;if(source && row.workspace.canDropTodayTask(source.taskId,row.taskId)){row.actionStarted();if(row.workspace.dropTodayTask(source.taskId,row.taskId)){row.reorderStarted();drop.acceptProposedAction()}else row.errorReported(row.workspace.errorText)}allowed=false}
    }
    Rectangle {x:0;y:dropZone.sourceTask && dropZone.sourceTask.index>row.index ? 0 : row.height-2;width:row.width;height:2;color:Theme.accent;visible:dropZone.containsDrag && dropZone.allowed}
    DS.ToolTip {contextualFeedback:true;delay:0;timeout:-1;visible:dropZone.containsDrag && !dropZone.allowed && !!dropZone.sourceTask;text:qsTr("只能在同父任务、同置顶组内排序；具体时段请在日历调整。")}
    contentItem: RowLayout {
        id: body
        spacing: 4
        IconToolButton {
            objectName: "widgetComplete-"+row.taskId
            hintEnabled: false
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: 32
            Layout.preferredHeight: Math.max(32,Theme.iconSize+8)
            enabled: row.workspace.ready && !row.workspace.busy && !row.completing
            Accessible.name: qsTr("完成任务：%1").arg(row.title)
            onClicked: row.complete()
            contentItem: Item {
                implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize
                Rectangle {
                    anchors.centerIn: parent
                    width: Theme.iconSize;height: width;radius: width/2
                    color: row.removing ? Theme.accent : "transparent"
                    border.color: row.removing ? Theme.accent : Theme.secondary
                    AppIcon {anchors.centerIn: parent;width: Theme.iconSize*2/3;height: width;name: "check";ink: Theme.accentText;visible: row.removing}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true;Layout.minimumWidth: 0
            spacing: 2
            HoverHandler {id: textHover}
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                AppIcon {
                    visible: row.parentTitle.length>0 || row.subtaskTotal>0
                    name: row.parentTitle.length>0 ? "subtask" : "subtasks"
                    Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize
                    Accessible.name: row.parentTitle.length>0 ? qsTr("子任务") : qsTr("包含子任务")
                }
                Label {
                    id: titleLabel
                    Layout.fillWidth: true;Layout.minimumWidth: 0
                    text: row.title;textFormat: Text.PlainText;font.pixelSize: Theme.listTitleSize
                    font.strikeout: row.removing;color: row.removing ? Theme.secondary : Theme.text
                    elide: Text.ElideRight
                }
                AppIcon {visible: row.important;name: "flag";ink: Theme.accent;Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize;Accessible.name: qsTr("特别关注")}
                AppIcon {visible:row.pinned;name:"pin";ink:Theme.secondary;Layout.preferredWidth:Theme.iconSize;Layout.preferredHeight:Theme.iconSize;Accessible.name:qsTr("已置顶")}
            }
            Label {
                id: summaryLabel
                visible: row.summary.length>0
                Layout.fillWidth: true;Layout.minimumWidth: 0
                text: row.summary;textFormat: Text.PlainText;font.pixelSize: Theme.captionSize
                color: row.isOverdue ? Theme.danger : Theme.secondary;elide: Text.ElideRight
            }
        }
    }
    ToolTip.visible: !Theme.hintsBlocked && textHover.hovered && !down && !completing && !dragging && (titleLabel.truncated || summaryLabel.truncated)
    ToolTip.delay: Theme.hintDelay
    ToolTip.timeout: Theme.hintTimeout
    ToolTip.text: title+(summary.length ? "\n"+summary : "")
    background: Rectangle {
        radius: Theme.rowRadius
        color: row.down ? Theme.pressed : row.hovered ? Theme.hover : "transparent"
        border.width: row.visualFocus || row.highlighted ? 1 : 0;border.color: Theme.accent
    }
}
