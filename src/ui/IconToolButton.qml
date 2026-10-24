import QtQuick
import QtQuick.Controls
ToolButton {
    id: control
    ToolTip.visible: enabled && (hovered || activeFocus) && Accessible.name.length>0
    ToolTip.delay: 600
    ToolTip.text: Accessible.name
}
