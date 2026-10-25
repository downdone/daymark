pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string objectPrefix: "task"
    property string startDateValue: ""
    property string endDateValue: ""
    property bool datesEdited: false
    property bool inlinePickers:false
    signal pickerRequested(var field,string kind)
    property Item startEndpointHost:null
    property Item endEndpointHost:null
    readonly property bool hasSpecificTime: allDay.checked
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
        hadSchedule=!!task.schedule;allDay.checked=s.allDay===undefined ? false : !s.allDay
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
    RowLayout {visible:root.hasSpecificTime && zoneExpanded.checked;Layout.fillWidth:true
        Label {text:qsTr("提前提醒");font.pixelSize:Theme.captionSize;color:Theme.secondary}
        ComboBox {Layout.fillWidth:true;model:[qsTr("指定提醒时刻"),qsTr("开始时"),qsTr("开始前5分钟"),qsTr("开始前10分钟"),qsTr("开始前30分钟"),qsTr("开始前1小时")];currentIndex:[-1,0,5,10,30,60].indexOf(root.alertMinutes);onActivated:{root.alertMinutes=[-1,0,5,10,30,60][currentIndex];root.changed(true)}}
    }
    ColumnLayout {
        Layout.fillWidth: true;spacing: 6
        SettingSwitch {id: allDay;objectName: root.objectPrefix+"AllDay";Layout.fillWidth: true;symbol: "calendar";text: qsTr("指定时间");Accessible.description: qsTr("为同一开始和结束日期指定时间");onClicked: {if(checked)root.dateDefaultsRequested();root.changed(true)}}
        GridLayout {
            id: timesGrid
            visible: root.hasSpecificTime
            Layout.fillWidth: true;columns: root.width>=440*Theme.scale ? 2 : 1;columnSpacing: 16;rowSpacing: 6
            ColumnLayout {
                parent:root.startEndpointHost || timesGrid;visible:root.hasSpecificTime
                Layout.fillWidth: true;Layout.preferredWidth:root.startEndpointHost ? 120*Theme.scale : 1;Layout.minimumWidth:root.startEndpointHost ? 96*Theme.scale : 0;Layout.maximumWidth:root.startEndpointHost ? 132*Theme.scale : Infinity
                Label {visible:!root.startEndpointHost;text: qsTr("开始时间");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TimeField {id: startTime;objectName: root.objectPrefix+"TimeStart";inlineMode:root.inlinePickers;onInlinePickerRequested:root.pickerRequested(startTime,"time");Layout.fillWidth: true;timeLabel: qsTr("开始时间");Accessible.name: qsTr("开始时间");onTextEdited: root.changed(true);onTimeChosen: root.changed(true)}
            }
            ColumnLayout {
                parent:root.endEndpointHost || timesGrid;visible:root.hasSpecificTime
                Layout.fillWidth: true;Layout.preferredWidth:root.endEndpointHost ? 120*Theme.scale : 1;Layout.minimumWidth:root.endEndpointHost ? 96*Theme.scale : 0;Layout.maximumWidth:root.endEndpointHost ? 132*Theme.scale : Infinity
                Label {visible:!root.endEndpointHost;text: qsTr("结束时间");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TimeField {id: endTime;objectName: root.objectPrefix+"TimeEnd";inlineMode:root.inlinePickers;onInlinePickerRequested:root.pickerRequested(endTime,"time");Layout.fillWidth: true;timeLabel: qsTr("结束时间");Accessible.name: qsTr("结束时间");onTextEdited: root.changed(true);onTimeChosen: root.changed(true)}
            }
        }
        CheckBox {id:zoneExpanded;visible:root.hasSpecificTime;text:qsTr("时区与提醒选项");checked: false}
        RowLayout {
            visible: root.hasSpecificTime && zoneExpanded.checked;Layout.fillWidth: true;spacing: 12
            Label {text: qsTr("时区");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            TextField {id: zone;objectName: root.objectPrefix+"TimeZone";Layout.fillWidth: true;Accessible.name: qsTr("起止时间所属时区");onTextEdited: root.changed(true)}
        }
    }
    ColumnLayout {
        Layout.fillWidth: true;spacing: 8
        SettingSwitch {id: repeating;objectName: root.objectPrefix+"Repeating";Layout.fillWidth: true;symbol: "reload";text: qsTr("循环生成任务");Accessible.description: qsTr("按周期生成可独立完成的任务");onClicked: root.changed(false)}
        Label {visible: root.hadRepeat && !repeating.checked;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存将停止循环；本次、历史和单独编辑过的任务保留，未修改的未来任务移除。能在编辑记录中查看此次修改。 ")}
        ColumnLayout {
            visible: repeating.checked;Layout.fillWidth: true;spacing: 8
            ComboBox {id: frequency;objectName: root.objectPrefix+"RepeatFrequency";Layout.fillWidth: true;Accessible.name: qsTr("任务循环周期");model: [qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("完成后按天循环")];onActivated: root.changed(false)}
            RowLayout {Layout.fillWidth: true;Label {text: qsTr("每隔")} SpinBox {id: interval;objectName: root.objectPrefix+"RepeatInterval";from: 1;to: 36500;editable: true;Accessible.name: qsTr("任务循环间隔");onValueModified: root.changed(false)} Label {Layout.fillWidth: true;text: [qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年"),qsTr("天")][frequency.currentIndex]}}
            Flow {
                visible: frequency.currentIndex===1;Layout.fillWidth: true;spacing: 4
                Repeater {model: [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")];CheckBox {required property int index;required property string modelData;text: modelData;checked: !!(root.weekdayMask & (1<<index));onClicked: {root.weekdayMask=checked ? root.weekdayMask | (1<<index) : root.weekdayMask & ~(1<<index);root.changed(false)}}}
            }
            DateField {id: until;objectName: root.objectPrefix+"RepeatUntil";inlineMode:root.inlinePickers;onInlinePickerRequested:root.pickerRequested(until,"date");Layout.fillWidth: true;workspace: root.workspace;dateLabel: qsTr("循环结束日期");placeholderText: qsTr("结束日期 · 留空持续循环");onTextEdited: root.changed(false);onDateChosen: root.changed(false)}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("以计划日期为起点；月、年按起点的月日循环，无效日期跳过。每次任务独立完成和编辑。")}
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary;text:root.workspace.recurrencePreview({anchor:root.startDateValue,frequency:root.frequencies[frequency.currentIndex],interval:interval.value,weekdays:root.weekdayMask,end:until.text})}
            SettingSwitch {id: future;objectName: root.objectPrefix+"RepeatFuture";visible: root.hadRepeat;Layout.fillWidth: true;symbol: "reload";text: qsTr("后续任务沿用本次属性");onClicked: root.draftChanged()}
            Label {visible: root.hadRepeat && (root.repeatDirty || future.checked);Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存将更新本次及后续未修改任务；历史与已单独编辑的任务保留。")}
        }
    }
}
