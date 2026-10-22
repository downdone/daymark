pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: widget
    required property WidgetViewModel controller
    required property TaskWorkspace workspace
    objectName: "widgetWindow"
    width: 340
    height: 360
    minimumWidth: 260
    minimumHeight: widget.controller.mini ? 48 : 190
    x: 80
    y: 80
    visible: false
    color: "transparent"
    title: qsTr("Daymark 今日任务")

    Control {
        anchors.fill: parent
        font.pixelSize: 14*Theme.scale
        palette.window: Theme.content
        palette.windowText: Theme.text
        palette.text: Theme.text
        palette.button: Theme.sidebar
        palette.buttonText: Theme.text
        palette.highlight: Theme.accent
        contentItem: Rectangle {
        radius: 14
        color: Qt.rgba(Theme.content.r, Theme.content.g, Theme.content.b, widget.controller.backgroundOpacity)
        border.color: Theme.separator

        ColumnLayout {
            visible: !widget.controller.mini
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 36
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Daymark"
                        font.bold: true
                        color: Theme.text
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.SizeAllCursor
                        onPressed: widget.controller.startMove()
                    }
                }
                Button { text: qsTr("迷你"); onClicked: widget.controller.setMini(true) }
                Button { text: qsTr("隐藏"); onClicked: widget.controller.setVisible(false) }
            }
            Label {
                visible: widget.workspace.todayTasks.count === 0
                text: widget.workspace.ready ? qsTr("今天暂无任务\n在主窗口的“今天”页面添加任务。") : qsTr("正在加载本地任务…")
                color: Theme.secondary
                lineHeight: 1.4
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: widget.controller.errorText.length > 0 || widget.workspace.errorText.length > 0
                text: widget.workspace.errorText || widget.controller.errorText
                font.pixelSize: 11*Theme.scale
                color: Theme.danger
            }
            ListView {
                id: todayList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: widget.workspace.todayTasks
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property string taskId
                    required property string title
                    required property real rowVersion
                    id: todayRow
                    width: todayList.width
                    Button {
                        text: "○"
                        Accessible.name: qsTr("完成任务")
                        enabled: widget.workspace.ready && !widget.workspace.busy
                        onClicked: widget.workspace.changeState(todayRow.taskId, todayRow.rowVersion, "complete")
                    }
                    Label { Layout.fillWidth: true; text: todayRow.title; elide: Text.ElideRight; color: Theme.text }
                }
            }
        }

        RowLayout {
            visible: widget.controller.mini
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Daymark · 今日 %1 项").arg(widget.workspace.todayTasks.count)
                    font.pixelSize: 12*Theme.scale
                    color: Theme.text
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    onPressed: widget.controller.startMove()
                }
            }
            Button { text: qsTr("展开"); onClicked: widget.controller.setMini(false) }
        }
        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 16
            height: 16
            visible: !widget.controller.mini && !widget.controller.locked
            cursorShape: Qt.SizeFDiagCursor
            onPressed: widget.controller.startResize()
            Accessible.name: qsTr("调整组件大小")
        }
        }
    }
}
