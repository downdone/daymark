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
    Drag.active: drag.active
    Drag.source: root
    Drag.hotSpot.x: width/2+drag.centroid.position.x-drag.centroid.pressPosition.x
    Drag.hotSpot.y: height/2+drag.centroid.position.y-drag.centroid.pressPosition.y
    DragHandler {id: drag;target: null;onActiveChanged: {if(!active)root.Drag.drop();root.dragStateChanged(active)}}
    DropArea {anchors.fill: parent;onDropped: function(drop){const source=drop.source as TaskRow;if(source){root.dropRequested(source.taskId);drop.acceptProposedAction()}}}
    implicitHeight: Math.max(Theme.compact ? 38 : 44,contentItem.implicitHeight+(Theme.compact ? 6 : 10))
    Accessible.name: title + (isDone ? qsTr("，已结束") : qsTr("，未完成"))
    onClicked: selectionMode ? selectionRequested() : quickEditRequested()
    contentItem: RowLayout {
        spacing: 8
        CheckBox {visible: root.selectionMode;checked: root.selectedForBatch;Accessible.name: qsTr("选择任务：")+root.title;onClicked: root.selectionRequested()}
        IconToolButton {
            Layout.preferredWidth: Math.max(32,Theme.iconSize+12)
            Layout.preferredHeight: Math.max(32,Theme.iconSize+12)
            Accessible.name: (root.isDone ? qsTr("恢复任务：") : qsTr("完成任务："))+root.title
            onClicked: root.completeRequested()
            contentItem: Item {
                implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize
                Rectangle {
                    anchors.centerIn: parent
                    width: Theme.iconSize;height: width;radius: width/2
                    color: root.isDone ? Theme.accent : "transparent"
                    border.color: root.isDone ? Theme.accent : Theme.secondary
                    AppIcon {anchors.centerIn: parent;name: "check";width: Math.round(Theme.iconSize*2/3);height: width;visible: root.isDone;ink: Theme.accentText}
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            Label { text: root.title; font.pixelSize: Theme.bodySize; color: Theme.text; font.strikeout: root.isDone; Layout.fillWidth: true; elide: Text.ElideRight }
            Label {
                text: [root.inToday && root.showTodayMarker ? qsTr("今日") : "",root.projectName,root.tagSummary,root.checklistPending>0 ? qsTr("清单剩余 %1").arg(root.checklistPending) : "",root.startOn ? qsTr("开始 %1").arg(root.startOn) : "",root.deadlineOn ? qsTr("截止 %1").arg(root.deadlineOn) : "",root.taskStatus === "CANCELLED" ? qsTr("已取消") : ""].filter(function(x){return x.length>0}).join(" · ")
                visible: text.length>0
                color: root.isOverdue ? Theme.danger : Theme.secondary
                font.pixelSize: Theme.captionSize
                Layout.fillWidth: true
                elide: Text.ElideRight
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
                MenuItem {text: qsTr("激活到随时可做");enabled: !root.isDone;onTriggered: root.activateRequested()}
                MenuItem { text: qsTr("上移");enabled: root.canMoveUp; onTriggered: root.moveRequested(-1) }
                MenuItem { text: qsTr("下移");enabled: root.canMoveDown; onTriggered: root.moveRequested(1) }
            }
        }
    }
    background: Rectangle { radius: Theme.rowRadius; color: root.highlighted ? Theme.selected : root.hovered ? Theme.hover : "transparent"; border.width: root.activeFocus ? 2 : 0; border.color: Theme.accent }
}
