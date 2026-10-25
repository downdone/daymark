pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string startDateValue: ""
    property string endDateValue: ""
    property bool datesEdited: false
    readonly property bool hasSpecificTime: !allDay.checked
    signal dateDefaultsRequested()
    property bool loading: false
    property bool timeDirty: false
    property bool repeatDirty: false
    property bool hadSchedule: false
    property bool hadRepeat: false
    property real repeatVersion: 1
    property int weekdayMask: 0
    property int monthDay: 1
    property int yearMonth: 1
    property int alertMinutes: -1
    readonly property bool modalOpen: until.pickerVisible || startTime.pickerVisible || endTime.pickerVisible || frequency.popup.visible
    function closePickers(){until.closePicker();startTime.closePicker();endTime.closePicker();frequency.popup.close()}
    readonly property var frequencies: ["DAILY","WEEKLY","MONTHLY","YEARLY","AFTER_DAYS"]
    signal draftChanged()
    function changed(time) {if(loading)return;if(time)timeDirty=true;else repeatDirty=true;draftChanged()}
    function loadTask(task) {
        closePickers();loading=true
        const s=task.schedule || {},r=task.repeat || {}
        hadSchedule=!!task.schedule;allDay.checked=s.allDay===undefined ? true : s.allDay
        startTime.text=s.startTime || "09:00";endTime.text=s.endTime || "10:00";zone.text=s.zone || workspace.systemTimeZone();alertMinutes=s.alertMinutes===undefined ? -1 : s.alertMinutes
        hadRepeat=!!r.active;repeating.checked=hadRepeat;frequency.currentIndex=Math.max(0,frequencies.indexOf(r.frequency || "DAILY"));interval.value=r.interval || 1;until.text=r.end || "";repeatVersion=r.rowVersion || 1;weekdayMask=r.weekdays || 0;monthDay=r.monthDay || 1;yearMonth=r.yearMonth || 1
        future.checked=false;loading=false;timeDirty=false;repeatDirty=false
    }
    function detailsPatch() {
        const patch={}
        if(timeDirty || (datesEdited && hadSchedule))patch.schedule=hasSpecificTime ? {allDay:false,startDate:startDateValue,endDate:endDateValue,startTime:startTime.text,endTime:endTime.text,zone:zone.text,alertMinutes:alertMinutes} : null
        if(repeatDirty || future.checked){if(hadRepeat)patch.repeatVersion=repeatVersion;patch.repeat=repeating.checked ? {frequency:frequencies[frequency.currentIndex],interval:interval.value,end:until.text,rowVersion:repeatVersion,weekdays:weekdayMask,monthDay:monthDay,yearMonth:yearMonth} : null}
        return patch
    }
    spacing: Theme.fieldGap
    ColumnLayout {
        Layout.fillWidth: true;spacing: 6
        SettingSwitch {id: allDay;objectName: "taskAllDay";Layout.fillWidth: true;symbol: "calendar";text: qsTr("全天");Accessible.description: qsTr("关闭后为同一开始和结束日期指定时间");onClicked: {if(!checked)root.dateDefaultsRequested();root.changed(true)}}
        GridLayout {
            visible: root.hasSpecificTime
            Layout.fillWidth: true;columns: root.width>=440*Theme.scale ? 2 : 1;columnSpacing: 16;rowSpacing: 6
            ColumnLayout {
                Layout.fillWidth: true;Layout.preferredWidth: 1
                Label {text: qsTr("开始时间");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TimeField {id: startTime;objectName: "taskTimeStart";Layout.fillWidth: true;timeLabel: qsTr("开始时间");Accessible.name: qsTr("开始时间");onTextEdited: root.changed(true);onTimeChosen: root.changed(true)}
            }
            ColumnLayout {
                Layout.fillWidth: true;Layout.preferredWidth: 1
                Label {text: qsTr("结束时间");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TimeField {id: endTime;objectName: "taskTimeEnd";Layout.fillWidth: true;timeLabel: qsTr("结束时间");Accessible.name: qsTr("结束时间");onTextEdited: root.changed(true);onTimeChosen: root.changed(true)}
            }
        }
        RowLayout {
            visible: root.hasSpecificTime;Layout.fillWidth: true;spacing: 12
            Label {text: qsTr("时区");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            TextField {id: zone;objectName: "taskTimeZone";Layout.fillWidth: true;Accessible.name: qsTr("起止时间所属时区");onTextEdited: root.changed(true)}
        }
    }
    ColumnLayout {
        Layout.fillWidth: true;spacing: 8
        SettingSwitch {id: repeating;objectName: "taskRepeating";Layout.fillWidth: true;symbol: "reload";text: qsTr("重复任务");Accessible.description: qsTr("按周期生成可独立完成的任务");onClicked: root.changed(false)}
        Label {visible: root.hadRepeat && !repeating.checked;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存将停止循环；本次、历史和单独编辑过的任务保留，未修改的未来任务移除。能在编辑记录中查看此次修改。 ")}
        ColumnLayout {
            visible: repeating.checked;Layout.fillWidth: true;spacing: 8
            ComboBox {id: frequency;objectName: "taskRepeatFrequency";Layout.fillWidth: true;Accessible.name: qsTr("任务循环周期");model: [qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("完成后按天循环")];onActivated: root.changed(false)}
            RowLayout {Layout.fillWidth: true;Label {text: qsTr("每隔")} SpinBox {id: interval;objectName: "taskRepeatInterval";from: 1;to: 36500;editable: true;Accessible.name: qsTr("任务循环间隔");onValueModified: root.changed(false)} Label {Layout.fillWidth: true;text: [qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年"),qsTr("天")][frequency.currentIndex]}}
            Flow {
                visible: frequency.currentIndex===1;Layout.fillWidth: true;spacing: 4
                Repeater {model: [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")];CheckBox {required property int index;required property string modelData;text: modelData;checked: !!(root.weekdayMask & (1<<index));onClicked: {root.weekdayMask=checked ? root.weekdayMask | (1<<index) : root.weekdayMask & ~(1<<index);root.changed(false)}}}
            }
            DateField {id: until;objectName: "taskRepeatUntil";Layout.fillWidth: true;workspace: root.workspace;dateLabel: qsTr("循环结束日期");placeholderText: qsTr("结束日期 · 留空持续循环");onTextEdited: root.changed(false);onDateChosen: root.changed(false)}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("以计划日期为起点；月、年按起点的月日循环，无效日期跳过。每次任务独立完成和编辑。")}
            SettingSwitch {id: future;objectName: "taskRepeatFuture";visible: root.hadRepeat;Layout.fillWidth: true;symbol: "reload";text: qsTr("后续任务沿用本次属性");onClicked: root.draftChanged()}
            Label {visible: root.hadRepeat && (root.repeatDirty || future.checked);Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存将更新本次及后续未修改任务；历史与已单独编辑的任务保留。")}
        }
    }
}
