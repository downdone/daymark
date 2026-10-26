pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: group
    property string heading: ""
    property string detail: ""
    property bool compact: true
    default property alias fields: fieldStack.data
    Layout.fillWidth: true
    padding: Theme.sectionPadding
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {color: Theme.grouped;radius: Theme.panelRadius}
    contentItem: ColumnLayout {
        spacing: Theme.groupGap
        Label {visible: group.heading.length>0;text: group.heading;font.pixelSize: group.compact ? Theme.captionSize : Theme.sectionSize;font.weight: Font.DemiBold;Layout.fillWidth: true;wrapMode: Text.Wrap}
        Label {visible: group.detail.length>0;text: group.detail;color: Theme.secondary;font.pixelSize: Theme.captionSize;Layout.fillWidth: true;wrapMode: Text.Wrap}
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: Theme.fieldGap}
    }
}
