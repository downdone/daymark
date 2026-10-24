pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: group
    property string heading: ""
    property string detail: ""
    property bool compact: false
    default property alias fields: fieldStack.data
    Layout.fillWidth: true
    padding: compact ? 12 : 16
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {color: Theme.dark ? Theme.grouped : Theme.content;radius: 16}
    contentItem: ColumnLayout {
        spacing: Theme.fieldGap
        Label {visible: group.heading.length>0;text: group.heading;font.pixelSize: group.compact ? Theme.captionSize : Theme.sectionSize;font.weight: Font.DemiBold;Layout.fillWidth: true;wrapMode: Text.Wrap}
        Label {visible: group.detail.length>0;text: group.detail;color: Theme.secondary;font.pixelSize: Theme.captionSize;Layout.fillWidth: true;wrapMode: Text.Wrap}
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: Theme.fieldGap}
    }
}
