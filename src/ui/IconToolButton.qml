import QtQuick
import QtQuick.Controls
ToolButton {
    id: control
    readonly property AppIcon hintIcon: contentItem as AppIcon
    property bool hintEnabled: false
    readonly property bool standardAction: !hintIcon || text.length>0 || ["more","left","right","up","down","close","collapse","expand","check","circle","subtasks","batch","list","calendar","filter","search"].indexOf(hintIcon.name)>=0
    property bool hintBlocked: false
    property string hintText: Accessible.name
    function hintForVerification(){const tip=ToolTip.toolTip;return {visible:tip ? tip.visible && tip.parent===control : false,text:tip ? tip.text : "",x:tip ? tip.x : 0,y:tip ? tip.y : 0,width:tip ? tip.width : 0,height:tip ? tip.height : 0}}
    ToolTip.visible: hintEnabled && !standardAction && !hintBlocked && !Theme.hintsBlocked && enabled && hovered && !down && hintText.length>0
    ToolTip.delay: Theme.hintDelay
    ToolTip.timeout: Theme.hintTimeout
    ToolTip.text: hintText
}
