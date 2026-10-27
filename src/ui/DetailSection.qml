pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: section
    property string heading: ""
    property string summary: ""
    property string symbol: "list"
    property bool plain: false
    property bool divider: plain
    default property alias fields: fieldStack.data
    Layout.fillWidth: true
    padding: plain ? 0 : Theme.sectionPadding
    topPadding: plain ? 0 : Theme.sectionPadding
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {visible:!section.plain;radius: Theme.groupRadius;color: Theme.dark ? Theme.grouped : Theme.content}
    contentItem: ColumnLayout {
        spacing: Theme.fieldGap
        Rectangle {visible:section.divider;Layout.fillWidth:true;implicitHeight:1;color:Theme.separator;Layout.topMargin:4;Layout.bottomMargin:4}
        RowLayout {
            visible: section.heading.length>0
            Layout.fillWidth: true
            Label {text: section.heading;font.pixelSize: Theme.sectionSize;font.weight: Font.DemiBold;color: Theme.secondary;Layout.fillWidth: true;wrapMode: Text.Wrap}
            Label {visible: text.length>0;text: section.summary;font.pixelSize: Theme.captionSize;color: Theme.secondary}
        }
        Rectangle {visible: section.heading.length>0;Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator}
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: Theme.fieldGap}
    }
}
