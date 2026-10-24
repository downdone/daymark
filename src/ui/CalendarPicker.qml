pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: calendar
    required property TaskWorkspace workspace
    property var month: ({title:"",days:[]})
    property string selectedDate: ""
    property bool allowClear: true
    signal dateSelected(string value)
    signal moveMonth(int offset)
    signal dismissRequested()
    function focusSelectedDay(){
        const index=month.days.findIndex(day=>day.date===selectedDate)
        const day=dayButtons.itemAt(Math.max(0,index));if(day)day.forceActiveFocus()
    }
    Keys.onEscapePressed: dismissRequested()
    spacing: 4
    RowLayout {
        Layout.fillWidth: true
        IconToolButton {contentItem: AppIcon {name: "left"} Accessible.name: qsTr("上月");onClicked: calendar.moveMonth(-1)}
        Label {Layout.fillWidth: true;text: calendar.month.title;horizontalAlignment: Text.AlignHCenter}
        IconToolButton {contentItem: AppIcon {name: "right"} Accessible.name: qsTr("下月");onClicked: calendar.moveMonth(1)}
    }
    GridLayout {
        Layout.fillWidth: true
        columns: 7
        uniformCellWidths: true
        columnSpacing: 2
        rowSpacing: 2
        Repeater {
            model: calendar.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
            Label {required property string modelData;text: modelData;font.pixelSize: Theme.captionSize;color: Theme.secondary;Layout.fillWidth: true;horizontalAlignment: Text.AlignHCenter}
        }
        Repeater {
            id: dayButtons
            model: calendar.month.days
            Button {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                Layout.minimumWidth: 28
                implicitWidth: 32
                padding: 4
                horizontalPadding: 0
                implicitHeight: Math.max(32,contentItem.implicitHeight+8)
                text: modelData.day
                flat: calendar.selectedDate!==modelData.date
                highlighted: calendar.selectedDate===modelData.date
                font.weight: modelData.today ? Font.DemiBold : Font.Normal
                palette.buttonText: modelData.today ? Theme.accent : Theme.text
                Accessible.name: modelData.date+(modelData.today ? qsTr("，今天") : "")+(highlighted ? qsTr("，已选择") : "")
                onClicked: calendar.dateSelected(modelData.date)
                function focusOffset(offset) {const next=dayButtons.itemAt(Math.max(0,Math.min(dayButtons.count-1,index+offset)));if(next)next.forceActiveFocus()}
                Keys.onLeftPressed: focusOffset(-1)
                Keys.onRightPressed: focusOffset(1)
                Keys.onUpPressed: focusOffset(-7)
                Keys.onDownPressed: focusOffset(7)
            }
        }
    }
    Flow {
        Layout.fillWidth: true
        spacing: 4
        Button {text: qsTr("今天");flat: true;onClicked: calendar.dateSelected(calendar.workspace.dateOffset(0))}
        Button {text: qsTr("明天");flat: true;onClicked: calendar.dateSelected(calendar.workspace.dateOffset(1))}
        Button {visible: calendar.allowClear;text: qsTr("清除");flat: true;onClicked: calendar.dateSelected("")}
    }
    Label {Layout.fillWidth: true;text: qsTr("可直接输入 yyyy-MM-dd，Enter 收起日历");font.pixelSize: Theme.captionSize;color: Theme.secondary;wrapMode: Text.Wrap}
}
