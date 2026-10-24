pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    required property string title
    property bool expanded: true
    property bool containsSelection: false
    signal expansionRequested(bool value)
    default property alias entries: body.data
    spacing: 2
    Layout.fillWidth: true
    Layout.topMargin: 8
    ItemDelegate {
        objectName: root.objectName+"Header"
        Layout.fillWidth: true
        implicitHeight: Math.max(30,contentItem.implicitHeight+10)
        leftPadding: 10;rightPadding: 10
        Accessible.name: root.title+(root.expanded ? qsTr("，已展开") : qsTr("，已折叠"))
        Accessible.role: Accessible.Button
        onClicked: root.expansionRequested(!root.expanded)
        Keys.onLeftPressed: root.expansionRequested(false)
        Keys.onRightPressed: root.expansionRequested(true)
        contentItem: RowLayout {
            spacing: 6
            Label {Layout.fillWidth: true;text: root.title;font.pixelSize: Theme.captionSize;font.weight: Font.DemiBold;color: root.containsSelection && !root.expanded ? Theme.accent : Theme.secondary;elide: Text.ElideRight}
            Rectangle {visible: root.containsSelection && !root.expanded;implicitWidth: 5;implicitHeight: 5;radius: 3;color: Theme.accent}
            AppIcon {name: root.expanded ? "down" : "right";implicitWidth: 12;implicitHeight: 12;ink: Theme.secondary}
        }
    }
    ColumnLayout {id: body;Layout.fillWidth: true;visible: root.expanded;spacing: 2}
}
