pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: root
    required property int itemIndex
    required property string title
    required property bool done
    property int itemCount: 0
    property bool dragging: false
    property real originY: 0
    signal titleEdited(string value)
    signal doneEdited(bool value)
    signal removeRequested()
    signal moveRequested(int from,int to)
    Layout.fillWidth: true
    implicitHeight: contentItem.implicitHeight
    z: dragging ? 30 : 0
    function dragPointIn(item){return proxy.mapToItem(item,width/2,originY)}
    function compensateScroll(delta){proxy.y+=delta}
    function cancelDrag(){proxy.Drag.cancel();dragging=false;proxy.y=0}
    Keys.onEscapePressed: function(event){if(dragging)cancelDrag();else event.accepted=false}
    Keys.onPressed: function(event){
        if(event.modifiers & Qt.ControlModifier){
            if(event.key===Qt.Key_Up && itemIndex>0){moveRequested(itemIndex,itemIndex-1);event.accepted=true}
            else if(event.key===Qt.Key_Down && itemIndex<itemCount-1){moveRequested(itemIndex,itemIndex+1);event.accepted=true}
        }
    }
    contentItem: RowLayout {
        spacing: 4
        opacity: root.dragging ? 0.45 : 1
        CheckBox {checked: root.done;Accessible.name: qsTr("完成清单项：")+root.title;onClicked: root.doneEdited(checked)}
        TextField {Layout.fillWidth: true;text: root.title;Accessible.name: qsTr("清单项标题，Ctrl 加上下键排序");onTextEdited: root.titleEdited(text);Keys.forwardTo: [root]}
        Item {
            objectName: "checklistDragHandle"
            implicitWidth: 32;implicitHeight: 32
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("拖动排序：")+root.title
            Accessible.description: qsTr("按住拖动，或用 Ctrl 加上下键排序")
            activeFocusOnTab: true
            Keys.forwardTo: [root]
            AppIcon {anchors.centerIn: parent;width: 16;height: 16;name: "grip";ink: Theme.secondary}
            MouseArea {
                anchors.fill: parent;preventStealing: true;cursorShape: root.dragging ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                onPressed: function(mouse){root.originY=mapToItem(root,mouse.x,mouse.y).y;root.forceActiveFocus();root.dragging=true}
                onPositionChanged: function(mouse){if(pressed && root.dragging)proxy.y=mapToItem(root,mouse.x,mouse.y).y-root.originY}
                onReleased: {if(root.dragging)proxy.Drag.drop();root.cancelDrag()}
                onCanceled: root.cancelDrag()
            }
        }
        IconToolButton {contentItem: AppIcon {name: "trash";ink: Theme.danger} Accessible.name: qsTr("删除清单项：")+root.title;onClicked: root.removeRequested()}
    }
    DropArea {
        id: dropArea;anchors.fill: parent
        onDropped: function(drop){const source=drop.source as ChecklistRow;if(source && source!==root){root.moveRequested(source.itemIndex,root.itemIndex);drop.acceptProposedAction()}}
        Rectangle {anchors.top: parent.top;width: parent.width;height: 2;visible: dropArea.containsDrag && dropArea.drag.source!==root;color: Theme.accent}
    }
    Item {
        id: proxy;width: root.width;height: root.height
        Drag.active: root.dragging;Drag.source: root;Drag.hotSpot.x: width/2;Drag.hotSpot.y: root.originY
        Rectangle {anchors.fill: parent;visible: root.dragging;color: Theme.content;radius: Theme.rowRadius;border.color: Theme.accent
            Label {anchors.fill: parent;anchors.margins: 8;text: root.title;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}
        }
    }
}
