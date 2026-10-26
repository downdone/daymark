import QtQuick
import QtQuick.Templates as T
T.MenuItem {
    id: control
    property bool destructive: false
    TextMetrics {id: labelMetrics;text: control.text;font: control.font}
    implicitWidth: Math.max(labelMetrics.advanceWidth,contentItem.implicitWidth)+leftPadding+rightPadding+2
    implicitHeight: Math.max(DesignTokens.menuRowHeight,contentItem.implicitHeight+8)
    height: visible ? implicitHeight : 0
    leftPadding: 28*DesignTokens.scale
    rightPadding: (subMenu ? 28 : 12)*DesignTokens.scale
    hoverEnabled: true
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.menuFontSize
    font.weight: Font.Normal
    contentItem: Text {text: control.text;font.family: DesignTokens.fontFamily;font.pixelSize: DesignTokens.menuFontSize;font.weight: Font.Normal;color: !control.enabled ? DesignTokens.disabledText : control.destructive ? DesignTokens.danger : DesignTokens.text;verticalAlignment: Text.AlignVCenter;wrapMode: Text.Wrap}
    indicator: Text {visible: control.checked;x: 10*DesignTokens.scale;y: (control.height-height)/2;text: "✓";color: DesignTokens.accent;font.family: DesignTokens.fontFamily;font.weight: Font.Normal;font.pixelSize: DesignTokens.menuFontSize}
    arrow: Text {visible: !!control.subMenu;text: "›";color: DesignTokens.secondary;anchors.right: parent.right;anchors.rightMargin: 10;anchors.verticalCenter: parent.verticalCenter;font.family: DesignTokens.fontFamily;font.weight: Font.Normal;font.pixelSize: DesignTokens.menuFontSize}
    // Menu owns the active item. An independent hover fill can paint a second
    // item, including disabled entries, while keyboard/submenu focus is retained.
    background: Rectangle {objectName: "menuItemSurface";radius: DesignTokens.radius;color: !control.enabled ? "transparent" : control.down ? DesignTokens.pressed : control.highlighted ? DesignTokens.selected : "transparent";border.width: control.enabled && control.visualFocus ? 2 : 0;border.color: DesignTokens.accent}
}
