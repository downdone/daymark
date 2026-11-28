pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string taskId: ""
    property string objectPrefix: "task"
    property string startName: objectPrefix+"StartDate"
    property string endName: objectPrefix+"EndDate"
    property string reminderName: objectPrefix+"Reminder"
    property bool compact: false
    property bool inlinePickers: false
    property bool showEntry: true
    property bool showActions: true
    property bool opened: false
    property bool restoring: false
    property bool datesEdited: false
    property bool endExpanded: false
    property bool persistOnApply: false
    signal saveRequested()
    readonly property bool savedNextAlarm: !!timing.cycleSnapshot.nextDate && !timing.timeDirty && !datesEdited
    readonly property string alarmPreview: timing.alertMinutes < 0 || savedNextAlarm ? reminder.text :
        workspace.relativeAlarmLocal(startDate.text,timing.hasSpecificTime ? timing.startTimeValue : "09:00",
                                    timing.hasSpecificTime ? timing.timeZoneValue : workspace.systemTimeZone(),timing.alertMinutes)
    function acceptEdits(){
        finish(true)
        if(!opened && persistOnApply)saveRequested()
    }
    property var checkpoint: null
    property string error: ""
    property alias startField: startDate
    property alias endField: endDate
    property alias startGroup: startRow
    property alias endGroup: endRow
    property alias reminderField: reminder
    property alias timingFields: timing
    readonly property bool pickerOpen: startDate.pickerVisible || endDate.pickerVisible || reminder.pickerVisible || reminderMode.popup.visible || timing.modalOpen
    readonly property string summary: !startDate.text && !endDate.text ? (alarmPreview ? qsTr("%1 提醒").arg(alarmPreview) : qsTr("安排"))
        : !startDate.text ? qsTr("最晚于 %1").arg(endDate.text)
        : startDate.text+(endDate.text && endDate.text!==startDate.text ? " — "+endDate.text : "")+
            (timing.hasSpecificTime ? " · "+timing.startTimeValue+"–"+timing.endTimeValue : "")
    readonly property string extras: [timing.repeatEnabled?timing.repeatSummary():"",alarmPreview && (startDate.text || endDate.text) ? qsTr("%1 提醒").arg(alarmPreview) : ""].filter(value=>value.length>0).join(" · ")
    signal pickerRequested(var field,string kind)
    signal datesChanged(string value)
    signal edited()
    signal cancelled()
    signal applied()

    spacing: compact ? 4 : Theme.fieldGap
    Layout.fillWidth:true
    function closePickers(){startDate.closePicker();endDate.closePicker();reminder.closePicker();reminderMode.popup.close();timing.closePickers()}
    function startEditing(){
        if(opened)return
        checkpoint={start:startDate.text,end:endDate.text,reminder:reminder.text,frequency:reminder.repeatFrequency,
            interval:reminder.repeatInterval,timing:timing.saveDraft(),datesEdited:datesEdited}
        endExpanded=!!endDate.text && endDate.text!==startDate.text
        opened=true;error=""
    }
    function finish(apply){
        if(!opened)return
        if(apply && timing.alertMinutes>=0 && !alarmPreview){error=qsTr("请检查开始日期、时间和时区，当前无法计算提醒时间。");return}
        if(apply && startDate.text && endDate.text && startDate.text>endDate.text){error=qsTr("结束日期不能早于开始日期。");return}
        closePickers()
        restoring=true
        if(!apply && checkpoint){
            startDate.text=checkpoint.start;endDate.text=checkpoint.end;reminder.text=checkpoint.reminder
            reminder.repeatFrequency=checkpoint.frequency;reminder.repeatInterval=checkpoint.interval
            timing.restoreDraft(checkpoint.timing);datesEdited=checkpoint.datesEdited
        }
        checkpoint=null;error="";opened=false;restoring=false
        if(apply)applied();else cancelled()
        if(entry.visible)entry.forceActiveFocus(Qt.PopupFocusReason)
    }
    function dateChanged(value){if(!restoring){datesEdited=true;datesChanged(value)}}
    function reminderChanged(){if(!restoring)edited()}
    function detailsPatch(zone){
        const patch=Object.assign(timing.detailsPatch(),reminder.detailsPatch(zone))
        // The arrangement derives the UTC instant for a relative reminder.
        if(timing.alertMinutes>=0)delete patch.remindAtLocal
        return patch
    }
    function setReminderMode(index){
        const wasRelative=timing.alertMinutes>=0
        if(index<2){reminder.text="";reminder.repeatFrequency="NONE";reminder.repeatInterval=1}
        if(index===1 && !startDate.text)startDate.text=endDate.text || workspace.dateOffset(0)
        const offset=index===1?offsets.value:-1
        if(timing.alertMinutes!==offset){timing.alertMinutes=offset;timing.changed(true)}
        if(index===2 && (wasRelative || !reminder.text))reminder.text=workspace.defaultReminderLocal()
    }

    ActionButton {
        id:entry;objectName:root.objectPrefix==="task" ? "editorTimeSummary" : root.objectPrefix+"Arrangement"
        visible:root.showEntry;Layout.fillWidth:true;symbol:root.opened?"up":"calendar";contentAlignment:Qt.AlignLeft;text:root.summary
        Accessible.name:qsTr("编辑安排：%1").arg(root.summary+(root.extras?" · "+root.extras:""))
        onClicked:if(root.opened)root.acceptEdits();else root.startEditing()
    }
    Label {visible:!root.opened && root.extras.length>0;text:root.extras;color:Theme.secondary;font.pixelSize:Theme.captionSize;Layout.fillWidth:true;wrapMode:Text.Wrap}
    ColumnLayout {
        objectName:root.objectPrefix==="task"?"editorDatesGrid":root.objectPrefix+"TemporalBody"
        visible:root.opened;Layout.fillWidth:true;spacing:root.spacing
        FormRow {id:startRow;contextual:true;label:root.endExpanded || timing.hasSpecificTime ? qsTr("开始") : qsTr("日期");minimumValueWidth:(timing.hasSpecificTime && !root.compact?280:140)*Theme.scale
            GridLayout {id:startValues;Layout.fillWidth:true;columns:!root.compact && width>=280*Theme.scale?2:1;columnSpacing:8;rowSpacing:4
                DateField {id:startDate;objectName:root.startName;workspace:root.workspace;Layout.fillWidth:true;dateLabel:qsTr("开始日期");inlineMode:root.inlinePickers;onInlinePickerRequested:root.pickerRequested(startDate,"date");onTextChanged:{
                    if(root.opened && !root.restoring && !root.endExpanded && endDate.text)endDate.text=text
                    root.dateChanged(text)
                }}
            }
        }
        ActionButton {objectName:root.objectPrefix+"ExpandEndDate";visible:!root.endExpanded && !timing.hasSpecificTime;flat:true;symbol:"plus";text:qsTr("结束或截止日期");onClicked:{root.endExpanded=true;endDate.forceActiveFocus()}}
        FormRow {id:endRow;visible:root.endExpanded || (!root.compact && timing.hasSpecificTime);contextual:true;label:startDate.text ? qsTr("结束") : qsTr("截止");minimumValueWidth:(timing.hasSpecificTime && !root.compact?280:140)*Theme.scale
            GridLayout {id:endValues;Layout.fillWidth:true;columns:!root.compact && width>=280*Theme.scale?3:1;columnSpacing:8;rowSpacing:4
                DateField {id:endDate;objectName:root.endName;workspace:root.workspace;Layout.fillWidth:true;dateLabel:qsTr("结束日期（可选）");placeholderText:qsTr("同一天");inlineMode:root.inlinePickers;onInlinePickerRequested:root.pickerRequested(endDate,"date");onTextChanged:{
                    if(root.opened && !root.restoring && text && text!==startDate.text)root.endExpanded=true
                    root.dateChanged(text)
                }}
            }
        }
        TaskTimingFields {
            id:timing;objectName:root.objectPrefix==="task"?"taskTimingFields":root.objectPrefix+"TimingFields"
            workspace:root.workspace;objectPrefix:root.objectPrefix;taskId:root.taskId;Layout.fillWidth:true
            compactPresentation:true;inlinePickers:root.inlinePickers;useTaskCycle:true
            datesEdited:root.datesEdited;startDateValue:startDate.text;endDateValue:endDate.text
            startEndpointHost:root.compact ? null : startValues;endEndpointHost:root.compact ? null : endValues
            onPickerRequested:function(field,kind){root.pickerRequested(field,kind)}
            onDraftChanged:root.edited()
            onDateDefaultsRequested:{if(!startDate.text)startDate.text=root.workspace.dateOffset(0);if(!endDate.text)endDate.text=startDate.text}
            onRangeDatesChosen:function(a,b){root.endExpanded=a!==b;startDate.text=a;endDate.text=b}
        }
        FormRow {contextual:true;label:qsTr("提醒")
            ColumnLayout {Layout.fillWidth:true;spacing:root.spacing
                DS.ComboBox {
                    id:reminderMode
                    objectName:root.objectPrefix+"ReminderMode";withinHostWindow:true;Layout.fillWidth:true
                    model:[qsTr("不提醒"),qsTr("随开始时间提醒"),qsTr("指定提醒时间")]
                    currentIndex:timing.alertMinutes>=0?1:reminder.text?2:0
                    onActivated:function(index){root.setReminderMode(index)}
                }
                RowLayout {visible:timing.alertMinutes>=0;Layout.fillWidth:true
                    Label {text:qsTr("提前")}
                    SpinBox {id:offsets;Layout.fillWidth:true;from:0;to:10080;stepSize:5
                        value:Math.max(0,timing.alertMinutes);editable:true;Accessible.name:qsTr("开始前分钟数，0 为开始时")
                        onValueModified:{timing.alertMinutes=value;timing.changed(true)}
                    }
                    Label {text:qsTr("分钟")}
                }
                Label {visible:timing.alertMinutes>=0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.secondary
                    text:!root.alarmPreview ? qsTr("请选择有效的开始日期和时间") : timing.hasSpecificTime
                        ? qsTr("%1 提醒（本地时间）").arg(root.alarmPreview)
                        : qsTr("%1 提醒 · 全天任务按 09:00 计算").arg(root.alarmPreview)
                }
                ReminderField {
                    id:reminder;objectName:root.reminderName;workspace:root.workspace;Layout.fillWidth:true
                    visible:!!text && timing.alertMinutes<0;inlineMode:root.inlinePickers;singleOccurrence:true
                    onInlinePickerRequested:root.pickerRequested(reminder,"reminder")
                    onTextChanged:root.reminderChanged();onRepeatFrequencyChanged:root.reminderChanged();onRepeatIntervalChanged:root.reminderChanged()
                }
                Label {visible:reminder.repeatFrequency!=="NONE";Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("原重复提醒 · %1").arg(reminder.repeatSummary);color:Theme.secondary}
                ActionButton {
                    visible:reminder.repeatFrequency!=="NONE" && !timing.repeatEnabled && !timing.legacyRule
                    Layout.fillWidth:true;symbol:"reload";contentAlignment:Qt.AlignLeft
                    text:qsTr("转为任务周期，保留此次提醒")
                    onClicked:if(timing.adoptReminderCadence(reminder.repeatFrequency,reminder.repeatInterval)){
                        reminder.repeatFrequency="NONE"
                        reminder.repeatInterval=1
                    }
                }
            }
        }
        Label {visible:!!root.error;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.error}
        RowLayout {visible:root.showActions;Layout.fillWidth:true;Item {Layout.fillWidth:true}
            Button {objectName:root.objectPrefix+"ArrangementCancel";text:qsTr("取消");onClicked:root.finish(false)}
            Button {objectName:root.objectPrefix+"ArrangementApply";text:root.persistOnApply?qsTr("保存安排"):qsTr("应用");enabled:!root.workspace.busy;highlighted:true;onClicked:root.acceptEdits()}
        }
    }
}
