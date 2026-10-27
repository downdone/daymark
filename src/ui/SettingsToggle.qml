import QtQuick
import QtQuick.Layouts

SettingSwitch {
    id: root
    property bool separator: true
    Layout.fillWidth:true
    showSymbol:false
    leftPadding:12;rightPadding:trackWidth+24
    implicitHeight:Math.max(40,contentItem.implicitHeight+16,trackHeight+16)
    Rectangle {visible:root.separator;x:12;y:root.height-1;width:root.width-24;height:1;color:Theme.separator}
}
