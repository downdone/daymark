pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
    id: root
    property string symbol: "settings"
    property string detail: ""
    property bool separator:true
    property bool disclosure:true
    property bool destructive:false
    property color iconColor:Theme.accent
    Layout.fillWidth:true
    implicitHeight:Math.max(44,contentItem.implicitHeight+16)
    padding:12
    Accessible.description:detail
    contentItem:RowLayout {
        spacing:10
        Rectangle {
            Layout.preferredWidth:28;Layout.preferredHeight:28;radius:Theme.controlRadius
            color:Theme.highContrast ? Theme.hover : Qt.rgba(root.iconColor.r,root.iconColor.g,root.iconColor.b,Theme.dark ? 0.2 : 0.1)
            AppIcon {anchors.centerIn:parent;width:18;height:18;name:root.symbol;ink:root.destructive ? Theme.danger : root.iconColor}
        }
        ColumnLayout {
            Layout.fillWidth:true;spacing:2
            Label {text:root.text;Layout.fillWidth:true;font.pixelSize:Theme.bodySize;color:!root.enabled ? Theme.disabledText : root.destructive ? Theme.danger : Theme.text;wrapMode:Text.Wrap}
            Label {visible:!!root.detail;text:root.detail;Layout.fillWidth:true;font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap}
        }
        AppIcon {visible:root.disclosure && !root.destructive;name:"right";ink:Theme.secondary;Layout.preferredWidth:14;Layout.preferredHeight:14}
    }
    background:Rectangle {radius:Theme.menuRadius;color:root.down ? Theme.pressed : root.hovered ? Theme.hover : "transparent";border.width:root.visualFocus ? 1 : 0;border.color:Theme.accent}
    Rectangle {visible:root.separator;x:50;y:root.height-1;width:root.width-62;height:1;color:Theme.separator}
}
