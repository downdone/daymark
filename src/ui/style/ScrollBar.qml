import QtQuick
import QtQuick.Templates as T
T.ScrollBar {
    id: control
    implicitWidth: vertical ? 8 : 48
    implicitHeight: vertical ? 48 : 8
    minimumSize: 0.08
    visible: policy===T.ScrollBar.AlwaysOn || (policy===T.ScrollBar.AsNeeded && size>0 && size<0.999)
    padding: 2
    hoverEnabled: true
    contentItem: Rectangle {implicitWidth: 4;implicitHeight: 4;radius: 2;color: control.pressed ? DesignTokens.accent : control.hovered ? DesignTokens.secondary : DesignTokens.border;opacity: control.active || control.hovered ? 1 : 0.4}
}
