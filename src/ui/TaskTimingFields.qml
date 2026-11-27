pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string objectPrefix: "task"
    property string taskId: ""
    property string startDateValue: ""
    property string endDateValue: ""
    property bool datesEdited: false
    property bool inlinePickers:false
    property bool compactPresentation:false
    property bool showTimeControls:true
    property bool useTaskCycle:false
    property bool legacyRule:false
    property bool convertLegacy:false
    property var cycleSnapshot:({})
    readonly property string startTimeValue:startTime.text
    readonly property string endTimeValue:endTime.text
    readonly property string timeZoneValue:zone.text
    property bool repeatExpanded:false
    signal pickerRequested(var field,string kind)
    property Item startEndpointHost:null
    property Item endEndpointHost:null
    readonly property bool hasSpecificTime: allDay.checked
    readonly property bool repeatEnabled: repeating.checked
    signal dateDefaultsRequested()
    signal rangeDatesChosen(string startDate,string endDate)
    readonly property bool rangePickerVisible: rangePicker.visible
    property bool loading: false
    property bool timeDirty: false
    property bool repeatDirty: false
    property bool hadSchedule: false
    property bool hadRepeat: false
    property real repeatVersion: 1
    property int weekdayMask: 0
    readonly property int defaultRepeatWeekdays: workspace.recurrenceWeekdayMask(startDateValue)
    property int monthDay: 1
    property int yearMonth: 1
    property int alertMinutes: -1
    readonly property bool modalOpen: repeatOptions.visible || until.pickerVisible || startTime.pickerVisible || endTime.pickerVisible || frequency.popup.visible || timeOptions.visible || rangePicker.visible || repeatChoice.popup.visible
    function closePickers(){finishRepeatOptions(false);until.closePicker();startTime.closePicker();endTime.closePicker();frequency.popup.close();timeOptions.close();rangePicker.close();repeatChoice.popup.close()}
    function openRange(source) {
        rangePicker.openFor(source || rangeButton,{taskId:taskId,startDate:startDateValue,endDate:endDateValue,startTime:hasSpecificTime ? startTime.text : "09:00",endTime:hasSpecificTime ? endTime.text : "10:00",zone:zone.text});
    }
    TimeRangePicker {
        id:rangePicker;objectPrefix:root.objectPrefix;workspace:root.workspace
        onSelected:function(value){allDay.checked=true;startTime.text=value.startTime;endTime.text=value.endTime;zone.text=value.zone;root.rangeDatesChosen(value.startDate,value.endDate);root.changed(true)}
    }
    readonly property var frequencies: ["DAILY","WEEKLY","MONTHLY","YEARLY","AFTER_DAYS"]
    signal draftChanged()
    function changed(time) {if(loading)return;if(time)timeDirty=true;else repeatDirty=true;draftChanged()}
    function adoptReminderCadence(cadence, every) {
        const index=frequencies.indexOf(cadence)
        if(index<0 || repeating.checked || legacyRule)return false
        repeating.checked=true
        frequency.currentIndex=index
        interval.value=every
        weekdayMask=index===1?defaultRepeatWeekdays:0
        changed(false)
        return true
    }
    function loadTask(task) {
        closePickers();loading=true
        const s=task.schedule || {},r=(useTaskCycle ? task.cycle || task.repeat : task.repeat) || {}
        cycleSnapshot=task.cycle || {};legacyRule=useTaskCycle && !!task.recurrenceId && !!r.active && !task.cycle;convertLegacy=false
        hadSchedule=!!task.schedule;allDay.checked=s.allDay===undefined ? false : !s.allDay
        startTime.text=s.startTime || "09:00";endTime.text=s.endTime || "10:00";zone.text=s.zone || workspace.systemTimeZone();alertMinutes=s.alertMinutes===undefined ? -1 : s.alertMinutes
        hadRepeat=!!r.active;repeating.checked=hadRepeat;frequency.currentIndex=Math.max(0,frequencies.indexOf(r.frequency || "DAILY"));interval.value=r.interval || 1;until.text=r.end || "";repeatVersion=r.rowVersion || 1;weekdayMask=r.weekdays || 0;monthDay=r.monthDay || 1;yearMonth=r.yearMonth || 1
        repeatExpanded=false;zoneExpanded.checked=false;loading=false;timeDirty=false;repeatDirty=false
    }
    function detailsPatch() {
        const patch={}
        if(timeDirty || (datesEdited && hadSchedule))patch.schedule=hasSpecificTime || alertMinutes>=0 ? {allDay:!hasSpecificTime,startDate:startDateValue,endDate:endDateValue || startDateValue,startTime:startTime.text,endTime:endTime.text,zone:zone.text,alertMinutes:alertMinutes} : null
        if(repeatDirty){
            const rule={active:repeating.checked,frequency:frequencies[frequency.currentIndex],interval:interval.value,end:until.text,rowVersion:repeatVersion,weekdays:weekdayMask,monthDay:monthDay,yearMonth:yearMonth}
            if(useTaskCycle)patch.cycle=Object.assign(rule,{rowVersion:cycleSnapshot.rowVersion || "0",zone:zone.text,convertLegacy:convertLegacy,legacyVersion:repeatVersion})
            else {if(hadRepeat)patch.repeatVersion=repeatVersion;patch.repeat=repeating.checked ? rule : null}
        }
        return patch
    }
    function saveDraft() {return {specific:allDay.checked,start:startTime.text,end:endTime.text,zone:zone.text,alert:alertMinutes,
        repeat:repeating.checked,frequency:frequency.currentIndex,interval:interval.value,until:until.text,weekdays:weekdayMask,
        advanced:zoneExpanded.checked,timeDirty:timeDirty,repeatDirty:repeatDirty,convertLegacy:convertLegacy}}
    function restoreDraft(draft) {
        closePickers();loading=true
        allDay.checked=draft.specific;startTime.text=draft.start;endTime.text=draft.end;zone.text=draft.zone;alertMinutes=draft.alert
        repeating.checked=draft.repeat;frequency.currentIndex=draft.frequency;interval.value=draft.interval;until.text=draft.until
        weekdayMask=draft.weekdays;zoneExpanded.checked=draft.advanced;timeDirty=draft.timeDirty;repeatDirty=draft.repeatDirty
        convertLegacy=draft.convertLegacy;loading=false
    }
    spacing: Theme.fieldGap
    RowLayout {visible:!root.useTaskCycle && root.hasSpecificTime && zoneExpanded.checked;Layout.fillWidth:true
        Label {text:qsTr("提前提醒");font.pixelSize:Theme.captionSize;color:Theme.secondary}
        ComboBox {Layout.fillWidth:true;model:[qsTr("指定提醒时刻"),qsTr("开始时"),qsTr("开始前5分钟"),qsTr("开始前10分钟"),qsTr("开始前30分钟"),qsTr("开始前1小时")];currentIndex:[-1,0,5,10,30,60].indexOf(root.alertMinutes);onActivated:{root.alertMinutes=[-1,0,5,10,30,60][currentIndex];root.changed(true)}}
    }
    ColumnLayout {
        Layout.fillWidth: true;spacing: Theme.fieldGap
        SettingSwitch {id: allDay;visible:!root.compactPresentation;objectName: root.objectPrefix+"AllDay";Layout.fillWidth: true;symbol: "calendar";text: qsTr("指定时间");Accessible.description: qsTr("为同一开始和结束日期指定时间");onClicked: {if(checked)root.dateDefaultsRequested();root.changed(true)}}
        FormRow {contextual:root.compactPresentation;
            visible: root.compactPresentation && (!root.hasSpecificTime || !root.startEndpointHost);label:qsTr("时段")
            RowLayout {
                Layout.fillWidth:true;spacing:4
                ActionButton {id:rangeButton;visible:!root.hasSpecificTime || !root.startEndpointHost;objectName:root.objectPrefix+"ChooseTimeRange";symbol:"clock";contentAlignment:Qt.AlignLeft;Layout.fillWidth:true;text:root.hasSpecificTime ? startTime.text+" — "+endTime.text : qsTr("添加时间…");onClicked:root.openRange(rangeButton)}
                IconToolButton {
                    parent:root.hasSpecificTime && root.endEndpointHost ? root.endEndpointHost : rangeButton.parent
                    visible:root.hasSpecificTime;contentItem:AppIcon{name:"more"} Accessible.name:qsTr("时段选项");onClicked:timeOptions.popup(this,0,height)
                    DS.Menu {
                        id:timeOptions
                        MenuItem {text:qsTr("改为仅日期");onTriggered:{allDay.checked=false;root.changed(true)}}
                        DS.MenuSeparator {}
                        MenuItem {text:root.useTaskCycle?qsTr("时区…"):qsTr("时区与提前提醒…");onTriggered:zoneExpanded.checked=!zoneExpanded.checked}
                    }
                }
            }
        }
        GridLayout {
            id: timesGrid
            visible:root.hasSpecificTime && !root.compactPresentation
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
        CheckBox {id:zoneExpanded;visible:root.hasSpecificTime && !root.compactPresentation;text:qsTr("时区与提醒选项");checked: false}
        RowLayout {
            visible: root.hasSpecificTime && zoneExpanded.checked;Layout.fillWidth: true;spacing: 12
            Label {text: qsTr("时区");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            TextField {id: zone;objectName: root.objectPrefix+"TimeZone";Layout.fillWidth: true;Accessible.name: qsTr("起止时间所属时区");onTextEdited: root.changed(true)}
        }
    }
    property var repeatSnapshot: ({})
    property bool repeatSnapshotValid: false
    property point repeatAnchor: Qt.point(12,12)
    function showRepeatOptions(source) {
        repeatSnapshot={enabled:repeating.checked,frequency:frequency.currentIndex,interval:interval.value,weekdays:weekdayMask}
        repeatSnapshotValid=true
        repeatAnchor=source.mapToItem(Overlay.overlay,0,source.height)
        repeatExpanded=true;repeatOptions.open()
    }
    function finishRepeatOptions(apply) {
        if(!repeatSnapshotValid)return
        if(apply){repeating.checked=true;changed(false)}
        else {repeating.checked=repeatSnapshot.enabled;frequency.currentIndex=repeatSnapshot.frequency;interval.value=repeatSnapshot.interval;weekdayMask=repeatSnapshot.weekdays}
        repeatSnapshotValid=false;repeatExpanded=false;repeatOptions.close()
    }
    function repeatSummary() {
        if(!repeating.checked)return qsTr("不重复")
        const labels=[qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("完成后每天")]
        let text=interval.value===1 ? labels[frequency.currentIndex] : qsTr("每 %1 %2").arg(interval.value).arg([qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年"),qsTr("天（完成后）")][frequency.currentIndex])
        if(frequency.currentIndex===1 && weekdayMask && (interval.value>1 || weekdayMask!==defaultRepeatWeekdays)){const days=[];for(let i=0;i<7;++i)if(weekdayMask & (1<<i))days.push([qsTr("周一"),qsTr("周二"),qsTr("周三"),qsTr("周四"),qsTr("周五"),qsTr("周六"),qsTr("周日")][i]);text+=" · "+days.join("、")}
        return text
    }
    ColumnLayout {
        id:repeatSection
        Layout.fillWidth: true;spacing: Theme.fieldGap
        Label {visible:root.legacyRule;Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("旧循环保留已生成任务。转换时将以当前任务继续，其他任务保留。");color:Theme.secondary}
        Button {visible:root.legacyRule && !root.convertLegacy;text:qsTr("转换为同一任务周期");onClicked:{root.convertLegacy=true;root.changed(false)}}
        Label {
            visible:root.useTaskCycle && root.repeatEnabled && (root.repeatDirty || root.datesEdited || !!root.cycleSnapshot.nextDate)
            Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.secondary;font.pixelSize:Theme.captionSize
            text:root.repeatDirty || root.datesEdited
                ? root.workspace.recurrencePreview({anchor:root.startDateValue || root.workspace.dateOffset(0),frequency:root.frequencies[frequency.currentIndex],interval:interval.value,weekdays:root.weekdayMask,end:until.text,sameTask:true})
                : qsTr("下次执行：%1").arg(root.cycleSnapshot.nextDate || "")
        }
        SettingSwitch {id: repeating;visible:!root.compactPresentation;objectName: root.objectPrefix+"Repeating";Layout.fillWidth: true;symbol: "reload";text: qsTr("循环生成任务");Accessible.description: qsTr("按周期生成可独立完成的任务");onClicked: root.changed(false)}
        FormRow {contextual:root.compactPresentation;visible:root.compactPresentation;label:root.useTaskCycle?qsTr("周期"):qsTr("重复")
            DS.ComboBox {
                id:repeatChoice;objectName:root.objectPrefix+"RepeatChoice";Layout.fillWidth:true;Accessible.name:qsTr("任务重复规则")
                withinHostWindow:root.useTaskCycle
                enabled:!root.legacyRule || root.convertLegacy
                separatorBefore:[1,5]
                model:[qsTr("不重复"),qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("自定义…")]
                Binding {target:repeatChoice;property:"currentIndex";value:repeating.checked ? (interval.value>1 || frequency.currentIndex===4 || (frequency.currentIndex===1 && root.weekdayMask!==0 && root.weekdayMask!==root.defaultRepeatWeekdays) ? 5 : frequency.currentIndex+1) : 0;when:!repeatChoice.popup.visible;restoreMode:Binding.RestoreNone}
                displayText:root.repeatSummary()
                onActivated:function(index){
                    if(index===5){root.showRepeatOptions(repeatChoice);return}
                    repeating.checked=index>0
                    if(index>0){frequency.currentIndex=index-1;interval.value=1;root.weekdayMask=index===2 ? root.defaultRepeatWeekdays : 0}
                    root.repeatExpanded=false;root.changed(false)
                }
            }
        }
        FormRow {contextual:true;visible:root.compactPresentation && repeating.checked;label:qsTr("结束重复")
            ColumnLayout {id:repeatEndHost;Layout.fillWidth:true}
        }
        Label {visible: root.hadRepeat && !repeating.checked;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("保存将停止循环；历史和单独编辑的任务保留。")}
        ColumnLayout {
            id:repeatControls;objectName:root.objectPrefix+"RepeatParameters"
            parent:root.compactPresentation ? repeatPopupBody : repeatSection
            visible:root.compactPresentation ? repeatOptions.visible : repeating.checked
            Layout.fillWidth: true;spacing: Theme.fieldGap
            DS.ComboBox {id: frequency;objectName: root.objectPrefix+"RepeatFrequency";withinHostWindow:true;Layout.fillWidth: true;Accessible.name: qsTr("任务循环周期");model:[qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("完成后按天循环")];onActivated: if(!repeatOptions.visible)root.changed(false)}
            RowLayout {Layout.fillWidth: true;Label {text: qsTr("每隔")} SpinBox {id: interval;objectName: root.objectPrefix+"RepeatInterval";from: 1;to: 36500;editable: true;Accessible.name: qsTr("任务循环间隔");onValueModified: if(!repeatOptions.visible)root.changed(false)} Label {Layout.fillWidth: true;text: [qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年"),qsTr("天")][frequency.currentIndex]}}
            Flow {
                visible: frequency.currentIndex===1;Layout.fillWidth: true;spacing: 4
                Repeater {model: [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")];CheckBox {required property int index;required property string modelData;text: modelData;checked: !!(root.weekdayMask & (1<<index));onClicked: {root.weekdayMask=checked ? root.weekdayMask | (1<<index) : root.weekdayMask & ~(1<<index);if(!repeatOptions.visible)root.changed(false)}}}
            }
            DateField {id: until;parent:root.compactPresentation ? repeatEndHost : repeatControls;objectName: root.objectPrefix+"RepeatUntil";enabled:!root.legacyRule || root.convertLegacy;inlineMode:root.inlinePickers && !root.compactPresentation;onInlinePickerRequested:root.pickerRequested(until,"date");Layout.fillWidth: true;workspace: root.workspace;dateLabel: qsTr("结束重复");placeholderText: qsTr("永不");onTextEdited: root.changed(false);onDateChosen: root.changed(false)}
            Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary;text:root.workspace.recurrencePreview({anchor:root.startDateValue || root.workspace.dateOffset(0),frequency:root.frequencies[frequency.currentIndex],interval:interval.value,weekdays:root.weekdayMask,end:until.text,sameTask:root.useTaskCycle})}
        }
        Label {visible:root.hadRepeat && repeating.checked && root.repeatDirty;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary
            text:root.useTaskCycle?qsTr("之后将按此规则安排同一任务，已完成记录会保留。"):qsTr("将更新后续未修改任务；历史和单独编辑项保留。")}
    }
    Item {
    visible:false
    Popup {
        id:repeatOptions;objectName:root.objectPrefix+"RepeatOptions";parent:Overlay.overlay
        x:Math.max(12,Math.min(root.repeatAnchor.x,parent ? parent.width-width-12 : 12))
        y:Math.max(12,Math.min(root.repeatAnchor.y+4,parent ? parent.height-height-12 : 12))
        width:Math.min(380*Theme.scale,parent ? parent.width-24 : 380)
        height:Math.min(contentItem.implicitHeight+24,parent ? parent.height-24 : 540)
        padding:12;modal:true;focus:true;closePolicy:Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Overlay.modal:Item {}
        enter:Transition {}
        exit:Transition {}
        background:PopoverSurface {}
        onClosed:{root.finishRepeatOptions(false);repeatChoice.forceActiveFocus()}
        contentItem:ColumnLayout {
            spacing:12
            Label {text:qsTr("自定义重复");font.weight:Font.DemiBold;Layout.fillWidth:true}
            ScrollView {id:repeatScroll;Layout.fillWidth:true;Layout.fillHeight:true;implicitHeight:repeatControls.implicitHeight;clip:true
                ColumnLayout {id:repeatPopupBody;width:repeatScroll.availableWidth;spacing:Theme.fieldGap}
            }
            RowLayout {Layout.fillWidth:true;Item {Layout.fillWidth:true} Button {objectName:root.objectPrefix+"CancelRepeatOptions";text:qsTr("取消");onClicked:root.finishRepeatOptions(false)} Button {objectName:root.objectPrefix+"ApplyRepeatOptions";text:qsTr("完成");highlighted:true;onClicked:root.finishRepeatOptions(true)}}
        }
    }
    }
}
