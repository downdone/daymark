import QtQuick
import QtQuick.Controls

Item {
    id: root
    property string taskId: ""
    property real rowVersion: 0
    property string title: ""
    width: 220 * Theme.scale
    height: 32 * Theme.scale
    Rectangle {
        anchors.fill: parent
        radius: 5
        color: Theme.selected
        border.color: Theme.accent
    }
    Label {
        anchors.fill: parent
        anchors.margins: 5
        text: root.title
        elide: Text.ElideRight
        font.pixelSize: Theme.captionSize
    }
}
