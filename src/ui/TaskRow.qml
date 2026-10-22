pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
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
    property bool selectionMode: false
    property bool selectedForBatch: false
    signal selectionRequested()
    signal completeRequested()
    signal editRequested()
    signal moveRequested(int direction)
    signal dropRequested(string source)
    signal activateRequested()
    Drag.active: drag.active
    Drag.source: root
    Drag.hotSpot.x: width/2+drag.centroid.position.x-drag.centroid.pressPosition.x
    Drag.hotSpot.y: height/2+drag.centroid.position.y-drag.centroid.pressPosition.y
    DragHandler {id: drag;target: null;onActiveChanged: if(!active)root.Drag.drop()}
    DropArea {anchors.fill: parent;onDropped: function(drop){const source=drop.source as TaskRow;if(source){root.dropRequested(source.taskId);drop.acceptProposedAction()}}}
    implicitHeight: Math.max(Theme.compact ? 40 : 48,contentItem.implicitHeight+(Theme.compact ? 8 : 16))
    Accessible.name: title + (isDone ? qsTr("，已结束") : qsTr("，未完成"))
    onClicked: selectionMode ? selectionRequested() : editRequested()
    contentItem: RowLayout {
        spacing: 12
        CheckBox {visible: root.selectionMode;checked: root.selectedForBatch;Accessible.name: qsTr("选择任务：")+root.title;onClicked: root.selectionRequested()}
        ToolButton {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32
            Accessible.name: (root.isDone ? qsTr("恢复任务：") : qsTr("完成任务："))+root.title
            onClicked: root.completeRequested()
            contentItem: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                radius: 5
                color: root.isDone ? Theme.accent : "transparent"
                border.color: Theme.secondary
                Label { anchors.centerIn: parent; text: "✓"; visible: root.isDone; color: Theme.dark ? "#191a1d" : "white" }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Label { text: root.title; font.pixelSize: 15*Theme.scale; color: Theme.text; font.strikeout: root.isDone; Layout.fillWidth: true; elide: Text.ElideRight }
            Label {visible: root.inToday;text: qsTr("今日");color: Theme.accent;font.pixelSize: 11*Theme.scale}
            Label {
                text: [root.projectName,root.tagSummary,root.checklistPending>0 ? qsTr("清单剩余 %1").arg(root.checklistPending) : "",root.startOn ? qsTr("开始 %1").arg(root.startOn) : "",root.deadlineOn ? qsTr("⚑ 截止 %1").arg(root.deadlineOn) : "",root.taskStatus === "CANCELLED" ? qsTr("已取消") : ""].filter(function(x){return x.length>0}).join(" · ")
                visible: text.length>0
                color: root.isOverdue ? Theme.danger : Theme.secondary
                font.pixelSize: 12*Theme.scale
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
        ToolButton {
            text: "⋯"
            Accessible.name: qsTr("任务操作：")+root.title
            onClicked: menu.open()
            Menu {
                id: menu
                MenuItem { text: qsTr("编辑详情…"); onTriggered: root.editRequested() }
                MenuItem {text: qsTr("激活到随时可做");enabled: !root.isDone;onTriggered: root.activateRequested()}
                MenuItem { text: qsTr("上移"); onTriggered: root.moveRequested(-1) }
                MenuItem { text: qsTr("下移"); onTriggered: root.moveRequested(1) }
            }
        }
    }
    background: Rectangle { radius: 6; color: root.highlighted ? Theme.selected : root.hovered ? Theme.hover : "transparent"; border.width: root.activeFocus ? 2 : 0; border.color: Theme.accent }
}
