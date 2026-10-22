pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
    property string symbol: ""
    property color symbolColor: Theme.accent
    property bool selected: false
    property int count: -1
    property bool acceptsTaskDrop: false
    signal taskDropped(string taskId,real version)
    DropArea {anchors.fill: parent;enabled: root.acceptsTaskDrop;onDropped: function(drop){const source=drop.source as TaskRow;if(source){root.taskDropped(source.taskId,source.rowVersion);drop.acceptProposedAction()}}}
    implicitHeight: Math.max(36,contentItem.implicitHeight+12)
    highlighted: selected
    Accessible.name: text
    contentItem: RowLayout {
        spacing: 10
        Label { text: root.symbol; color: root.symbolColor; font.pixelSize: 17*Theme.scale; Layout.preferredWidth: 20*Theme.scale }
        Label { text: root.text; color: Theme.text; font.pixelSize: 14*Theme.scale; Layout.fillWidth: true; elide: Text.ElideRight }
        Label { visible: root.count>=0; text: root.count; color: Theme.secondary; font.pixelSize: 12*Theme.scale }
    }
    background: Rectangle { radius: 6; color: root.selected ? Theme.selected : root.hovered ? Theme.hover : "transparent"; border.width: root.activeFocus ? 2 : 0; border.color: Theme.accent }
}
