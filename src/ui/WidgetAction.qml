import QtQuick
import QtQuick.Controls

ToolButton {
    id: action
    property string symbol: "more"
    required property string description
    readonly property real glyphSize: Math.min(24,16*Theme.scale)
    implicitWidth: Math.max(32,glyphSize+12)
    implicitHeight: implicitWidth
    padding: 6
    Accessible.name: description
    contentItem: AppIcon {name: action.symbol;ink: !action.enabled ? Theme.separator : action.checked ? Theme.accent : Theme.secondary;implicitWidth: action.glyphSize;implicitHeight: action.glyphSize}
    ToolTip.visible: hovered || activeFocus
    ToolTip.delay: 600
    ToolTip.text: description
}
