pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
    property string symbol: ""
    property color symbolColor: selected ? Theme.accent : Theme.secondary
    property bool selected: false
    property int count: -1
    property bool acceptsTaskDrop: false
    signal taskDropped(string taskId,real version)
    DropArea {anchors.fill: parent;enabled: root.acceptsTaskDrop;onDropped: function(drop){const source=drop.source as TaskRow;if(source){root.taskDropped(source.taskId,source.rowVersion);drop.acceptProposedAction()}}}
    implicitHeight: Math.max(34,contentItem.implicitHeight+10)
    highlighted: selected
    Accessible.name: text
    contentItem: RowLayout {
        spacing: 10
        AppIcon {name: root.symbol;ink: root.symbolColor;Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize}
        Label { font.weight: root.selected ? Font.DemiBold : Font.Normal;text: root.text; color: Theme.text; font.pixelSize: Theme.bodySize; Layout.fillWidth: true; elide: Text.ElideRight }
        Label { visible: root.count>=0; text: root.count; color: Theme.secondary; font.pixelSize: Theme.captionSize }
    }
    background: Rectangle { radius: Theme.rowRadius; color: root.selected ? Theme.selected : root.hovered ? Theme.hover : "transparent"; border.width: root.activeFocus ? 2 : 0; border.color: Theme.accent }
}
