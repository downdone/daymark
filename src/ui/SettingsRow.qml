pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Control {
    id: root
    objectName:"desktopFormRow"
    required property string label
    property bool separator: true
    property real minimumValueWidth: 180*Theme.scale
    readonly property bool stacked: availableWidth<120*Theme.scale+minimumValueWidth+12
    default property alias controls: values.data
    Layout.fillWidth:true
    leftPadding:12;rightPadding:12;topPadding:4;bottomPadding:4
    implicitHeight:Math.max(40,implicitContentHeight+topPadding+bottomPadding)
    function verifyLayout() {
        if(width<=0 || height<=0 || values.width<=0 || values.height<=0)return "empty settings row";
        if(values.x<0 || values.x+values.width>availableWidth+1)return "settings value outside row";
        if(stacked ? labelItem.y+labelItem.height>values.y+1 : labelItem.x+labelItem.width>values.x+1)return "settings label overlaps value";
        return "PASS";
    }
    contentItem:GridLayout {
        columns:root.stacked ? 1 : 2;columnSpacing:12;rowSpacing:2
        Label {
            id:labelItem
            text:root.label;Layout.preferredWidth:root.stacked ? -1 : 120*Theme.scale;Layout.fillWidth:root.stacked
            color:root.enabled ? Theme.text : Theme.disabledText;font.pixelSize:Theme.bodySize;wrapMode:Text.Wrap
        }
        ColumnLayout {id:values;Layout.fillWidth:true;spacing:2}
    }
    Rectangle {visible:root.separator;x:12;y:parent.height-1;width:parent.width-24;height:1;color:Theme.separator}
}
