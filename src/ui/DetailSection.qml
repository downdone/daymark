pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: section
    property string heading: ""
    property string summary: ""
    property string symbol: "list"
    default property alias fields: fieldStack.data
    Layout.fillWidth: true
    padding: 12
    topPadding: 12
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {radius: 16;color: Theme.dark ? Theme.grouped : Theme.content}
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Rectangle {implicitWidth: 26;implicitHeight: 26;radius: 7;color: Theme.selected;AppIcon {anchors.centerIn: parent;width: 17;height: 17;name: section.symbol;ink: Theme.accent}}
            Label {text: section.heading;font.pixelSize: Theme.sectionSize;font.weight: Font.DemiBold;Layout.fillWidth: true;wrapMode: Text.Wrap}
            Label {visible: text.length>0;text: section.summary;font.pixelSize: Theme.captionSize;color: Theme.secondary}
        }
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: 8}
    }
}
