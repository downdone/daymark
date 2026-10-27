pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

// Keep move/resize in the window system, including platform snapping and limits.
Item {
    id: root
    required property var targetWindow
    readonly property int edgeWidth: 6
    readonly property int cornerWidth: 16
    Repeater {
        model: [
            {edges: Qt.LeftEdge, cursor: Qt.SizeHorCursor},
            {edges: Qt.RightEdge, cursor: Qt.SizeHorCursor},
            {edges: Qt.TopEdge, cursor: Qt.SizeVerCursor},
            {edges: Qt.BottomEdge, cursor: Qt.SizeVerCursor},
            {edges: Qt.LeftEdge | Qt.TopEdge, cursor: Qt.SizeFDiagCursor},
            {edges: Qt.RightEdge | Qt.TopEdge, cursor: Qt.SizeBDiagCursor},
            {edges: Qt.LeftEdge | Qt.BottomEdge, cursor: Qt.SizeBDiagCursor},
            {edges: Qt.RightEdge | Qt.BottomEdge, cursor: Qt.SizeFDiagCursor}
        ]
        delegate: MouseArea {
            required property var modelData
            readonly property int edges: modelData.edges
            readonly property bool horizontalEdge: edges === Qt.TopEdge || edges === Qt.BottomEdge
            readonly property bool verticalEdge: edges === Qt.LeftEdge || edges === Qt.RightEdge
            objectName: "calendarResizeEdge-" + edges
            x: horizontalEdge ? root.cornerWidth : edges & Qt.RightEdge ? root.width - width : 0
            y: verticalEdge ? root.cornerWidth : edges & Qt.BottomEdge ? root.height - height : 0
            width: horizontalEdge ? root.width - 2 * root.cornerWidth : verticalEdge ? root.edgeWidth : root.cornerWidth
            height: verticalEdge ? root.height - 2 * root.cornerWidth : horizontalEdge ? root.edgeWidth : root.cornerWidth
            cursorShape: modelData.cursor
            onPressed: root.targetWindow.startSystemResize(edges)
        }
    }
    ToolButton {
        id: grip
        objectName: "calendarResizeGrip"
        anchors.right: parent.right;anchors.bottom: parent.bottom
        width: root.cornerWidth;height: root.cornerWidth
        padding: 0
        focusPolicy: Qt.TabFocus
        hoverEnabled: true
        HoverHandler {cursorShape: Qt.SizeFDiagCursor}
        Accessible.name: qsTr("调整日历大小")
        Accessible.description: qsTr("拖动调整；方向键微调，Shift加快")
        background: Rectangle {color: "transparent";radius: Theme.controlRadius;border.width: grip.visualFocus ? 1 : 0;border.color: Theme.accent}
        contentItem: Item {
            Repeater {
                model: [5,9]
                Rectangle {
                    required property int modelData
                    width: modelData;height: 1
                    x: 12-width;y: 12-width/2
                    rotation: -45
                    color: grip.hovered || grip.visualFocus ? Theme.secondary : Theme.separator
                }
            }
        }
        onPressed: root.targetWindow.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        Keys.onPressed: function(event) {
            const delta=event.modifiers & Qt.ShiftModifier ? 64 : 16;
            if(event.key===Qt.Key_Left || event.key===Qt.Key_Right)
                root.targetWindow.width=Math.max(root.targetWindow.minimumWidth,root.targetWindow.width+(event.key===Qt.Key_Right ? delta : -delta));
            else if(event.key===Qt.Key_Up || event.key===Qt.Key_Down)
                root.targetWindow.height=Math.max(root.targetWindow.minimumHeight,root.targetWindow.height+(event.key===Qt.Key_Down ? delta : -delta));
            else return;
            event.accepted=true;
        }
    }
}
