pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
Control {
    id: bar
    default property alias actions: buttons.data
    property real gap: Theme.gap
    padding: Theme.panelPadding
    topPadding: Theme.fieldGap
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {color: "transparent";Rectangle {width: parent.width;height: 1;color: Theme.separator}}
    contentItem: Item {
        implicitHeight: buttons.implicitHeight
        Flow {
            id: buttons
            anchors.right: parent.right
            width: Math.min(naturalWidth,bar.availableWidth)
            spacing: bar.gap
            readonly property real naturalWidth: {let size=0;let count=0;for(const item of children)if(item.visible){size+=item.width;count++}return size+Math.max(0,count-1)*spacing}
        }
    }
}
