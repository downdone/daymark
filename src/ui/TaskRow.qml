pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
    objectName: "taskRow-"+taskId
    function openVerificationMenu() {menu.open()}
    function closeVerificationMenu() {menu.close()}
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
    required property string taskId
    required property string title
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
    property string completionText: ""
    signal subtaskRequested()
    signal restoreRequested()
    required property bool isDone
    required property bool isOverdue
    required property real rowVersion
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
        MenuItem {objectName: "taskMenuCancelReminder-"+root.taskId;text: qsTr("取消提醒");enabled: root.hasReminder;onTriggered: root.cancelReminderRequested()}
        MenuSeparator {}
        MenuItem {text: root.isDone ? qsTr("重新打开") : qsTr("完成任务");onTriggered: root.completeRequested()}
        MenuItem {text: qsTr("上移");enabled: root.canReorder && root.canMoveUp;onTriggered: root.moveRequested(-1)}
        MenuItem {text: qsTr("下移");enabled: root.canReorder && root.canMoveDown;onTriggered: root.moveRequested(1)}
        Menu {
            title: qsTr("移到");enabled: !root.isDone
            MenuItem {text: qsTr("收集箱");onTriggered: root.moveToViewRequested("inbox")}
            MenuItem {text: qsTr("今天");onTriggered: root.moveToViewRequested("today")}
            MenuItem {text: qsTr("指定日期…");onTriggered: root.moveToViewRequested("upcoming")}
        }
        MenuItem {text: root.evening ? qsTr("安排到今天") : qsTr("安排到今晚");visible: root.inToday && !root.isDone;onTriggered: root.eveningRequested(!root.evening)}
        MenuItem {text: qsTr("清除日期安排");visible: root.canActivate;onTriggered: root.activateRequested()}
        MenuSeparator {}
        MenuItem {objectName: "taskMenuDelete-"+root.taskId;text: qsTr("删除任务…");palette.text: Theme.danger;onTriggered: root.deleteRequested()}
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
    Keys.onPressed: function(event){if(!root.deleted && (event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))){menu.popup();event.accepted=true}}
    TapHandler {acceptedButtons: Qt.RightButton;enabled: !root.deleted && root.expanded;onTapped: function(point){menu.popup(root,point.position.x,point.position.y)}}
    MouseArea {
        id: holdArea;objectName: "taskHoldDrag";anchors.fill: parent;enabled: !root.expanded && !root.selectionMode && !root.deleted
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        pressAndHoldInterval: 350;preventStealing: root.dragArmed;cursorShape: root.dragArmed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: function(mouse){root.heldPress=false;if(mouse.button===Qt.RightButton){menu.popup(root,mouse.x,mouse.y);return}root.dragOriginX=mouse.x;root.dragOriginY=mouse.y}
        onPressAndHold: function(mouse){if(mouse.button===Qt.LeftButton && root.canDrag){root.heldPress=true;root.armDrag(mouse.x,mouse.y)}}
        onClicked: function(mouse){if(mouse.button===Qt.LeftButton && !root.heldPress)root.quickEditRequested()}
        onReleased: root.finishDrag(true)
        onCanceled: root.finishDrag(false)
        onPositionChanged: function(mouse){
            if(!(pressedButtons & Qt.LeftButton) || !root.canDrag)return
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
    DropArea {id: dropZone;anchors.fill: parent;enabled: root.canReorder;onDropped: function(drop){const source=drop.source as TaskRow;if(source && source!==root){root.dropRequested(source.taskId);drop.acceptProposedAction()}}}
    padding: 4
    leftPadding: 10+Math.min(8,taskDepth)*20
    rightPadding: 10
    function columnPositions() {
        const completion=expanded ? inlineEditor.completionItem : completionAction
        const titleItem=expanded ? inlineEditor.titleItem : rowTitle
        return {completion:completion.mapToItem(root,0,0).x,title:titleItem.mapToItem(root,0,0).x,depth:taskDepth}
    }
    implicitHeight: Math.max(Theme.compact ? 36 : 40,contentItem.implicitHeight+topPadding+bottomPadding)
    function verifyGeometry() {
        for(const child of metadata.children)if(child.visible && child.y+child.height>metadata.height+1)return "metadata exceeds flow"
        if(!expanded && rowTitle.mapToItem(root,0,0).y+rowTitle.height>root.height-bottomPadding+1)return "title exceeds row"
        if(!expanded && metadata.mapToItem(root,0,0).y+metadata.height>root.height-bottomPadding+1)return "metadata exceeds row"
        return "PASS"
    }
    Accessible.name: title + (isDone ? qsTr("，已结束") : qsTr("，未完成"))
    onClicked: selectionMode ? selectionRequested() : quickEditRequested()
    contentItem: ColumnLayout {
        z: 1
        spacing: 0
        RowLayout {
        Layout.fillWidth: true
        visible: !root.expanded
        spacing: 8
        Item {
            Layout.preferredWidth: 24;Layout.preferredHeight: 32
            WidgetAction {
                anchors.centerIn: parent;visible: !root.smartView && root.visibleChildCount>0
                objectName: "taskChildren-"+root.taskId
                symbol: root.childrenExpanded ? "down" : "right"
                description: (root.childrenExpanded ? qsTr("收起 %1 个子任务") : qsTr("展开 %1 个子任务")).arg(root.visibleChildCount)
                onClicked: root.childrenExpansionRequested(!root.childrenExpanded)
            }
        }
        CheckBox {visible: root.selectionMode;checked: root.selectedForBatch;Accessible.name: qsTr("选择任务：")+root.title;onClicked: root.selectionRequested()}
        IconToolButton {
            id: completionAction
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
                    width: Theme.iconSize;height: width;radius: width/2
                    color: (root.isDone || root.removingAfterCompletion) ? Theme.accent : "transparent"
                    border.color: (root.isDone || root.removingAfterCompletion) ? Theme.accent : Theme.secondary
                    AppIcon {anchors.centerIn: parent;name: "check";width: Math.round(Theme.iconSize*2/3);height: width;visible: root.isDone || root.removingAfterCompletion;ink: Theme.accentText}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Label {id: rowTitle;topPadding: Math.max(0,(Math.max(32,Theme.iconSize+12)-Theme.listTitleSize*1.35)/2);text: root.title; font.pixelSize: Theme.listTitleSize; color: Theme.text; font.strikeout: root.isDone || root.removingAfterCompletion; Layout.fillWidth: true; elide: Text.ElideRight }
            Flow {
                id: metadata
                Layout.fillWidth: true;spacing: 6
                Label {visible: root.taskDepth===0 && !!root.parentTitle;text: (root.parentDeleted ? qsTr("原父任务（已删除）：%1") : qsTr("父任务：%1")).arg(root.parentTitle);color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Label {visible: root.planningState==="SOMEDAY";text: qsTr("已暂停");color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Label {visible: root.pinned;text: qsTr("置顶");color: Theme.accent;font.pixelSize: Theme.captionSize}
                Label {visible: root.important;text: qsTr("特别关注");color: Theme.accent;font.pixelSize: Theme.captionSize}
                Label {visible: root.inToday && root.showTodayMarker;text: qsTr("今天");color: Theme.accent;font.pixelSize: Theme.captionSize}
                Label {visible: !!root.projectName;text: root.projectName;color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Label {visible: !!root.tagSummary;text: root.tagSummary;color: Theme.secondary;font.pixelSize: Theme.captionSize}
                ToolButton {id: childLink;objectName: "taskSubtasksLink-"+root.taskId;visible: root.subtaskPending>0 && !root.deleted;text: qsTr("%1 个子任务").arg(root.subtaskPending);padding: 0;implicitHeight: contentItem.implicitHeight;Accessible.name: qsTr("查看 %1 的子任务").arg(root.title);contentItem: Label {text: childLink.text;font.pixelSize: Theme.captionSize;color: Theme.accent} background: Item {} onClicked: root.subtaskRequested()}
                Label {visible: root.completionText.length>0;text: root.completionText;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                Row {visible: !!root.startOn;spacing: 3;AppIcon {name: "calendar";width: Math.min(24,14*Theme.scale);height: width} Label {text: root.startOn.slice(5);font.pixelSize: Theme.captionSize;color: Theme.secondary}}
                Row {visible: !!root.deadlineOn;spacing: 3;AppIcon {name: "flag";width: Math.min(24,14*Theme.scale);height: width;ink: root.isOverdue ? Theme.danger : Theme.secondary} Label {text: root.deadlineOn.slice(5);font.pixelSize: Theme.captionSize;color: root.isOverdue ? Theme.danger : Theme.secondary}}
                Label {visible: root.taskStatus==="CANCELLED";text: qsTr("已取消");color: Theme.secondary;font.pixelSize: Theme.captionSize}
            }
        }
        Item {
            opacity: root.canDrag && (root.hovered || root.activeFocus || root.dragArmed) ? 1 : 0
            Accessible.ignored: !root.canDrag
            Layout.preferredWidth: 28;Layout.preferredHeight: 32
            Accessible.name: qsTr("拖动排序或拖到导航栏移动任务")
            AppIcon {anchors.centerIn: parent;name: "grip"}
            MouseArea {
                id: handleArea;objectName: "taskDragHandle";anchors.fill: parent;enabled: root.canDrag;cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                preventStealing: true
                onPressed: function(mouse){const point=mapToItem(root,mouse.x,mouse.y);root.armDrag(point.x,point.y)}
                onPositionChanged: function(mouse){if(pressed && root.dragArmed){const point=mapToItem(root,mouse.x,mouse.y);root.updateDrag(point.x,point.y)}}
                onReleased: root.finishDrag(true)
                onCanceled: root.finishDrag(false)
            }
        }
        IconToolButton {
            objectName: "taskDetails-"+root.taskId
            contentItem: AppIcon {name: root.deleted ? "undo" : "info"}
            Accessible.name: (root.deleted ? qsTr("恢复任务：") : qsTr("打开任务详情："))+root.title
            ToolTip.visible: hovered;ToolTip.text: root.deleted ? qsTr("恢复任务") : qsTr("详情")
            onClicked: {if(root.deleted)root.restoreRequested();else root.editRequested()}

        }
        }
        Item {id: inlineHost;Layout.fillWidth: true;Layout.leftMargin: 32;visible: root.expanded;implicitHeight: root.inlineEditor ? root.inlineEditor.implicitHeight : 0}
    }
    background: Rectangle {
        opacity: root.dragging ? 0.5 : 1
        radius: Theme.rowRadius
        color: root.expanded ? Theme.grouped : root.highlighted ? Theme.selected : root.hovered ? Theme.hover : "transparent"
        border.width: root.visualFocus || root.dragArmed ? 1 : 0;border.color: Theme.accent
        Item {visible: root.taskDepth>0;x: root.leftPadding-10;width: 10;height: root.expanded ? root.topPadding+root.inlineEditor.topPadding+16 : parent.height/2
            Rectangle {width: 1;height: parent.height;color: Theme.separator}
            Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;color: Theme.separator}
        }
        Rectangle {visible: dropZone.containsDrag && !root.dragging;height: 2;width: parent.width;anchors.top: parent.top;color: Theme.accent}
        Rectangle {objectName: "taskRowSeparator";visible: root.showSeparator && !root.expanded && !root.highlighted && !root.hovered && !root.visualFocus;height: 1;x: root.leftPadding+32+completionAction.width+8;width: Math.max(0,parent.width-x-root.rightPadding);anchors.bottom: parent.bottom;color: Theme.separator}
    }
}
