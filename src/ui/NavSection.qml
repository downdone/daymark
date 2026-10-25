pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: root
    required property string title
    property bool expanded: true
    property bool containsSelection: false
    property bool showActions: false
    property bool showManageAction: false
    signal optionsRequested()
    property bool actionsSelected: false
    property string createDescription: qsTr("新建分类")
    property string manageDescription: qsTr("管理分类")
    signal createRequested()
    signal manageRequested()
    signal expansionRequested(bool value)
    default property alias entries: body.data
    spacing: 0
    Layout.fillWidth: true
    Layout.topMargin: 4
    RowLayout {
        Layout.fillWidth: true;spacing: 0
    ItemDelegate {
        objectName: root.objectName+"Header"
        Layout.fillWidth: true
        implicitHeight: Math.max(28,contentItem.implicitHeight+6)
        leftPadding: 10;rightPadding: 10
        Accessible.name: root.title+(root.expanded ? qsTr("，已展开") : qsTr("，已折叠"))
        Accessible.role: Accessible.Button
        onClicked: root.expansionRequested(!root.expanded)
        Keys.onLeftPressed: root.expansionRequested(false)
        Keys.onRightPressed: root.expansionRequested(true)
        TapHandler {acceptedButtons: Qt.RightButton;onTapped: root.optionsRequested()}
        Keys.onPressed: function(event){if(event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))){root.optionsRequested();event.accepted=true}}
        contentItem: RowLayout {
            spacing: 6
            Label {Layout.fillWidth: true;text: root.title;font.pixelSize: Theme.captionSize;font.weight: Font.DemiBold;color: root.containsSelection && !root.expanded ? Theme.accent : Theme.secondary;elide: Text.ElideRight}
            Rectangle {visible: root.containsSelection && !root.expanded;implicitWidth: 5;implicitHeight: 5;radius: 3;color: Theme.accent}
            AppIcon {name: root.expanded ? "down" : "right";implicitWidth: 12;implicitHeight: 12;ink: Theme.secondary}
        }
    }
    WidgetAction {objectName: root.objectName+"Create";visible: root.showActions;symbol: "plus";description: root.createDescription;onClicked: root.createRequested()}
    WidgetAction {objectName: root.objectName+"Manage";visible: root.showActions && root.showManageAction;symbol: "more";checked: root.actionsSelected;description: root.manageDescription;onClicked: root.manageRequested()}
    }
    ColumnLayout {id: body;Layout.fillWidth: true;visible: root.expanded;spacing: 0}
}
