pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    property string text: "09:00"
    signal accepted()
    function chooseTime(hour,minute){text=String(hour).padStart(2,"0")+":"+String(minute).padStart(2,"0")}
    onTextChanged: {if(/^([01]\d|2[0-3]):[0-5]\d$/.test(text)){hours.value=Number(text.slice(0,2));minutes.value=Number(text.slice(3,5))}}
    RowLayout {
        Layout.fillWidth: true;spacing: 8
        ColumnLayout {Layout.fillWidth: true;Label {text: qsTr("小时");font.pixelSize: Theme.captionSize;color: Theme.secondary} SpinBox {id: hours;objectName: "pickerHour";Layout.fillWidth: true;from: 0;to: 23;value: 9;editable: true;Accessible.name: qsTr("提醒小时");onValueModified: root.chooseTime(value,minutes.value)}}
        Label {text: ":"}
        ColumnLayout {Layout.fillWidth: true;Label {text: qsTr("分钟");font.pixelSize: Theme.captionSize;color: Theme.secondary} SpinBox {id: minutes;objectName: "pickerMinute";Layout.fillWidth: true;from: 0;to: 59;editable: true;Accessible.name: qsTr("提醒分钟");textFromValue: function(value,locale){return String(value).padStart(2,"0")};onValueModified: root.chooseTime(hours.value,value)}}
    }
}
