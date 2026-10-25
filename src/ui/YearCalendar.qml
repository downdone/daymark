pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    implicitWidth: 640
    implicitHeight: 480
    required property TaskWorkspace workspace
    property string anchorDate: workspace.dateOffset(0)
    property var months: []
    property string wantedDate: ""
    signal anchorRequested(string date)
    signal daySelected(string date)
    signal monthSelected(string date)
    contentWidth: availableWidth
    clip: true
    function moveFocus(date, offset, months) {
        wantedDate = months ? workspace.shiftCalendarScale(date, offset, "month") : workspace.shiftDate(date, offset);
        if (wantedDate.slice(0, 4) !== anchorDate.slice(0, 4))
            anchorRequested(wantedDate);
        Qt.callLater(focusWanted);
    }
    function focusWanted() {
        for (let m = 0; m < monthCells.count; m++) {
            const month = monthCells.itemAt(m) as MonthView;
            if (!month)
                continue;
            const item = month.dateItem(wantedDate);
            if (!item)
                continue;
            item.forceActiveFocus();
            const flick = contentItem as Flickable;
            if (flick) {
                const y = item.mapToItem(flick.contentItem, 0, 0).y;
                flick.contentY = Math.max(0, Math.min(y - 40, Math.max(0, flick.contentHeight - flick.height)));
            }
            return;
        }
    }
    function refresh() {
        months = workspace.calendarMonthsFor(anchorDate);
    }
    onAnchorDateChanged: refresh()
    Component.onCompleted: refresh()
    Connections {
        target: root.workspace
        function onStateChanged() {
            if (root.visible)
                root.refresh();
        }
    }
    component DateButton: Button {
        required property var modelData
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitWidth: 28
        implicitHeight: Math.max(28, Theme.captionSize + 10)
        padding: 1
        horizontalPadding: 0
        text: modelData.day
        flat: true
        highlighted: modelData.today
        palette.buttonText: modelData.inMonth ? Theme.text : Theme.secondary
        font.pixelSize: Theme.captionSize
        Accessible.name: modelData.date
        onClicked: root.daySelected(modelData.date)
        Keys.onLeftPressed: root.moveFocus(modelData.date, -1, false)
        Keys.onRightPressed: root.moveFocus(modelData.date, 1, false)
        Keys.onUpPressed: root.moveFocus(modelData.date, -7, false)
        Keys.onDownPressed: root.moveFocus(modelData.date, 7, false)
        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown) {
                root.moveFocus(modelData.date, event.key === Qt.Key_PageUp ? -1 : 1, true);
                event.accepted = true;
            }
        }
    }
    component MonthView: ColumnLayout {
        id: month
        function dateItem(date) {
            for (let i = 0; i < dateCells.count; i++) {
                const item = dateCells.itemAt(i) as DateButton;
                if (item && item.modelData.inMonth && item.modelData.date === date)
                    return item;
            }
            return null;
        }
        required property var modelData
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: 4
        Button {
            Layout.fillWidth: true
            text: month.modelData.month + qsTr("月")
            flat: true
            font.pixelSize: Theme.headingSize
            onClicked: root.monthSelected(month.modelData.anchor)
        }
        GridLayout {
            columns: 7
            Layout.fillWidth: true
            columnSpacing: 1
            rowSpacing: 1
            Repeater {
                model: root.workspace.sundayFirst ? ["日", "一", "二", "三", "四", "五", "六"] : ["一", "二", "三", "四", "五", "六", "日"]
                Label {
                    required property string modelData
                    text: modelData
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.captionSize
                    color: Theme.secondary
                }
            }
            Repeater {
                id: dateCells
                model: month.modelData.days
                DateButton {}
            }
        }
    }
    GridLayout {
        width: root.availableWidth
        columns: Math.max(1, Math.min(4, Math.floor(width / (240 * Theme.scale))))
        columnSpacing: 24
        rowSpacing: 24
        Repeater {
            id: monthCells
            model: root.months
            MonthView {}
        }
    }
}
