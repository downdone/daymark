pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS

DropArea {
    id:root
    objectName:"widgetDropTarget-"+(browseOnly?"chooser":contextKey)
    required property TaskWorkspace workspace
    property string contextKey:""
    property string destinationName:""
    property bool blocked:false
    property bool browseOnly:false
    signal browseRequested()
    signal moveAccepted(string destination)
    signal errorReported(string message)
    anchors.fill:parent
    keys:["daymark-widget-task"]
    readonly property WidgetTaskRow sourceTask:drag.source as WidgetTaskRow
    readonly property bool validSource:!!sourceTask && sourceTask.workspace===workspace && sourceTask.dragging && sourceTask.dragProfile===workspace.widgetProfileToken
    readonly property string reason:containsDrag && validSource && !browseOnly ? workspace.widgetCategoryDropReason(sourceTask.taskId,sourceTask.dragTaskVersion,contextKey,sourceTask.dragProfile) : ""
    readonly property bool allowed:!blocked && !workspace.busy && validSource && (browseOnly || reason.length===0)
    onDropped:function(drop){
        browseDelay.stop()
        if(browseOnly){drop.accepted=false;return}
        if(!allowed){drop.accepted=false;return}
        if(workspace.moveWidgetTaskToCategory(sourceTask.taskId,sourceTask.dragTaskVersion,contextKey,sourceTask.dragProfile)){
            drop.acceptProposedAction();moveAccepted(destinationName)
        }else {drop.accepted=false;errorReported(workspace.widgetBrowseError || workspace.errorText)}
    }
    Timer {id:browseDelay;interval:500;running:root.browseOnly && root.containsDrag && root.allowed;onTriggered:if(root.containsDrag && root.allowed)root.browseRequested()}
    Rectangle {
        anchors.fill:parent;radius:Theme.controlRadius;color:"transparent"
        visible:root.containsDrag && root.validSource && !root.blocked
        border.width:2;border.color:root.allowed?Theme.accent:Theme.danger
    }
    DS.ToolTip {
        contextualFeedback:true;delay:0;timeout:-1
        visible:root.containsDrag && root.validSource && !root.blocked
        text:root.browseOnly?qsTr("停留以展开所有分类"):root.allowed?qsTr("移到 %1").arg(root.destinationName):root.reason
    }
}
