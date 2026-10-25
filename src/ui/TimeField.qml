pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: field
    property string timeLabel: qsTr("时间")
    readonly property bool pickerVisible: picker.visible
    readonly property var hostWindow: Window.window
    signal timeChosen()
    placeholderText: "HH:mm"
    Accessible.name: timeLabel
    selectByMouse: true
    rightPadding: clockButton.width+8
    function openPicker() {
        const valid=/^([01]\d|2[0-3]):[0-5]\d$/.test(text)
        hours.value=valid ? Number(text.slice(0,2)) : 9
        minutes.value=valid ? Number(text.slice(3,5)) : 0
        picker.open();forceActiveFocus()
    }
    function chooseTime(hour,minute) {
        text=String(hour).padStart(2,"0")+":"+String(minute).padStart(2,"0")
        timeChosen();picker.close();forceActiveFocus()
    }
    function closePicker(){picker.close()}
    onVisibleChanged: if(!visible)closePicker()
    Keys.onEscapePressed: function(event){if(picker.visible)closePicker();else event.accepted=false}
    Keys.onReturnPressed: function(event){if(picker.visible)closePicker();else event.accepted=false}
    Keys.onPressed: function(event){if(event.key===Qt.Key_Down && (event.modifiers & Qt.AltModifier)){openPicker();event.accepted=true}}
    TapHandler {onTapped: field.openPicker()}
    IconToolButton {
        id: clockButton
        anchors.right: parent.right;anchors.verticalCenter: parent.verticalCenter
        width: Math.max(32,Theme.bodySize+16);height: field.height-4
        contentItem: AppIcon {name: "clock"}
        Accessible.name: qsTr("选择%1").arg(field.timeLabel)
        onClicked: field.pickerVisible ? field.closePicker() : field.openPicker()
    }
    Popup {
        id: picker
        objectName: "timePickerPopup"
        x: Math.min(0,field.width-width)
        y: field.height+4
        width: Math.min(Math.max(280,280*Theme.scale),field.hostWindow ? field.hostWindow.width-32 : 560)
        padding: 12;margins: 8;focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {radius: Theme.panelRadius;color: Theme.content;border.color: Theme.separator}
        contentItem: ColumnLayout {
            spacing: 8
            Label {text: field.timeLabel;font.weight: Font.DemiBold}
            RowLayout {
                Layout.fillWidth: true
                ColumnLayout {Layout.fillWidth: true;Label {text: qsTr("小时");color: Theme.secondary} SpinBox {id: hours;objectName: "pickerHour";Layout.fillWidth: true;from: 0;to: 23;editable: true;Accessible.name: qsTr("小时")}}
                Label {text: ":"}
                ColumnLayout {Layout.fillWidth: true;Label {text: qsTr("分钟");color: Theme.secondary} SpinBox {id: minutes;textFromValue: function(value,locale){return String(value).padStart(2,"0")};objectName: "pickerMinute";Layout.fillWidth: true;from: 0;to: 59;editable: true;Accessible.name: qsTr("分钟")}}
            }
            Flow {
                Layout.fillWidth: true;spacing: 6
                Button {text: qsTr("取消");onClicked: field.closePicker()}
                Button {objectName: "applyTime";text: qsTr("使用时间");highlighted: true;onClicked: field.chooseTime(hours.value,minutes.value)}
            }
        }
    }
}
