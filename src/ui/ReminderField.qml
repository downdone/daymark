pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: field
    required property TaskWorkspace workspace
    property bool iconOnly: false
    property bool commitTask: false
    readonly property bool pickerVisible: picker.visible || date.pickerVisible
    readonly property var hostWindow: Window.window
    signal reminderChosen()
    placeholderText: "yyyy-MM-dd HH:mm"
    selectByMouse: true
    Accessible.name: qsTr("提醒，本地日期和时间，留空清除")
    rightPadding: bell.width+8
    function openPicker() {
        date.text=text.length>=10 ? text.slice(0,10) : workspace.dateOffset(0)
        time.text=text.length>=16 ? text.slice(11,16) : "09:00"
        picker.open()
    }
    function closePicker(){date.closePicker();picker.close()}
    function applySelection(){text=date.text+" "+time.text;reminderChosen();closePicker();forceActiveFocus()}
    onVisibleChanged: if(!visible)closePicker()
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
        objectName: "reminderPickerPopup"
        x: Math.min(0,field.width-width);y: field.height+4
        width: Math.min(Math.max(320,320*Theme.scale),field.hostWindow ? field.hostWindow.width-32 : 640)
        height: Math.min(implicitHeight,field.hostWindow ? field.hostWindow.height-24 : 640)
        padding: 12;margins: 8;focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: {date.closePicker()}
        background: Rectangle {radius: 16;color: Theme.content;border.color: Theme.separator}
        contentItem: ColumnLayout {
            spacing: 8
            ScrollView {
                Layout.fillWidth: true;Layout.fillHeight: true;implicitHeight: fields.implicitHeight;clip: true
                ColumnLayout {
                    id: fields;width: picker.availableWidth;spacing: 8
                    Label {text: qsTr("提醒 · 本地时间");font.weight: Font.DemiBold}
                    Label {visible: field.commitTask;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("完成后保存此提醒和当前任务修改。")}
                    DateField {id: date;objectName: "reminderPickerDate";Layout.fillWidth: true;workspace: field.workspace;dateLabel: qsTr("提醒日期")}
                    InlineClock {id: time;objectName: "reminderPickerTime";Layout.fillWidth: true;onAccepted: field.applySelection()}
                }
            }
            Flow {
                Layout.fillWidth: true;spacing: 6
                Button {text: qsTr("清除");onClicked: {field.text="";field.reminderChosen();field.closePicker()}}
                Button {text: qsTr("取消");onClicked: field.closePicker()}
                Button {objectName: "applyReminder";text: field.commitTask ? qsTr("完成并保存") : qsTr("完成");highlighted: true;onClicked: field.applySelection()}
            }
        }
    }
}
