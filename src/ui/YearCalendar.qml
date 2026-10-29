pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    objectName: "calendarYearView"
    implicitWidth: 640
    implicitHeight: 480
    property bool smallSurface: false
    readonly property CalendarMetrics metrics: CalendarMetrics {smallSurface: root.smallSurface}
    required property TaskWorkspace workspace
    property string anchorDate: workspace.dateOffset(0)
    property var months: []
    property string wantedDate: ""
    property string selectedDate: ""
    property string selectedMonth: ""
    property bool revealPending: false
    property bool revealWithFocus: false
    readonly property real monthHeadingHeight: Math.max(32, root.metrics.headingSize + 12)
    readonly property var pinnedMonths: {
        const flick = contentItem as Flickable;
        if (!flick || flick.contentY <= 0)
            return [];
        const result = [];
        for (let i = 0; i < monthCells.count; i++) {
            const month = monthCells.itemAt(i) as MonthView;
            if (month && month.y < flick.contentY && month.y + month.height > flick.contentY)
                result.push({month: month.modelData.month, x: month.x, width: month.width});
        }
        return result;
    }
    signal dateHighlighted(string date)
    signal anchorRequested(string date)
    signal daySelected(string date)
    signal monthSelected(string date)
    contentWidth: availableWidth
    contentHeight: yearGrid.implicitHeight
    clip: true
    function reveal(date, takeFocus) {
        wantedDate = date;
        selectedMonth = "";
        revealPending = true;
        revealWithFocus = !!takeFocus;
        Qt.callLater(applyReveal);
    }
    function moveFocus(date, offset, months) {
        wantedDate = months ? workspace.shiftCalendarScale(date, offset, "month") : workspace.shiftDate(date, offset);
        if (wantedDate.slice(0, 4) !== anchorDate.slice(0, 4))
            anchorRequested(wantedDate);
        reveal(wantedDate, true);
    }
    function focusWanted() {
        reveal(wantedDate, true);
    }
    function applyReveal() {
        const flick = contentItem as Flickable;
        if (!revealPending || !visible || !flick || flick.height <= 0 || yearGrid.implicitHeight <= 0)
            return;
        for (let m = 0; m < monthCells.count; m++) {
            const month = monthCells.itemAt(m) as MonthView;
            if (!month)
                continue;
            const item = month.dateItem(wantedDate);
            if (!item)
                continue;
            if (month.height <= 0 || item.height <= 0)
                return;
            const y = month.height <= flick.height ? month.y - 8 : item.mapToItem(flick.contentItem, 0, 0).y - monthHeadingHeight - 8;
            flick.contentY = Math.max(0, Math.min(y, Math.max(0, flick.contentHeight - flick.height)));
            revealPending = false;
            if (revealWithFocus)
                item.forceActiveFocus();
            return;
        }
    }
    function refresh() {
        const next=workspace.calendarMonthsFor(anchorDate);
        if(JSON.stringify(next)!==JSON.stringify(months))months=next;
    }
    onAnchorDateChanged: {refresh();reveal(anchorDate, false)}
    onAvailableHeightChanged: if(revealPending)Qt.callLater(applyReveal)
    onContentHeightChanged: if(revealPending)Qt.callLater(applyReveal)
    Component.onCompleted: {refresh();reveal(anchorDate, false)}
    Connections {
        target: root.workspace
        function onStateChanged() {
            if (root.visible)
                root.refresh();
        }
    }
    component DateButton: Button {
        id: dateButton
        required property var modelData
        objectName: "calendarYearDay-"+modelData.date+"-"+modelData.inMonth
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        implicitWidth: 28
        implicitHeight: Math.max(28, root.metrics.captionSize + 10)
        padding: 1
        horizontalPadding: 0
        text: modelData.day
        flat: true
        readonly property bool todayMarker: modelData.inMonth && modelData.today
        readonly property bool dateSelected: modelData.inMonth && root.selectedDate===modelData.date
        highlighted: false
        enabled: modelData.inMonth
        focusPolicy: modelData.inMonth ? Qt.StrongFocus : Qt.NoFocus
        contentItem: Text {text:dateButton.modelData.inMonth ? dateButton.text : "";font:dateButton.font;color:dateButton.dateSelected ? Theme.accentText : dateButton.todayMarker ? Theme.calendarTodayText : Theme.text;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
        background: Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width,parent.height);height:width;radius:width/2
            color: dateButton.dateSelected ? Theme.accent : dateButton.todayMarker ? Theme.calendarToday : dateButton.hovered ? Theme.hover : "transparent"
            border.width: dateButton.visualFocus ? 1 : 0;border.color: Theme.accent
        }
        palette.buttonText: modelData.inMonth ? Theme.text : Theme.secondary
        font.pixelSize: root.metrics.captionSize
        Accessible.name: modelData.date
        onClicked: root.dateHighlighted(modelData.date)
        onDoubleClicked: root.daySelected(modelData.date)
        Keys.onReturnPressed: root.daySelected(modelData.date)
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
        Layout.minimumWidth: 0
        Layout.alignment: Qt.AlignTop
        Layout.preferredWidth: 1
        spacing: 4
        Button {
            Layout.fillWidth: true
            objectName: "calendarYearMonth-"+month.modelData.anchor
            text: month.modelData.month + qsTr("月")
            flat: true
            font.pixelSize: root.metrics.headingSize
            onClicked: root.selectedMonth=month.modelData.anchor
            highlighted: root.selectedMonth===month.modelData.anchor
            onDoubleClicked: root.monthSelected(month.modelData.anchor)
            Keys.onReturnPressed: root.monthSelected(month.modelData.anchor)
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
                    font.pixelSize: root.metrics.captionSize
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
        id: yearGrid
        width: root.availableWidth
        onImplicitHeightChanged: if(root.revealPending)Qt.callLater(root.applyReveal)
        columns: Math.max(1, Math.min(4, Math.floor(width / ((root.smallSurface ? 196 : 240) * Theme.scale))))
        columnSpacing: root.smallSurface ? 12 : 24
        rowSpacing: root.smallSurface ? 12 : 24
        Repeater {
            id: monthCells
            model: root.months
            onItemAdded: if(root.revealPending)Qt.callLater(root.applyReveal)
            MonthView {}
        }
    }
    // A clipped month heading must not leave a dense year viewport anonymous.
    Item {
        parent: root
        z: 2
        width: root.availableWidth;height: root.monthHeadingHeight
        visible: root.pinnedMonths.length > 0
        Rectangle {anchors.fill: parent;color: Theme.content}
        MouseArea {anchors.fill: parent} // Do not activate a date hidden by the heading.
        Repeater {
            model: root.pinnedMonths
            Label {
                required property var modelData
                objectName: "calendarPinnedMonth-" + modelData.month
                x: modelData.x;width: modelData.width;height: root.monthHeadingHeight
                text: modelData.month + qsTr("月")
                font.pixelSize: root.metrics.headingSize
                horizontalAlignment: Text.AlignHCenter;verticalAlignment: Text.AlignVCenter
            }
        }
        Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;color: Theme.separator}
    }
}
