pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
Button {
    id: control
    required property string symbol
    property int contentAlignment: Qt.AlignHCenter
    readonly property bool textTruncated: label.truncated
    readonly property real glyphSize: Math.min(24,16*Theme.scale)
    readonly property color foreground: !enabled ? Theme.disabledText : highlighted ? Theme.accentText : palette.buttonText
    flat: true
    Accessible.name: text
    contentItem: Item {
        implicitWidth: control.glyphSize+6+label.implicitWidth
        implicitHeight: Math.max(control.glyphSize,label.implicitHeight)
        Row {
            anchors.verticalCenter: parent.verticalCenter
            x:control.contentAlignment===Qt.AlignLeft ? 0 : Math.max(0,(parent.width-width)/2)
            spacing: 6
            AppIcon {name: control.symbol;ink: control.foreground;width: control.glyphSize;height: width;anchors.verticalCenter: parent.verticalCenter}
            Text {id: label;text: control.text;font: control.font;color: control.foreground;textFormat: Text.PlainText;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter;width: Math.max(0,Math.min(implicitWidth,control.availableWidth-control.glyphSize-6))}
        }
    }
}
