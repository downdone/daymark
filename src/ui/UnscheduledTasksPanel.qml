pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS

// Button-opened nonmodal panel: no hidden Drawer edge grab over the workspace.
Popup {
    id: root
    objectName: "unscheduledDrawer"
    required property TaskWorkspace workspace
    property var tasks: []
    property real top: 0
    property real workspaceWidth: 640
    signal openTask(string taskId)
    signal dragStarted(var entry, Item source, point origin)
    signal dragMoved(Item source, point position)
    signal dragEnded(bool commit)
    x: parent.width - width
    y: top
    height: Math.max(120, parent.height - y)
    width: Math.min(280 * Theme.scale, workspaceWidth * 0.6)
    modal: false
    dim: false
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: Theme.content
        border.color: Theme.separator
    }
    contentItem: ColumnLayout {
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("未安排任务")
                font.weight: Font.DemiBold
                Layout.fillWidth: true
            }
            WidgetAction {
                symbol: "close"
                description: qsTr("收起未安排任务")
                onClicked: root.close()
            }
        }
        Label {
            text: qsTr("拖到日期或时段，或打开详情安排。")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: Theme.captionSize
            color: Theme.secondary
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            cacheBuffer: 0
            clip: true
            model: root.tasks
            DS.ScrollPhysics {
                target: list
            }
            delegate: ItemDelegate {
                id: row
                required property var modelData
                objectName: "unscheduledTask-" + modelData.taskId
                width: ListView.view.width
                text: modelData.title
                TaskDragGesture {
                    entry: row.modelData
                    dragDistance: root.workspace.dragDistance
                    onActivated: {
                        root.close();
                        root.openTask(row.modelData.taskId);
                    }
                    onDragStarted: function (entry, source, origin) {
                        root.dragStarted(entry, source, origin);
                    }
                    onDragMoved: function (source, position) {
                        root.dragMoved(source, position);
                    }
                    onDragEnded: function (commit) {
                        root.dragEnded(commit);
                    }
                }
            }
            Label {
                anchors.centerIn: parent
                visible: root.tasks.length === 0
                text: qsTr("没有未安排的任务")
                wrapMode: Text.Wrap
                width: parent.width
            }
        }
    }
}
