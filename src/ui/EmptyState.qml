pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: state
    property string symbol: "inbox"
    required property string heading
    property string detail: ""
    property string actionText: ""
    property alias actionObjectName: action.objectName
    signal triggered()
    padding: 24
    implicitHeight: implicitContentHeight+48
    contentItem: ColumnLayout {
        spacing: 12
        AppIcon {name: state.symbol;ink: Theme.secondary;Layout.alignment: Qt.AlignHCenter;Layout.preferredWidth: 28;Layout.preferredHeight: 28}
        Label {text: state.heading;font.pixelSize: Theme.headingSize;font.weight: Font.DemiBold;horizontalAlignment: Text.AlignHCenter;Layout.fillWidth: true;wrapMode: Text.Wrap}
        Label {visible: state.detail.length>0;text: state.detail;color: Theme.secondary;horizontalAlignment: Text.AlignHCenter;Layout.fillWidth: true;wrapMode: Text.Wrap}
        Button {id: action;visible: state.actionText.length>0;text: state.actionText;Layout.alignment: Qt.AlignHCenter;onClicked: state.triggered()}
    }
}
