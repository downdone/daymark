pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    property var selectedDay: ({date: "", entries: []})
    property var moving: ({})
    property var draggedEntry: null
    property bool draggingMarker: false
    function offerMove(entry, date) {
        moving=entry
        targetDate.text=date
        markerKind.currentIndex=entry.kind === "deadline" ? 1 : 0
        moveDialog.open()
    }
    Connections {
        target: panel.workspace
        function onStateChanged() {
            if (!panel.selectedDay.date) return
            for (const day of panel.workspace.calendarDays)
                if (day.date === panel.selectedDay.date) { panel.selectedDay=day; return }
            panel.selectedDay=({date: "",entries: []})
        }
        function onOperationFinished(operation, success) {
            if (operation === "calendarCreate" && success) newTitle.clear()
            if (operation === "calendarMove" && success) moveDialog.close()
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button { text: qsTr("上一页"); onClicked: panel.workspace.stepCalendar(-1) }
        Label { text: panel.workspace.calendarTitle; font.pixelSize: 20*Theme.scale; font.bold: true; Layout.fillWidth: true }
        Button { text: qsTr("今天"); onClicked: panel.workspace.stepCalendar(0) }
        Button { text: qsTr("下一页"); onClicked: panel.workspace.stepCalendar(1) }
        Button { text: panel.workspace.view === "week" ? qsTr("月") : qsTr("周"); onClicked: panel.workspace.setView(panel.workspace.view === "week" ? "month" : "week") }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 8
        CheckBox { text: qsTr("周日作为每周首日"); checked: panel.workspace.sundayFirst; onClicked: panel.workspace.sundayFirst=checked }
        CheckBox {text: qsTr("显示已完成");checked: panel.workspace.calendarCompleted;onClicked: panel.workspace.calendarCompleted=checked}
        Button {text: qsTr("未排期（%1）").arg(panel.workspace.unscheduledTasks.length);onClicked: unscheduled.open()}
    }
    RowLayout {
        Layout.preferredWidth: dayGrid.width
        Layout.fillWidth: false
        Repeater {
            model: panel.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
            Label { required property string modelData; text: modelData; Layout.fillWidth: true; Layout.preferredWidth: 1; horizontalAlignment: Text.AlignHCenter }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        GridLayout {
            id: dayGrid
            columns: 7
            rowSpacing: 3
            columnSpacing: 3
            Layout.fillWidth: true
            Layout.fillHeight: true
            Repeater {
                model: panel.workspace.calendarDays
                Rectangle {
                    id: dayCell
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 44
                    Layout.minimumHeight: panel.workspace.view === "week" ? 280 : 58
                    color: modelData.today ? Theme.selected : modelData.weekend ? Theme.sidebar : Theme.content
                    border.color: panel.selectedDay.date === modelData.date ? Theme.accent : Theme.separator
                    radius: 5
                    Accessible.name: modelData.date
                    HoverHandler {
                        id: dayHover
                        onHoveredChanged: if (hovered && panel.draggingMarker) panel.selectedDay=dayCell.modelData
                    }
                    DropArea {
                        anchors.fill: parent
                        onDropped: function(drop) {
                            if(panel.draggedEntry) {panel.offerMove(panel.draggedEntry,dayCell.modelData.date);drop.acceptProposedAction()}
                        }
                    }
                    TapHandler { onTapped: panel.selectedDay=dayCell.modelData; onDoubleTapped: {panel.selectedDay=dayCell.modelData;newTitle.forceActiveFocus()} }
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 1
                        Label { text: dayCell.modelData.day; color: dayCell.modelData.inMonth ? Theme.text : Theme.secondary; font.bold: dayCell.modelData.today }
                        Repeater {
                            model: dayCell.modelData.entries.slice(0,panel.workspace.view === "week" ? 8 : 3)
                            Label {
                                id: marker
                                required property var modelData
                                readonly property var entry: modelData
                                Layout.fillWidth: true
                                text: (modelData.taskStatus === "COMPLETED" ? "✓ " : "")+(modelData.kind === "start" ? "● " : modelData.kind === "deadline" ? "⚑ " : "●⚑ ") + modelData.title
                                color: modelData.kind === "start" ? Theme.accent : Theme.danger
                                elide: Text.ElideRight
                                font.pixelSize: 11*Theme.scale
                                TapHandler { onTapped: {panel.selectedDay=dayCell.modelData;panel.openTask(marker.modelData.taskId)} }
                                Drag.active: markerDrag.active
                                Drag.source: marker
                                Drag.hotSpot.x: width/2 + markerDrag.centroid.position.x - markerDrag.centroid.pressPosition.x
                                Drag.hotSpot.y: height/2 + markerDrag.centroid.position.y - markerDrag.centroid.pressPosition.y
                                DragHandler { id: markerDrag; target: null; onActiveChanged: {panel.draggingMarker=active;if(active)panel.draggedEntry=marker.entry;else {marker.Drag.drop();panel.draggedEntry=null}} }
                            }
                        }
                        Label {
                            visible: dayCell.modelData.entries.length > (panel.workspace.view === "week" ? 8 : 3)
                            text: "+"+(dayCell.modelData.entries.length-(panel.workspace.view === "week" ? 8 : 3))
                            color: Theme.accent
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.preferredWidth: 200
            Layout.minimumWidth: 200
            Layout.maximumWidth: 200
            Layout.fillHeight: true
            Label { text: panel.selectedDay.date || qsTr("选择日期"); font.bold: true }
            TextField { id: newTitle; Layout.fillWidth: true; placeholderText: qsTr("当天新任务，Enter添加"); enabled: !!panel.selectedDay.date && !panel.workspace.busy; onAccepted: panel.workspace.createForDate(text,panel.selectedDay.date) }
            Label { text: qsTr("● 开始  ⚑ 截止\n同日合并；点改期可跨月调整。"); Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: 11*Theme.scale }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ColumnLayout {
                    width: 170
                    Repeater {
                        model: panel.selectedDay.entries
                        ColumnLayout {
                            id: detailRow
                            required property var modelData
                            Layout.fillWidth: true
                            Button { Layout.fillWidth: true; text: detailRow.modelData.title; onClicked: panel.openTask(detailRow.modelData.taskId) }
                            Button { text: qsTr("改期…"); onClicked: panel.offerMove(detailRow.modelData,panel.selectedDay.date) }
                        }
                    }
                }
            }
        }
    }
    Item {
        visible: false
        Dialog {
            id: unscheduled
            parent: Overlay.overlay
            anchors.centerIn: parent
            title: qsTr("未排期任务")
            width: Math.min(480,parent.width-40)
            height: Math.min(500,parent.height-40)
            modal: true
            standardButtons: Dialog.Close
            contentItem: ListView {
                clip: true
                model: panel.workspace.unscheduledTasks
                delegate: ItemDelegate {required property var modelData;width: ListView.view.width;text: modelData.title;onClicked: {unscheduled.close();panel.openTask(modelData.taskId)}}
                Label {anchors.centerIn: parent;visible: panel.workspace.unscheduledTasks.length===0;text: qsTr("没有未排期的活动任务")}
            }
        }
        Dialog {
        id: moveDialog
        parent: Overlay.overlay
        x: (parent.width-width)/2
        y: (parent.height-height)/2
        title: qsTr("调整任务日期")
        modal: true
        closePolicy: Popup.NoAutoClose
        ColumnLayout {
            Label { text: panel.moving.title || "" }
            Label { text: qsTr("只修改所选字段，另一个日期保持原值。") }
            ComboBox { id: markerKind; model: [qsTr("计划开始"),qsTr("截止日期")]; enabled: panel.moving.kind === "both" }
            TextField { id: targetDate; placeholderText: "yyyy-MM-dd"; selectByMouse: true }
            Label { text: panel.workspace.errorText; color: Theme.danger; Layout.maximumWidth: 300; wrapMode: Text.Wrap }
        }
        footer: DialogButtonBox {
            Button { text: qsTr("取消"); enabled: !panel.workspace.busy; onClicked: moveDialog.close() }
            Button { text: qsTr("确认改期"); enabled: !panel.workspace.busy; onClicked: panel.workspace.moveCalendar(panel.moving.taskId,panel.moving.rowVersion,markerKind.currentIndex === 0 ? "start" : "deadline",targetDate.text) }
        }
        }
    }
}
