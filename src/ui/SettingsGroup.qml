pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string heading: ""
    property string detail: ""
    default property alias rows: stack.data
    Layout.fillWidth:true
    spacing:Theme.fieldGap
    Label {
        visible:root.heading.length>0;Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12
        text:root.heading;color:Theme.secondary;font.pixelSize:Theme.sectionSize;wrapMode:Text.Wrap
    }
    Control {
        Layout.fillWidth:true;padding:0
        implicitHeight:implicitContentHeight
        background:Rectangle {radius:Theme.groupRadius;color:Theme.content;border.width:Theme.highContrast ? 1 : 0;border.color:Theme.separator}
        contentItem:ColumnLayout {id:stack;spacing:0}
    }
    Label {
        visible:root.detail.length>0;Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12
        text:root.detail;color:Theme.secondary;font.pixelSize:Theme.captionSize;wrapMode:Text.Wrap
    }
}
