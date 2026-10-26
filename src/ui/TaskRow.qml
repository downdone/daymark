pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
    objectName: "taskRow-"+taskId
    function openVerificationMenu() {menu.open()}
    function closeVerificationMenu() {menu.close()}
    function verifyMenuHighlight() {
        let active=0
        for(let i=0;i<menu.count;i++) {
            const item=menu.itemAt(i) as MenuItem
            if(!item)continue
            const surface=item.background as Rectangle
            if(surface && surface.color!==undefined && surface.color.a>0) {if(!item.enabled)return "disabled item filled";++active}
        }
        return active<=1 ? "PASS" : "multiple active menu rows"
    }
    function hoverDisabledMenuPosition() {
        for(let i=0;i<menu.count;i++) {const item=menu.itemAt(i) as MenuItem;if(item && item.visible && !item.enabled && item.text===qsTr("移到")){const at=item.mapToItem(null,item.width/2,item.height/2);return {x:at.x,y:at.y}}}
        return {}
    }
    function showContextMenu(x,y) {menu.popup(root,x,y)}
    signal contextRequested(real x,real y)
    signal selectionGestureRequested(int modifiers)
    function verifyMenuSpacing() {
        let bottom=0
        for(let i=0;i<menu.count;i++) {
            const item=menu.itemAt(i)
            if(!item.visible) {if(item.height>0.5)return "hidden item height="+item.height;continue}
            if(Math.abs(item.y-bottom)>1)return "gap at item "+i+": "+item.y+" vs "+bottom
            bottom=item.y+item.height
        }
        return Math.abs(menu.height-bottom-menu.topPadding-menu.bottomPadding)<1 ? "PASS" : "menu height"
    }
    required property TaskWorkspace workspace
    required property string taskId
    required property string title
    required property string notes
    required property string startOn
    required property string deadlineOn
    required property string projectName
    required property string tagSummary
    required property string taskStatus
    required property string planningState
    required property bool parentDeleted
    property bool showSeparator: true
    property bool deleted: false
    property bool smartView: false
    readonly property real treeGutter: smartView || !workspace.tasks.hasHierarchy ? 0 : 32
    property string completionText: ""
    signal subtaskRequested()
    signal restoreRequested()
    required property bool isDone
    required property bool isOverdue
    required property real rowVersion
    required property int subtaskTotal
    required property string arrangementSummary
    required property int subtaskPending
    required property bool evening
    required property bool inToday
    required property string parentTitle
    required property int visibleChildCount
    property bool childrenExpanded: true
    signal childrenExpansionRequested(bool value)
    required property int taskDepth
    required property bool pinned
    required property bool important
    signal importantRequested(bool value)
    signal cancelReminderRequested()
    signal deleteRequested()
    property bool hasReminder: false
    signal pinRequested(bool value)
    property bool showTodayMarker: true
    property bool showCategory: true
    readonly property string relationSymbol: parentTitle || taskDepth>0 ? "subtask" : subtaskTotal>0 ? "subtasks" : ""
    readonly property string metadataText: [
        arrangementSummary ? (isOverdue ? qsTr("已逾期 · ") : "")+arrangementSummary : "",
        taskDepth===0 && parentTitle ? (parentDeleted ? qsTr("原父任务（已删除）：%1") : qsTr("父任务：%1")).arg(parentTitle) : "",
        completionText,
        planningState==="SOMEDAY" ? qsTr("已暂停") : "",
        inToday && showTodayMarker && !arrangementSummary ? qsTr("今天") : "",
        showCategory ? projectName : "",tagSummary,
        taskStatus==="CANCELLED" ? qsTr("已取消") : ""
    ].filter(value=>value.length>0).join(" · ")
    property bool selectionMode: false
    property bool selectedForBatch: false
    property bool canMoveUp: true
    property bool canMoveDown: true
    property bool canReorder: false
    property int dragThreshold: 10
    property bool canDrag: canReorder
    property Item dragLayer: null
    readonly property bool pointerHeld: holdArea.pressed || handleArea.pressed
    signal pointerStateChanged(bool held)
    onPointerHeldChanged: pointerStateChanged(pointerHeld)
    property bool heldPress: false
    property bool dragArmed: false
    property bool dragging: false
    property real dragOriginX: 0
    property real dragOriginY: 0
    property bool canActivate: false
    property bool expanded: false
    property TaskQuickEditor inlineEditor: null
    readonly property Item editorHost: inlineHost
    property bool reduceMotion: false
    property bool completionFeedback: false
    property bool removingAfterCompletion: false
    function prepareCompletion(){completionFeedback=true}
    function cancelCompletion(){completionFeedback=false;removingAfterCompletion=false}
    signal editorDetached()
    Component.onDestruction: if(expanded)editorDetached()
    ListView.onRemove: {
        if(completionFeedback && !reduceMotion){removingAfterCompletion=true;ListView.delayRemove=true;completedRemoval.start()}
    }
    SequentialAnimation {
        id: completedRemoval
        PauseAnimation {duration: 110}
        ParallelAnimation {
            NumberAnimation {target: root;property: "opacity";to: 0;duration: 80;easing.type: Easing.OutQuad}
            NumberAnimation {target: root;property: "height";to: 0;duration: 80;easing.type: Easing.OutQuad}
        }
        ScriptAction {script: root.ListView.delayRemove=false}
    }
    signal selectionRequested()
    signal completeRequested()
    signal editRequested()
    signal quickEditRequested()
    signal moveRequested(int direction)
    signal dropRequested(string source)
    signal activateRequested()
    signal eveningRequested(bool value)
    signal dragStateChanged(bool active)
    signal menuRequested()
    signal moveToViewRequested(string view)
    Menu {
        id: menu;objectName: "taskContext-"+root.taskId
        onAboutToShow: root.menuRequested()
        MenuItem {text: qsTr("编辑详情…");onTriggered: root.editRequested()}
        MenuItem {objectName: "taskMenuPin-"+root.taskId;text: root.pinned ? qsTr("取消置顶") : qsTr("置顶");onTriggered: root.pinRequested(!root.pinned)}
        MenuItem {objectName: "taskMenuImportant-"+root.taskId;text: root.important ? qsTr("取消特别关注") : qsTr("特别关注");onTriggered: root.importantRequested(!root.important)}
        MenuItem {objectName: "taskMenuCancelReminder-"+root.taskId;text: qsTr("取消提醒");visible: root.hasReminder;onTriggered: root.cancelReminderRequested()}
        MenuSeparator {}
        MenuItem {objectName: "taskMenuState-"+root.taskId;text: root.isDone ? qsTr("重新打开") : qsTr("完成任务");onTriggered: root.completeRequested()}
        MenuItem {text: qsTr("上移");visible: root.canReorder && root.canMoveUp;onTriggered: root.moveRequested(-1)}
        MenuItem {text: qsTr("下移");visible: root.canReorder && root.canMoveDown;onTriggered: root.moveRequested(1)}
        Menu {
            objectName:"taskMenuMove-"+root.taskId;title: qsTr("移到");enabled: !root.isDone
            MenuItem {text: qsTr("收集箱");onTriggered: root.moveToViewRequested("inbox")}
            MenuItem {text: qsTr("今天");onTriggered: root.moveToViewRequested("today")}
            MenuItem {text: qsTr("指定日期…");onTriggered: root.moveToViewRequested("upcoming")}
        }
        MenuItem {text: root.evening ? qsTr("安排到今天") : qsTr("安排到今晚");visible: root.inToday && !root.isDone;onTriggered: root.eveningRequested(!root.evening)}
        MenuItem {text: qsTr("清除日期安排");visible: root.canActivate;onTriggered: root.activateRequested()}
        MenuSeparator {}
        DS.MenuItem {objectName: "taskMenuDelete-"+root.taskId;text: qsTr("删除任务…");destructive: true;onTriggered: root.deleteRequested()}
    }
    function placeDrag(x,y){const at=mapToItem(dragProxy.parent,x,y);dragProxy.x=at.x-12;dragProxy.y=at.y-dragOriginY}
    function armDrag(x,y){dragOriginX=x;dragOriginY=y;dragArmed=true;dragProxy.parent=dragLayer || root;placeDrag(x,y);forceActiveFocus()}
    function updateDrag(x,y){if(!dragArmed)return;const dx=x-dragOriginX,dy=y-dragOriginY;if(!dragging && Math.abs(dx)+Math.abs(dy)>2)dragging=true;if(dragging)placeDrag(x,y)}
    function finishDrag(drop){if(!dragArmed)return;dragArmed=false;if(dragging){if(drop)dragProxy.Drag.drop();else dragProxy.Drag.cancel()}dragging=false;dragProxy.parent=root;dragProxy.x=0;dragProxy.y=0}
    function dragPointIn(item){return dragProxy.mapToItem(item,12,dragOriginY)}
    function compensateScroll(delta){if(dragProxy.parent===root)dragProxy.y+=delta}
    onDraggingChanged: dragStateChanged(dragging)
    z: dragging ? 100 : 0
    Keys.onEscapePressed: function(event){if(dragging || dragArmed)finishDrag(false);else event.accepted=false}
    Keys.onPressed: function(event){if(!root.deleted && (event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))){root.contextRequested(width/2,height/2);event.accepted=true}}
    TapHandler {acceptedButtons:Qt.LeftButton;acceptedModifiers:Qt.MetaModifier;enabled:Qt.platform.os==="osx" && root.expanded && !root.deleted;onTapped:function(point){root.contextRequested(point.position.x,point.position.y)}}
    TapHandler {acceptedButtons: Qt.RightButton;enabled: !root.deleted && root.expanded;onTapped: function(point){root.contextRequested(point.position.x,point.position.y)}}
    MouseArea {
        id: holdArea;objectName: "taskHoldDrag";anchors.fill: parent;enabled: !root.expanded && !root.deleted
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        pressAndHoldInterval: 350;preventStealing: root.dragArmed;cursorShape: root.dragArmed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: function(mouse){root.heldPress=false;if(mouse.button===Qt.RightButton || (Qt.platform.os==="osx" && (mouse.modifiers & Qt.MetaModifier))){root.contextRequested(mouse.x,mouse.y);return}root.dragOriginX=mouse.x;root.dragOriginY=mouse.y}
        onPressAndHold: function(mouse){if(mouse.button===Qt.LeftButton && root.canDrag && !root.selectionMode && !(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))){root.heldPress=true;root.armDrag(mouse.x,mouse.y)}}
        onDoubleClicked: function(mouse){if(mouse.button===Qt.LeftButton && !root.selectionMode && !mouse.modifiers)root.editRequested()}
        onClicked: function(mouse){
            if(mouse.button!==Qt.LeftButton || (Qt.platform.os==="osx" && (mouse.modifiers & Qt.MetaModifier)) || root.heldPress)return
            if(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))root.selectionGestureRequested(mouse.modifiers)
            else if(root.selectionMode)root.selectionRequested()
            else root.quickEditRequested()
        }
        onReleased: root.finishDrag(true)
        onCanceled: root.finishDrag(false)
        onPositionChanged: function(mouse){
            if(!(pressedButtons & Qt.LeftButton) || !root.canDrag || root.selectionMode || (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)))return
            const point=mapToItem(root,mouse.x,mouse.y)
            if(!root.dragArmed && Math.abs(point.x-root.dragOriginX)+Math.abs(point.y-root.dragOriginY)>root.dragThreshold){root.heldPress=true;root.armDrag(root.dragOriginX,root.dragOriginY)}
            if(root.dragArmed)root.updateDrag(point.x,point.y)
        }
    }
    Item {
        id: dragProxy;z: 1000;width: Math.min(root.width,360*Theme.scale);height: root.height
        Drag.active: root.dragging;Drag.source: root;Drag.hotSpot.x: 12;Drag.hotSpot.y: root.dragOriginY;Drag.supportedActions: Qt.MoveAction
        Rectangle {anchors.fill: parent;visible: root.dragging;color: Theme.content;radius: 12;border.color: Theme.accent;opacity: 0.95
            Text {anchors.fill: parent;anchors.margins: 12;text: root.title;font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize;color: Theme.text;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
        }
    }
    DropArea {id: dropZone;anchors.fill: parent;enabled: root.canReorder
        property bool allowed:false
        readonly property TaskRow sourceTask:drag.source as TaskRow
        onEntered:function(drag){const source=drag.source as TaskRow;allowed=!!source && root.workspace.canDropTask(source.taskId,root.taskId)}
        onExited:allowed=false
        onDropped:function(drop){const source=drop.source as TaskRow;if(source && root.workspace.canDropTask(source.taskId,root.taskId)){root.dropRequested(source.taskId);drop.acceptProposedAction()}allowed=false}
    }
    Rectangle {anchors.left:parent.left;anchors.right:parent.right;anchors.bottom:parent.bottom;height:2;color:Theme.accent;visible:dropZone.containsDrag && dropZone.allowed}
    DS.ToolTip {contextualFeedback:true;delay:0;timeout:-1;visible:dropZone.containsDrag && !dropZone.allowed && !!dropZone.sourceTask;text:dropZone.sourceTask ? root.workspace.dropTaskReason(dropZone.sourceTask.taskId,root.taskId) : ""}
    padding: Theme.compact ? 2 : 4
    leftPadding: 6+Math.min(8,taskDepth)*24
    rightPadding: 10
    function columnPositions() {
        const completion=expanded ? inlineEditor.completionItem : completionAction
        const titleItem=expanded ? inlineEditor.titleItem : summary.titleItem
        const notesItem=expanded ? inlineEditor.notesItem : summary.notesItem
        return {completion:completion.mapToItem(root,0,0).x,title:titleItem.mapToItem(root,0,0).x,notes:notesItem.mapToItem(root,0,0).x,separator:rowSeparator.x,depth:taskDepth}
    }
    implicitHeight: Math.max(Theme.taskRowHeight,contentItem.implicitHeight+topPadding+bottomPadding)
    function verifyGeometry() {
        if(!expanded && summary.mapToItem(root,0,0).y+summary.height>root.height-bottomPadding+1)return "summary exceeds row"
        return expanded ? "PASS" : summary.verifyGeometry()
    }
    Accessible.name: (relationSymbol==="subtask" ? qsTr("子任务，") : relationSymbol==="subtasks" ? qsTr("包含子任务，") : "") + title + (isDone ? qsTr("，已结束") : qsTr("，未完成")) + (pinned ? qsTr("，已置顶") : "") + (important ? qsTr("，特别关注") : "")
    onClicked: selectionMode ? selectionRequested() : quickEditRequested()
    contentItem: ColumnLayout {
        z: 1
        spacing: 0
        RowLayout {
        Layout.fillWidth: true
        visible: !root.expanded
        spacing: 8
        Item {
            visible:root.treeGutter>0;Layout.preferredWidth: 24;Layout.preferredHeight: 32
            WidgetAction {
                anchors.centerIn: parent;visible: !root.smartView && root.visibleChildCount>0
                objectName: "taskChildren-"+root.taskId
                symbol: root.childrenExpanded ? "down" : "right"
                description: (root.childrenExpanded ? qsTr("收起 %1 个子任务") : qsTr("展开 %1 个子任务")).arg(root.visibleChildCount)
                onClicked: root.childrenExpansionRequested(!root.childrenExpanded)
            }
        }
        CheckBox {
            visible: root.selectionMode;Layout.preferredWidth: Math.max(32,Theme.iconSize+12);Layout.preferredHeight: Math.max(32,Theme.iconSize+12)
            checked: root.selectedForBatch;Accessible.name: qsTr("选择任务：")+root.title;onClicked: root.selectionRequested()
            MouseArea {anchors.fill: parent;onClicked: function(mouse){if(mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))root.selectionGestureRequested(mouse.modifiers);else root.selectionRequested()}}
        }
        IconToolButton {
            id: completionAction
            hintEnabled: false
            visible: !root.selectionMode
            enabled: !root.deleted
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Math.max(32,Theme.iconSize+12)
            Layout.preferredHeight: Math.max(32,Theme.iconSize+12)
            Accessible.name: (root.isDone ? qsTr("恢复任务：") : qsTr("完成任务："))+root.title
            onClicked: root.completeRequested()
            contentItem: Item {
                implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize
                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(24,18*Theme.scale);height: width;radius: width/2
                    color: (root.isDone || root.removingAfterCompletion) ? Theme.accent : "transparent"
                    border.color: (root.isDone || root.removingAfterCompletion) ? Theme.accent : Theme.secondary
                    border.width: 1.25
                    AppIcon {anchors.centerIn: parent;name: "check";width: Math.round(Theme.iconSize*2/3);height: width;visible: root.isDone || root.removingAfterCompletion;ink: Theme.accentText}
                }
            }
        }
        TaskRowSummary {
            id: summary
            Layout.fillWidth: true
            taskId: root.taskId;title: root.title;notes: root.notes;metadataText: root.metadataText
            done: root.isDone || root.removingAfterCompletion;overdue: root.isOverdue
            showSubtasks: root.subtaskTotal>0 && !root.deleted && (root.smartView || !root.childrenExpanded || !root.workspace.preferences.listShowSubtasks)
            subtaskTotal: root.subtaskTotal;subtaskPending: root.subtaskPending
            interactive: !root.selectionMode
            onSubtasksRequested: if(!root.selectionMode)root.subtaskRequested()
        }
        AppIcon {name:"pin";visible:root.pinned;Layout.preferredWidth:14;Layout.preferredHeight:14;ink:Theme.secondary;Accessible.name:qsTr("已置顶")}
        AppIcon {name:"flag";visible:root.important;Layout.preferredWidth:14;Layout.preferredHeight:14;ink:Theme.catalogColor("orange");Accessible.name:qsTr("特别关注")}
        Item {
            opacity: root.canDrag && !root.selectionMode && (root.hovered || root.activeFocus || root.dragArmed) ? 1 : 0
            Accessible.ignored: !root.canDrag
            Layout.preferredWidth: 28;Layout.preferredHeight: 32
            Accessible.name: qsTr("拖动排序或拖到导航栏移动任务")
            AppIcon {anchors.centerIn: parent;name: "grip"}
            MouseArea {
                id: handleArea;objectName: "taskDragHandle";anchors.fill: parent;enabled: root.canDrag && !root.selectionMode;cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                preventStealing: true
                onPressed: function(mouse){const point=mapToItem(root,mouse.x,mouse.y);root.armDrag(point.x,point.y)}
                onPositionChanged: function(mouse){if(pressed && root.dragArmed){const point=mapToItem(root,mouse.x,mouse.y);root.updateDrag(point.x,point.y)}}
                onReleased: root.finishDrag(true)
                onCanceled: root.finishDrag(false)
            }
        }
        IconToolButton {
            objectName: "taskDetails-"+root.taskId
            opacity: !root.selectionMode && (root.hovered || root.highlighted || root.activeFocus || activeFocus) ? 1 : 0
            Accessible.ignored: root.selectionMode
            enabled: !root.selectionMode
            contentItem: AppIcon {name: root.deleted ? "undo" : "info"}
            Accessible.name: (root.deleted ? qsTr("恢复任务：") : qsTr("打开任务详情："))+root.title
            hintText: root.deleted ? qsTr("恢复任务") : qsTr("编辑任务详情")
            onClicked: {if(root.deleted)root.restoreRequested();else root.editRequested()}

        }
        }
        Item {id: inlineHost;Layout.fillWidth: true;Layout.leftMargin: root.treeGutter;visible: root.expanded;implicitHeight: root.inlineEditor ? root.inlineEditor.implicitHeight : 0}
    }
    background: Rectangle {
        opacity: root.dragging ? 0.5 : 1
        radius: Theme.rowRadius
        color: root.selectionMode ? (root.selectedForBatch ? Theme.selected : root.hovered ? Theme.hover : "transparent") : root.expanded ? "transparent" : root.highlighted ? Theme.selected : root.hovered ? Theme.hover : "transparent"
        border.width: root.visualFocus || root.dragArmed ? 1 : 0;border.color: Theme.accent
        Rectangle {visible: dropZone.containsDrag && !root.dragging;height: 2;width: parent.width;anchors.top: parent.top;color: Theme.accent}
        Rectangle {id:rowSeparator;objectName: "taskRowSeparator";visible: root.showSeparator;height: 1;x: root.leftPadding+root.treeGutter+completionAction.width+8;width: Math.max(0,parent.width-x-root.rightPadding);anchors.bottom: parent.bottom;color: Theme.separator}
    }
}
