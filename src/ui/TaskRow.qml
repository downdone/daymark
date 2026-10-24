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
    required property bool isDone
    required property bool isOverdue
    required property real rowVersion
    required property int checklistPending
    required property bool evening
    required property bool inToday
    property bool showTodayMarker: true
    property bool selectionMode: false
    property bool selectedForBatch: false
    property bool canMoveUp: true
    property bool canMoveDown: true
    property bool canReorder: false
    property bool heldPress: false
    property bool dragArmed: false
    property bool dragging: false
    property real dragOriginX: 0
    property real dragOriginY: 0
    property bool canActivate: false
    property bool expanded: false
    property Item inlineEditor: null
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
    function armDrag(x,y){dragOriginX=x;dragOriginY=y;dragArmed=true;dragProxy.x=0;dragProxy.y=0;forceActiveFocus()}
    function updateDrag(x,y){if(!dragArmed)return;const dx=x-dragOriginX,dy=y-dragOriginY;if(!dragging && Math.abs(dx)+Math.abs(dy)>2)dragging=true;if(dragging){dragProxy.x=dx;dragProxy.y=dy}}
    function finishDrag(drop){if(dragging){if(drop)dragProxy.Drag.drop();else dragProxy.Drag.cancel()}dragging=false;dragArmed=false;dragProxy.x=0;dragProxy.y=0}
    function dragPointIn(item){return dragProxy.mapToItem(item,dragOriginX,dragOriginY)}
    function compensateScroll(delta){dragProxy.y+=delta}
    onDraggingChanged: dragStateChanged(dragging)
    z: dragging ? 100 : 0
    Keys.onEscapePressed: function(event){if(dragging || dragArmed)finishDrag(false);else event.accepted=false}
    MouseArea {
        id: holdArea;objectName: "taskHoldDrag";anchors.fill: parent;enabled: root.canReorder && !root.expanded && !root.selectionMode
        pressAndHoldInterval: 350;preventStealing: root.dragArmed;cursorShape: root.dragArmed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: root.heldPress=false
        onPressAndHold: function(mouse){root.heldPress=true;root.armDrag(mouse.x,mouse.y)}
        onClicked: if(!root.heldPress)root.quickEditRequested()
        onReleased: root.finishDrag(true)
        onCanceled: root.finishDrag(false)
        onPositionChanged: function(mouse){if(pressed && root.dragArmed){const point=mapToItem(root,mouse.x,mouse.y);root.updateDrag(point.x,point.y)}}
    }
    Item {
        id: dragProxy;z: 20;width: root.width;height: root.height
        Drag.active: root.dragging;Drag.source: root;Drag.hotSpot.x: root.dragOriginX;Drag.hotSpot.y: root.dragOriginY
        Rectangle {anchors.fill: parent;visible: root.dragging;color: Theme.content;radius: 12;border.color: Theme.accent;opacity: 0.95
            Text {anchors.fill: parent;anchors.margins: 12;text: root.title;font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize;color: Theme.text;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
        }
    }
    DropArea {id: dropZone;anchors.fill: parent;enabled: root.canReorder;onDropped: function(drop){const source=drop.source as TaskRow;if(source && source!==root){root.dropRequested(source.taskId);drop.acceptProposedAction()}}}
    padding: expanded ? 0 : 6
    horizontalPadding: expanded ? 0 : 10
    implicitHeight: Math.max(Theme.compact ? 38 : 48,contentItem.implicitHeight+topPadding+bottomPadding)
    Accessible.name: title + (isDone ? qsTr("，已结束") : qsTr("，未完成"))
    onClicked: selectionMode ? selectionRequested() : quickEditRequested()
    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
        Layout.fillWidth: true
        visible: !root.expanded
        spacing: 8
        CheckBox {visible: root.selectionMode;checked: root.selectedForBatch;Accessible.name: qsTr("选择任务：")+root.title;onClicked: root.selectionRequested()}
        IconToolButton {
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
            spacing: 2
            Label {topPadding: Math.max(0,(Math.max(32,Theme.iconSize+12)-Theme.bodySize*1.35)/2);text: root.title; font.pixelSize: Theme.bodySize; color: Theme.text; font.strikeout: root.isDone || root.removingAfterCompletion; Layout.fillWidth: true; elide: Text.ElideRight }
            Flow {
                Layout.fillWidth: true;spacing: 8
                Label {visible: root.inToday && root.showTodayMarker;text: qsTr("今天");color: Theme.accent;font.pixelSize: Theme.captionSize}
                Label {visible: !!root.projectName;text: root.projectName;color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Label {visible: !!root.tagSummary;text: root.tagSummary;color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Row {visible: root.checklistPending>0;spacing: 3;AppIcon {name: "list";width: Math.min(24,14*Theme.scale);height: width} Label {text: qsTr("%1步").arg(root.checklistPending);font.pixelSize: Theme.captionSize;color: Theme.secondary}}
                Row {visible: !!root.startOn;spacing: 3;AppIcon {name: "calendar";width: Math.min(24,14*Theme.scale);height: width} Label {text: root.startOn.slice(5);font.pixelSize: Theme.captionSize;color: Theme.secondary}}
                Row {visible: !!root.deadlineOn;spacing: 3;AppIcon {name: "flag";width: Math.min(24,14*Theme.scale);height: width;ink: root.isOverdue ? Theme.danger : Theme.secondary} Label {text: root.deadlineOn.slice(5);font.pixelSize: Theme.captionSize;color: root.isOverdue ? Theme.danger : Theme.secondary}}
                Label {visible: root.taskStatus==="CANCELLED";text: qsTr("已取消");color: Theme.secondary;font.pixelSize: Theme.captionSize}
            }
        }
        Item {
            opacity: root.canReorder && (root.hovered || root.activeFocus || root.dragArmed) ? 1 : 0
            Accessible.ignored: !root.canReorder
            Layout.preferredWidth: 28;Layout.preferredHeight: 32
            Accessible.name: qsTr("拖动排序；也可长按任务，Ctrl加上下箭头移动")
            AppIcon {anchors.centerIn: parent;name: "grip"}
            MouseArea {
                id: handleArea;anchors.fill: parent;enabled: root.canReorder;cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                preventStealing: true
                onPressed: function(mouse){const point=mapToItem(root,mouse.x,mouse.y);root.armDrag(point.x,point.y)}
                onPositionChanged: function(mouse){if(pressed && root.dragArmed){const point=mapToItem(root,mouse.x,mouse.y);root.updateDrag(point.x,point.y)}}
                onReleased: root.finishDrag(true)
                onCanceled: root.finishDrag(false)
            }
        }
        IconToolButton {
            contentItem: AppIcon {name: "more"}
            Accessible.name: qsTr("任务操作：")+root.title
            onClicked: menu.open()
            Menu {
                id: menu
                onAboutToShow: root.menuRequested()
                x: parent.width-width
                y: parent.height
                MenuItem { text: qsTr("编辑详情…"); onTriggered: root.editRequested() }
                MenuItem {text: root.evening ? qsTr("安排到今天") : qsTr("安排到今晚");visible: root.inToday && !root.isDone;onTriggered: root.eveningRequested(!root.evening)}
                MenuItem {text: qsTr("激活到随时可做");visible: root.canActivate;onTriggered: root.activateRequested()}

            }
        }
        }
        Item {id: inlineHost;Layout.fillWidth: true;visible: root.expanded;implicitHeight: root.inlineEditor ? root.inlineEditor.implicitHeight : 0}
    }
    background: Rectangle {
        opacity: root.dragging ? 0.5 : 1
        radius: root.expanded ? 14 : Theme.rowRadius
        color: root.expanded ? Theme.grouped : root.highlighted ? Theme.selected : root.hovered ? Theme.hover : "transparent"
        border.width: root.activeFocus || root.dragArmed ? 1 : 0;border.color: Theme.accent
        Rectangle {visible: dropZone.containsDrag && !root.dragging;height: 2;width: parent.width;anchors.top: parent.top;color: Theme.accent}
        Rectangle {visible: !root.expanded && !root.highlighted && !root.hovered;height: 1;x: 50;width: Math.max(0,parent.width-60);anchors.bottom: parent.bottom;color: Theme.separator}
    }
}
