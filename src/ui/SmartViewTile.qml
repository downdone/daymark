pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

NavRow {
    id: root
    property color tint: Theme.accent
    implicitHeight: Math.max(Theme.smartTileHeight,contentItem.implicitHeight+(Theme.compact ? 8 : 12))
    padding: Theme.compact ? 4 : 6
    Accessible.name: text+qsTr("，%1 项").arg(count)
    contentItem: ColumnLayout {
        spacing: 2
        RowLayout {
            Layout.fillWidth: true
            Rectangle {
                implicitWidth: Math.min(30,22*Theme.scale);implicitHeight: implicitWidth;radius: width/2
                color: (root.selected || root.taskDropActive) ? Theme.accentText : root.tint
                AppIcon {anchors.centerIn: parent;width: Math.min(20,14*Theme.scale);height: width;name: root.symbol;ink: (root.selected || root.taskDropActive) ? root.tint : Theme.content}
            }
            Item {Layout.fillWidth: true}
            Label {text: root.count;font.pixelSize: Math.round(16*Theme.scale);font.weight: Font.DemiBold;color: (root.selected || root.taskDropActive) ? Theme.accentText : Theme.text}
        }
        Label {text: root.text;font.pixelSize: Theme.navigationSize;font.weight: Font.DemiBold;color: (root.selected || root.taskDropActive) ? Theme.accentText : Theme.text;Layout.fillWidth: true;elide: Text.ElideRight}
    }
    background: Rectangle {
        radius: Theme.rowRadius
        color: (root.selected || root.taskDropActive) ? root.tint : root.hovered ? Theme.hover : Theme.grouped
        border.color: Theme.accent;border.width: root.visualFocus || root.taskDropActive ? 1 : 0
    }
}
