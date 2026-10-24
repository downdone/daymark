pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: section
    property string heading: ""
    property string summary: ""
    default property alias fields: fieldStack.data
    Layout.fillWidth: true
    padding: 0
    topPadding: 12
    implicitHeight: implicitContentHeight+topPadding
    background: Item {Rectangle {width: parent.width;height: 1;color: Theme.separator}}
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {text: section.heading;font.pixelSize: Theme.sectionSize;font.weight: Font.DemiBold;Layout.fillWidth: true;wrapMode: Text.Wrap}
            Label {visible: text.length>0;text: section.summary;font.pixelSize: Theme.captionSize;color: Theme.secondary}
        }
        ColumnLayout {id: fieldStack;Layout.fillWidth: true;spacing: 8}
    }
}
