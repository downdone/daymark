pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import DaymarkStyle as DS
import QtQuick.Templates as T
T.Switch {
    id: control
    property bool showSymbol: true
    property string symbol: "settings"
    readonly property real trackWidth: Math.min(48,36*Theme.scale)
    readonly property real trackHeight: Math.min(28,22*Theme.scale)
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(32,contentItem.implicitHeight+8,trackHeight+8)
    padding: 4
    rightPadding: trackWidth+12
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    Accessible.name: text
    contentItem: RowLayout {
        spacing: 8
        AppIcon {visible: control.showSymbol;implicitWidth: 16;implicitHeight: 16;name: control.symbol;ink: control.enabled ? Theme.secondary : Theme.disabledText}
        Text {text: control.text;font: control.font;color: control.enabled ? Theme.text : Theme.disabledText;Layout.fillWidth: true;wrapMode: Text.Wrap}
    }
    indicator: Rectangle {
        x: control.width-width-control.rightPadding+control.trackWidth+8
        y: (control.height-height)/2
        implicitWidth: control.trackWidth;implicitHeight: control.trackHeight
        radius: height/2
        color: !control.enabled ? Theme.separator : control.checked ? Theme.toggleOn : DS.DesignTokens.outline
        border.width: control.visualFocus ? 2 : 0;border.color: Theme.accent
        Rectangle {
            width: parent.height-6;height: width;radius: width/2
            x: control.checked ? parent.width-width-3 : 3;y: 3
            color: Theme.dark && control.checked ? Theme.accentText : Theme.content
        }
    }
    background: Rectangle {radius: 8;color: control.down || control.hovered ? Theme.hover : "transparent";border.width: control.visualFocus ? 2 : 0;border.color: Theme.accent}
}
