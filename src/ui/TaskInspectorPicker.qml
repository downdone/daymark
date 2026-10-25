pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string objectPrefix: ""
    property var field: null
    property string kind: ""
    property var month: ({
            title: "",
            days: []
        })
    property string date: ""
    readonly property bool active: kind.length > 0
    signal dismissed
    Connections {
        target: root.field
        ignoreUnknownSignals: true
        function onPickerVisibleChanged() {
            if (root.field && !root.field.pickerVisible && root.active)
                root.close();
        }
    }
    function openFor(item, type) {
        field = item;
        kind = type;
        date = item.text.length >= 10 ? item.text.slice(0, 10) : type === "reminder" ? workspace.defaultReminderLocal().slice(0, 10) : workspace.dateOffset(0);
        month = workspace.datePicker(date, 0);
        clock.text = type === "time" ? item.text : item.text.length >= 16 ? item.text.slice(11, 16) : type === "reminder" ? workspace.defaultReminderLocal().slice(11, 16) : "09:00";
        frequency.currentIndex = type === "reminder" ? Math.max(0, ["NONE", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"].indexOf(item.repeatFrequency)) : 0;
        interval.value = type === "reminder" ? item.repeatInterval : 1;
        Qt.callLater(function () {
            if (root.kind === "date" || root.kind === "reminder")
                calendar.focusSelectedDay();
            else
                clock.forceActiveFocus();
        });
    }
    function close() {
        const owner = field;
        kind = "";
        field = null;
        if (owner)
            owner.closePicker();
        dismissed();
        if (owner && owner.visible)
            owner.forceActiveFocus();
    }
    Keys.onEscapePressed: close()
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        WidgetAction {
            symbol: "left"
            description: qsTr("返回任务详情")
            onClicked: root.close()
        }
        Label {
            Layout.fillWidth: true
            text: root.kind === "reminder" ? qsTr("提醒") : root.kind === "time" ? qsTr("选择时间") : qsTr("选择日期")
            font.weight: Font.DemiBold
        }
    }
    ScrollView {
        id: pickerScroll
        Layout.fillWidth: true
        Layout.fillHeight: true
        implicitHeight: 400
        clip: true
        contentWidth: availableWidth
        ColumnLayout {
            width: pickerScroll.availableWidth
            spacing: 8
            CalendarPicker {
                id: calendar
                Layout.fillWidth: true
                visible: root.kind !== "time"
                workspace: root.workspace
                month: root.month
                selectedDate: root.date
                onDateSelected: function (value) {
                    root.date = value;
                    if (root.kind === "date") {
                        root.field.chooseDate(value);
                        root.close();
                    }
                }
                onMoveMonth: function (offset) {
                    root.month = root.workspace.datePicker(root.month.anchor, offset);
                }
                onDismissRequested: root.close()
            }
            InlineClock {
                id: clock
                objectName: root.objectPrefix+"inspectorClock"
                Layout.fillWidth: true
                visible: root.kind !== "date"
                onAccepted: apply.clicked()
            }
            ComboBox {
                id: frequency
                Layout.fillWidth: true
                visible: root.kind === "reminder"
                model: [qsTr("提醒一次"), qsTr("每天提醒此任务"), qsTr("每周提醒此任务"), qsTr("每月提醒此任务"), qsTr("每年提醒此任务")]
                Accessible.name: qsTr("重复提醒")
            }
            RowLayout {
                visible: root.kind === "reminder" && frequency.currentIndex > 0
                Label {
                    text: qsTr("每隔")
                }
                SpinBox {
                    id: interval
                    from: 1
                    to: 36500
                    editable: true
                    Accessible.name: qsTr("提醒循环间隔")
                }
            }
            Label {
                visible: root.kind === "reminder"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: Theme.captionSize
                color: Theme.secondary
                text: root.workspace.reminderPreview(root.date + " " + clock.text, ["NONE", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"][frequency.currentIndex], interval.value)
            }
            Label {
                visible: root.kind === "reminder"
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                text: qsTr("应用运行时检查提醒；重复提醒在任务完成后停止。")
                font.pixelSize: Theme.captionSize
                color: Theme.secondary
            }
        }
    }
    RowLayout {
        Layout.fillWidth: true
        Button {
            visible: root.kind === "reminder"
            text: qsTr("取消提醒")
            onClicked: {
                root.field.text = "";
                root.field.repeatFrequency = "NONE";
                root.field.reminderChosen();
                root.close();
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Button {
            text: qsTr("返回")
            onClicked: root.close()
        }
        Button {
            id: apply
            objectName: root.objectPrefix+"applyInspectorPicker"
            visible: root.kind !== "date"
            text: qsTr("应用")
            highlighted: true
            onClicked: {
                if (root.kind === "time") {
                    root.field.chooseTime(Number(clock.text.slice(0, 2)), Number(clock.text.slice(3, 5)));
                } else {
                    root.field.text = root.date ? root.date + " " + clock.text : "";
                    root.field.repeatFrequency = ["NONE", "DAILY", "WEEKLY", "MONTHLY", "YEARLY"][frequency.currentIndex];
                    root.field.repeatInterval = interval.value;
                    root.field.reminderChosen();
                }
                root.close();
            }
        }
    }
}
