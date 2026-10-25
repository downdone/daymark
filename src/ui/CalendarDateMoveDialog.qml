pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property TaskWorkspace workspace
    signal moveRequested(string taskId,real version,string kind,string date)
    property var entry: ({})
    function offer(value, date) {
        entry = value;
        targetDate.text = date;
        markerKind.currentIndex = value.kind === "deadline" ? 1 : 0;
        open();
    }
    objectName: "calendarMoveDialog"
    parent: Overlay.overlay
    x: (parent.width - width) / 2
    y: (parent.height - height) / 2
    title: qsTr("调整任务日期")
    width: Math.min(440, parent.width - 32)
    modal: true
    closePolicy: Popup.NoAutoClose
    ColumnLayout {
        anchors.fill: parent
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: root.entry.title || ""
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: Theme.captionSize
            color: Theme.secondary
            text: qsTr("只修改所选日期，另一日期保持原值。")
        }
        ComboBox {
            id: markerKind
            Layout.fillWidth: true
            model: [qsTr("开始日期"), qsTr("结束日期")]
            enabled: root.entry.kind === "both"
        }
        DateField {
            id: targetDate
            objectName: "calendarMoveDate"
            workspace: root.workspace
            dateLabel: markerKind.currentIndex === 0 ? qsTr("开始日期") : qsTr("结束日期")
            Layout.fillWidth: true
        }
        Label {
            text: root.workspace.errorText
            color: Theme.danger
            Layout.maximumWidth: 300
            wrapMode: Text.Wrap
        }
    }
    footer: DialogButtonBox {
        Button {
            text: qsTr("取消")
            enabled: !root.workspace.busy
            onClicked: root.close()
        }
        Button {
            objectName: "confirmCalendarMove"
            text: qsTr("确认改期")
            highlighted: true
            enabled: !root.workspace.busy
            onClicked: root.moveRequested(root.entry.taskId, root.entry.rowVersion, markerKind.currentIndex === 0 ? "start" : "deadline", targetDate.text)
        }
    }
}
