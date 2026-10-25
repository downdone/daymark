pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: field
    required property TaskWorkspace workspace
    property string objectPrefix: ""
    property bool iconOnly: false
    property bool commitTask: false
    property bool inlineMode: false
    property bool inlineExpanded:false
    signal inlinePickerRequested()
    property string repeatFrequency: "NONE"
    property int repeatInterval: 1
    readonly property var frequencies: ["NONE","DAILY","WEEKLY","MONTHLY","YEARLY"]
    readonly property string repeatSummary: repeatFrequency==="NONE" ? "" : qsTr("每 %1 %2提醒").arg(repeatInterval).arg(["",qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年")][frequencies.indexOf(repeatFrequency)] || "")
    readonly property bool pickerVisible: picker.visible || date.pickerVisible || inlineExpanded
    readonly property var hostWindow: Window.window
    signal reminderChosen()
    placeholderText: "yyyy-MM-dd HH:mm"
    selectByMouse: true
    Accessible.name: qsTr("提醒，本地日期和时间，留空清除")
    rightPadding: bell.width+8
    function openPicker() {
        if(inlineMode){inlineExpanded=true;inlinePickerRequested();return}
        date.text=text.length>=10 ? text.slice(0,10) : workspace.defaultReminderLocal().slice(0,10)
        time.text=text.length>=16 ? text.slice(11,16) : workspace.defaultReminderLocal().slice(11,16)
        frequency.currentIndex=Math.max(0,frequencies.indexOf(repeatFrequency));interval.value=repeatInterval
        picker.open()
    }
    function closePicker(){inlineExpanded=false;date.closePicker();picker.close()}
    function applySelection(){text=date.text+" "+time.text;repeatFrequency=frequencies[frequency.currentIndex];repeatInterval=interval.value;reminderChosen();closePicker();forceActiveFocus()}
    onVisibleChanged: if(!visible && !inlineExpanded)closePicker()
    Keys.onPressed: function(event){if(event.key===Qt.Key_Down && (event.modifiers & Qt.AltModifier)){openPicker();event.accepted=true}}
    IconToolButton {
        id: bell
        anchors.right: parent.right;anchors.verticalCenter: parent.verticalCenter
        width: field.iconOnly ? field.width : Math.max(32,Theme.bodySize+16);height: field.iconOnly ? field.height : field.height-4
        contentItem: AppIcon {name: "bell"}
        Accessible.name: qsTr("选择提醒日期和时间")
        onClicked: picker.visible ? field.closePicker() : field.openPicker()
    }
    Popup {
        id: picker
        objectName: field.objectPrefix+"reminderPickerPopup"
        onAboutToShow:{const at=field.mapToItem(null,0,0);x=Math.max(8-at.x,Math.min(0,(field.hostWindow ? field.hostWindow.width : 1120)-at.x-width-8));y=Math.max(8-at.y,Math.min(field.height+4,(field.hostWindow ? field.hostWindow.height : 720)-at.y-height-8))}
        x: Math.min(0,field.width-width);y: field.height+4
        width: Math.min(Math.max(320,320*Theme.scale),field.hostWindow ? field.hostWindow.width-32 : 640)
        height: Math.min(implicitHeight,field.hostWindow ? field.hostWindow.height-24 : 640)
        padding: 12;margins: 8;focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: {date.closePicker()}
        background: Rectangle {radius: Theme.panelRadius;color: Theme.content;border.color: Theme.separator}
        contentItem: ColumnLayout {
            spacing: 8
            ScrollView {
                Layout.fillWidth: true;Layout.fillHeight: true;implicitHeight: fields.implicitHeight;clip: true
                ColumnLayout {
                    id: fields;width: picker.availableWidth;spacing: 8
                    Label {text: qsTr("提醒 · 本地时间");font.weight: Font.DemiBold}
                    Label {visible: field.commitTask;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("完成后保存此提醒和当前任务修改。")}
                    DateField {id: date;objectName: field.objectPrefix+"reminderPickerDate";Layout.fillWidth: true;workspace: field.workspace;inlineMode:true;dateLabel: qsTr("提醒日期")}
                    CalendarPicker {Layout.fillWidth:true;visible:date.inlineExpanded;workspace:field.workspace;month:date.month;selectedDate:date.text;onDateSelected:function(value){date.chooseDate(value)} onMoveMonth:function(offset){date.month=field.workspace.datePicker(date.month.anchor,offset)} onDismissRequested:date.closePicker()}
                    InlineClock {id: time;objectName: field.objectPrefix+"reminderPickerTime";Layout.fillWidth: true;onAccepted: field.applySelection()}
                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary;text:field.workspace.reminderPreview(date.text+" "+time.text,field.frequencies[frequency.currentIndex],interval.value)}
                    Label {text: qsTr("重复提醒此任务");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    ComboBox {id: frequency;objectName: field.objectPrefix+"reminderRepeatFrequency";Layout.fillWidth: true;model: [qsTr("仅一次"),qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年")]}
                    RowLayout {visible: frequency.currentIndex>0
                        Label {text: qsTr("每隔")}
                        SpinBox {id: interval;objectName: field.objectPrefix+"reminderRepeatInterval";from: 1;to: 36500;editable: true;Accessible.name: qsTr("提醒循环间隔")}
                        Label {text: ["",qsTr("天"),qsTr("周"),qsTr("月"),qsTr("年")][frequency.currentIndex]}
                    }
                    Label {visible: frequency.currentIndex>0;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("重复提示同一任务，完成或暂停后停止；应用需保持运行。")}
                }
            }
            Flow {
                Layout.fillWidth: true;spacing: 6
                Button {text: qsTr("清除");onClicked: {field.text="";field.repeatFrequency="NONE";field.repeatInterval=1;field.reminderChosen();field.closePicker()}}
                Button {text: qsTr("取消");onClicked: field.closePicker()}
                Button {objectName: field.objectPrefix+"applyReminder";text: field.commitTask ? qsTr("完成并保存") : qsTr("完成");highlighted: true;onClicked: field.applySelection()}
            }
        }
    }
}
