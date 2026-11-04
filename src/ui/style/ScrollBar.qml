import QtQuick
import QtQuick.Templates as T
T.ScrollBar {
    id: control
    implicitWidth: vertical ? 14 : 48
    implicitHeight: vertical ? 48 : 14
    minimumSize: 0.08
    visible: policy===T.ScrollBar.AlwaysOn || (policy===T.ScrollBar.AsNeeded && size>0 && size<0.999)
    padding: 5
    hoverEnabled: true
    // A wheel produces short active/inactive pulses. Keep a steady reading
    // contrast; only deliberate pointer interaction changes the emphasis.
    contentItem: Rectangle {implicitWidth: 4;implicitHeight: 4;radius: 2;color: control.pressed ? DesignTokens.accent : DesignTokens.secondary;opacity: DesignTokens.highContrast || control.pressed || control.hovered ? 1 : 0.35}
}
