pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

NavRow {
    id: root
    property color tint: Theme.accent
    implicitHeight: Math.max(Theme.smartTileHeight,contentItem.implicitHeight+(Theme.compact ? 8 : 12))
    padding: Theme.compact ? 8 : 10;leftPadding:10;rightPadding:10
    Accessible.name: text+qsTr("，%1 项").arg(count)
    contentItem: ColumnLayout {
        spacing: 4
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                implicitWidth: Math.min(24,20*Theme.scale);implicitHeight: implicitWidth;radius: width/2
                color: root.tint
                AppIcon {anchors.centerIn: parent;width: Math.min(20,14*Theme.scale);height: width;name: root.symbol;ink: Theme.accentText}
            }
            Item {Layout.fillWidth: true}
            Label {text: root.count;font.pixelSize: Math.round(16*Theme.scale);font.weight: Font.DemiBold;color: (root.selected || root.taskDropActive) ? root.tint : Theme.text}
        }
        Label {objectName:"smartNavigationTitle";text: root.text;font.pixelSize: Theme.captionSize;font.weight: root.selected ? Font.DemiBold : Font.Normal;color: (root.selected || root.taskDropActive) ? root.tint : Theme.text;Layout.fillWidth: true;elide: Text.ElideRight}
    }
    background: Rectangle {
        radius: Theme.rowRadius
        color: root.down ? Theme.pressed : (root.selected || root.taskDropActive) ? Theme.selected : root.hovered ? Theme.hover : Theme.grouped
        border.color: Theme.accent;border.width: root.visualFocus || root.taskDropActive ? 1 : 0
    }
}
