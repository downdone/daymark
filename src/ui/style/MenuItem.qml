import QtQuick
import QtQuick.Templates as T
T.MenuItem {
    id: control
    property bool destructive: false
    property string shortcutText: ""
    property bool groupStart: false
    readonly property real groupInset: groupStart ? 9 : 0
    Accessible.description: shortcutText
    function verifyColumns() {
        if(!shortcutText)return "PASS"
        if(contentItem.x+contentItem.width>shortcutLabel.x-8*DesignTokens.scale || shortcutLabel.x<0 || shortcutLabel.x+shortcutLabel.width>width)return "shortcut overlaps label or menu edge"
        return "PASS"
    }
    TextMetrics {id: labelMetrics;text: control.text;font: control.font}
    implicitWidth: Math.max(labelMetrics.advanceWidth,contentItem.implicitWidth)+leftPadding+rightPadding+2
    implicitHeight: Math.max(DesignTokens.menuRowHeight,contentItem.implicitHeight+8)+groupInset
    height: visible ? implicitHeight : 0
    topPadding: groupInset
    leftPadding: 28*DesignTokens.scale
    rightPadding: (subMenu ? 28 : 12)*DesignTokens.scale+(shortcutText ? shortcutLabel.implicitWidth+20*DesignTokens.scale : 0)
    hoverEnabled: true
    font.family: DesignTokens.fontFamily
    font.pixelSize: DesignTokens.menuFontSize
    font.weight: Font.Normal
    contentItem: Text {text: control.text;font.family: DesignTokens.fontFamily;font.pixelSize: DesignTokens.menuFontSize;font.weight: Font.Normal;color: !control.enabled ? DesignTokens.disabledText : control.destructive ? DesignTokens.danger : DesignTokens.text;verticalAlignment: Text.AlignVCenter;wrapMode: Text.Wrap}
    indicator: Glyph {visible:control.checked;x:8*DesignTokens.scale;y:control.groupInset+(control.height-control.groupInset-height)/2;width:Math.min(24,16*DesignTokens.scale);height:width;name:"check";ink:control.enabled ? DesignTokens.accent : DesignTokens.disabledText}
    arrow: Glyph {visible:!!control.subMenu;name:"right";ink:control.enabled ? DesignTokens.secondary : DesignTokens.disabledText;anchors.right:parent.right;anchors.rightMargin:10;y:control.groupInset+(control.height-control.groupInset-height)/2;width:Math.min(20,14*DesignTokens.scale);height:width}
    Text {id:shortcutLabel;visible:control.shortcutText.length>0;text:control.shortcutText;font:control.font;color:control.enabled ? DesignTokens.secondary : DesignTokens.disabledText;anchors.right:parent.right;anchors.rightMargin:12*DesignTokens.scale;y:control.groupInset+(control.height-control.groupInset-height)/2;Accessible.ignored:true}
    Rectangle {visible:control.groupStart;y:4;width:parent.width;height:1;color:DesignTokens.border}
    MouseArea {visible:control.groupStart;width:parent.width;height:control.groupInset;acceptedButtons:Qt.AllButtons;onPressed:function(mouse){mouse.accepted=true}}
    // Menu owns the active item. An independent hover fill can paint a second
    // item, including disabled entries, while keyboard/submenu focus is retained.
    background: Rectangle {objectName: "menuItemSurface";y:control.groupInset;height:control.height-control.groupInset;radius: DesignTokens.radius;color: !control.enabled ? "transparent" : control.down ? DesignTokens.pressed : control.highlighted ? DesignTokens.selected : "transparent";border.width: control.enabled && control.visualFocus ? 1 : 0;border.color: DesignTokens.accent}
}
