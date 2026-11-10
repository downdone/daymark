pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

NavRow {
    id: root
    property color tint: Theme.accent
    implicitHeight: Math.max(Theme.smartTileHeight,contentItem.implicitHeight+topPadding+bottomPadding)
    padding: Theme.compact ? 4 : 8;leftPadding:8;rightPadding:8
    Accessible.name: text+qsTr("，%1 项").arg(count)
    contentItem: ColumnLayout {
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                implicitWidth: Math.min(28,22*Theme.scale);implicitHeight: implicitWidth;radius: width/2
                color: root.tint
                AppIcon {anchors.centerIn: parent;width: Math.min(20,13*Theme.scale);height: width;name: root.symbol;ink: Theme.accentText}
            }
            Item {Layout.fillWidth: true}
            Label {text: root.count;font.pixelSize: Theme.smartCountSize;font.weight: Font.Medium;color: Theme.text}
        }
        Label {objectName:"smartNavigationTitle";text: root.text;font.pixelSize: Theme.navigationSize;font.weight: root.selected ? Font.DemiBold : Font.Normal;color: Theme.text;Layout.fillWidth: true;elide: Text.ElideRight}
    }
    background: Rectangle {
        radius: Theme.groupRadius
        color: root.down ? Theme.pressed : root.taskDropActive ? Theme.selected : root.selected ? Theme.navigationSelection : root.hovered ? Theme.hover : Theme.smartFill
        border.color: Theme.accent;border.width: root.visualFocus || root.taskDropActive ? 1 : 0
    }
}
