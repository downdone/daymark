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
    topPadding: 8
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    background: Rectangle {radius: Theme.panelRadius;color: Theme.dark ? Theme.grouped : Theme.content}
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {text: section.heading;font.pixelSize: Theme.captionSize;font.weight: Font.DemiBold;color: Theme.secondary;Layout.fillWidth: true;wrapMode: Text.Wrap}
            Label {visible: text.length>0;text: section.summary;font.pixelSize: Theme.captionSize;color: Theme.secondary}
        }
        Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator}
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: 8}
    }
}
