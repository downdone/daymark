import QtQuick
import QtQuick.Controls
ToolButton {
    id: control
    readonly property AppIcon hintIcon: contentItem as AppIcon
    property bool hintEnabled: !hintIcon || ["left","right","up","down","close","collapse","expand","check","circle"].indexOf(hintIcon.name)<0
    property bool hintBlocked: false
    property string hintText: Accessible.name
    function hintForVerification(){const tip=ToolTip.toolTip;return {visible:tip ? tip.visible && tip.parent===control : false,text:tip ? tip.text : ""}}
    ToolTip.visible: hintEnabled && !hintBlocked && !Theme.hintsBlocked && enabled && hovered && !down && hintText.length>0
    ToolTip.delay: Theme.hintDelay
    ToolTip.timeout: Theme.hintTimeout
    ToolTip.text: hintText
}
