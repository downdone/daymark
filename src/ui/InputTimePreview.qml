pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    required property TaskWorkspace workspace
    property string rawText: ""
    property bool composing: false
    property bool ignored: false
    property var overrides: ({})
    property string objectPrefix: ""
    property int clockRevision:0
    property var pickerMonth:({title:"",days:[]})
    readonly property var suggestion: {const revision=clockRevision;const preferences=workspace.preferences;return workspace.previewTaskInput(rawText)}
    Timer {interval:1000;repeat:true;running:root.visible && !root.ignored && !root.manual && !root.composing;onTriggered:root.clockRevision++}
    readonly property bool manual: Object.keys(overrides).length>0
    readonly property bool hasTime: !ignored && (manual || suggestion.state==="ready")
    readonly property bool popupOpen: picker.visible
    readonly property string summary: ignored ? qsTr("本条不识别时间") : manual ? (overrides.target==="deadline"?qsTr("截止 "):qsTr("计划 "))+overrides.date+(overrides.time ? " "+overrides.time : "")+(overrides.reminder?qsTr(" · 届时提醒"):qsTr(" · 不提醒")) : suggestion.summary || ""
    Layout.fillWidth: true
    padding:0
    implicitHeight:implicitContentHeight
    visible: rawText.trim().length>0 && !composing && summary.length>0
    onRawTextChanged: if(!rawText.length){ignored=false;overrides={};picker.close()}
    function openEditor() {
        const value=manual?overrides:suggestion
        date.text=value.date || workspace.dateOffset(0)
        pickerMonth=workspace.datePicker(date.text,0)
        time.text=value.time || workspace.preferences.recognitionReminderTime
        target.currentIndex=value.target==="deadline"?1:0
        remind.checked=!!value.reminder
        picker.open()
    }
    contentItem: RowLayout {
        spacing:4
        Button {
            id:suggestionButton;objectName:root.objectPrefix+"timeSuggestion"
            Layout.fillWidth:true;Layout.minimumWidth:0
            text:root.summary;flat:true
            contentItem:Label {text:suggestionButton.text;wrapMode:Text.Wrap;color:root.suggestion.state==="choice" && !root.manual && !root.ignored ? Theme.secondary : Theme.accent;font.pixelSize:Theme.captionSize}
            Accessible.name:root.summary+qsTr("，修改时间")
            onClicked:{root.ignored=false;root.openEditor()}
        }
        Button {
            objectName:root.objectPrefix+"ignoreTime"
            text:root.ignored?qsTr("恢复识别"):qsTr("忽略")
            flat:true;Accessible.name:root.ignored?qsTr("恢复本条时间识别"):qsTr("忽略本条时间识别")
            onClicked:{root.ignored=!root.ignored;if(root.ignored)root.overrides={}}
        }
    }
    Popup {
        id:picker
        popupType:Popup.Window
        objectName:root.objectPrefix+"timePicker"
        parent:Overlay.overlay
        x:Math.max(8,Math.min(root.mapToItem(parent,0,0).x,parent.width-width-8))
        y:Math.max(8,Math.min(root.mapToItem(parent,0,root.height).y,parent.height-height-8))
        width:Math.min(root.workspace.reminderPopupArea().width-24,340*Theme.scale)
        height:Math.min(root.workspace.reminderPopupArea().height-24,implicitHeight)
        background:PopoverSurface {}
        modal:true;focus:true;closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
        onClosed:{date.closePicker();time.closePicker()}
        contentItem:DS.ScrollView {
            implicitHeight:fields.implicitHeight
            clip:true
            contentWidth:availableWidth
            ColumnLayout {
                id:fields;width:parent.width;spacing:8
                Label {text:qsTr("本条任务的时间");font.weight:Font.Bold;Layout.fillWidth:true;wrapMode:Text.Wrap}
                ComboBox {id:target;objectName:root.objectPrefix+"timeTarget";Layout.fillWidth:true;model:[qsTr("计划日期"),qsTr("截止日期")];Accessible.name:qsTr("时间含义");onActivated:if(currentIndex===1)remind.checked=false}
                DateField {id:date;objectName:root.objectPrefix+"timeDate";visible:false;workspace:root.workspace;allowClear:false}
                CalendarPicker {
                    Layout.fillWidth:true;workspace:root.workspace;month:root.pickerMonth
                    selectedDate:date.text;allowClear:false;showShortcuts:false
                    onDateSelected:function(value){date.text=value}
                    onMoveMonth:function(offset){root.pickerMonth=root.workspace.datePicker(root.pickerMonth.anchor,offset)}
                    onDismissRequested:picker.close()
                }
                CheckBox {id:remind;objectName:root.objectPrefix+"timeRemind";text:qsTr("到时提醒");enabled:target.currentIndex===0;Layout.fillWidth:true}
                TimeField {id:time;objectName:root.objectPrefix+"timeClock";Layout.fillWidth:true;visible:remind.checked && target.currentIndex===0;timeLabel:qsTr("提醒时刻")}
                Button {
                    objectName:root.objectPrefix+"applyTime";text:qsTr("应用");Layout.fillWidth:true
                    onClicked:{root.overrides={date:date.text,time:remind.checked && target.currentIndex===0?time.text:"",target:target.currentIndex===1?"deadline":"planned",reminder:remind.checked && target.currentIndex===0};root.ignored=false;picker.close()}
                }
            }
        }
    }
}
