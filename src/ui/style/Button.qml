import QtQuick
import QtQuick.Templates as T
T.Button {
    id: control
    implicitWidth: contentItem.implicitWidth+leftPadding+rightPadding
    implicitHeight: Math.max(DesignTokens.controlHeight,contentItem.implicitHeight+12)
    padding: 6
    horizontalPadding: 10
    spacing: 8
    hoverEnabled: true
    font.pixelSize: DesignTokens.body
    font.family: DesignTokens.fontFamily
    font.weight: highlighted ? Font.DemiBold : Font.Normal
    readonly property string labelText: {
        if(!T.DialogButtonBox.buttonBox)return text
        const labels={"OK":qsTr("确认"),"Cancel":qsTr("取消"),"Close":qsTr("关闭"),"Save":qsTr("保存"),"Discard":qsTr("放弃"),"Yes":qsTr("是"),"No":qsTr("否"),"Apply":qsTr("应用")}
        return labels[text] || text
    }
    Accessible.name: labelText
    contentItem: Text {
        text: control.labelText
        font: control.font
        color: !control.enabled ? DesignTokens.disabledText : control.highlighted ? DesignTokens.accentText : control.palette.buttonText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        y: DesignTokens.controlInset
        height: Math.max(0,control.height-2*DesignTokens.controlInset)
        radius: DesignTokens.radius
        color: !control.enabled ? DesignTokens.sidebar : control.highlighted ? (control.down ? Qt.darker(DesignTokens.accent,1.12) : DesignTokens.accent) : control.down ? DesignTokens.pressed : control.hovered ? DesignTokens.hover : control.flat ? "transparent" : DesignTokens.fieldFill
        border.width: control.visualFocus || DesignTokens.highContrast && !control.flat ? 1 : 0
        border.color: control.visualFocus ? DesignTokens.accent : DesignTokens.outline
    }
}
