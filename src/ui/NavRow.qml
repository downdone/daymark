pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ItemDelegate {
    id: root
    property string symbol: ""
    property bool iconBadge: false
    property bool hasChildren: false
    property bool showBranchSlot: false
    property bool branchExpanded: true
    property bool ancestorSelected: false
    property bool showCatalogActions: false
    property string detailsText: text
    property bool suppressCatalogTooltips: false
    signal expansionRequested(bool value)
    signal optionsRequested(real x,real y)
    function hintForVerification(){const tip=ToolTip.toolTip;return {visible:tip ? tip.visible && tip.parent===root : false,text:tip ? tip.text : ""}}
    ToolTip.visible: !Theme.hintsBlocked && showCatalogActions && navTextHover.hovered && !down && !catalogDragging && !suppressCatalogTooltips && detailsText.length>0 && (navText.truncated || detailsText!==text)
    ToolTip.delay: Theme.hintDelay
    ToolTip.timeout: Theme.hintTimeout
    ToolTip.text: detailsText
    TapHandler {acceptedButtons:Qt.LeftButton;acceptedModifiers:Qt.ControlModifier;enabled:Qt.platform.os==="osx" && root.showCatalogActions;onTapped:function(point){root.optionsRequested(point.position.x,point.position.y)}}
    TapHandler {acceptedButtons: Qt.RightButton;enabled: root.showCatalogActions;onTapped: function(point){root.optionsRequested(point.position.x,point.position.y)}}
    Keys.onLeftPressed: function(event){if(hasChildren)expansionRequested(false);else event.accepted=false}
    Keys.onRightPressed: function(event){if(hasChildren)expansionRequested(true);else event.accepted=false}
    Keys.onPressed: function(event){if(showCatalogActions && (event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier)))){optionsRequested(width/2,height);event.accepted=true}}
    property color symbolColor: selected ? Theme.accent : Theme.secondary
    property bool selected: false
    property int count: -1
    readonly property bool taskDropActive: dropTarget.containsDrag && !catalogDropActive
    property bool acceptsTaskDrop: false
    property string catalogId: ""
    property real catalogVersion: 0
    property string catalogParentId: ""
    property bool canDragCatalog: false
    property Item catalogDragLayer: null
    property bool catalogDragging: false
    property string catalogDropPlacement: "inside"
    property bool catalogDropActive: false
    property bool catalogDropAllowed: false
    signal catalogDragChanged(bool active)
    signal catalogDropped(string sourceId,real sourceVersion,string placement)
    onCatalogDraggingChanged: catalogDragChanged(catalogDragging)
    function catalogDragPoint(item){return catalogProxy.mapToItem(item,12,catalogProxy.height/2)}
    function placeCatalogDrag(){const point=mapToItem(catalogProxy.parent,categoryDrag.centroid.position.x,categoryDrag.centroid.position.y);catalogProxy.x=point.x-12;catalogProxy.y=point.y-catalogProxy.height/2}
    DragHandler {
        id: categoryDrag;target: null;enabled: root.canDragCatalog;acceptedButtons: Qt.LeftButton
        onActiveChanged: {if(active){catalogProxy.parent=root.catalogDragLayer || root;root.catalogDragging=true;root.forceActiveFocus();root.placeCatalogDrag()}else if(root.catalogDragging){catalogProxy.Drag.drop();root.catalogDragging=false;catalogProxy.parent=root;catalogProxy.x=0;catalogProxy.y=0}}
        onTranslationChanged: if(active)root.placeCatalogDrag()
    }
    Item {
        id: catalogProxy;z: 2000;width: Math.min(280,root.width);height: Math.max(36,root.height)
        Drag.active: root.catalogDragging;Drag.source: root;Drag.hotSpot.x: 12;Drag.hotSpot.y: height/2;Drag.supportedActions: Qt.MoveAction
        Rectangle {anchors.fill: parent;visible: root.catalogDragging;color: Theme.content;radius: Theme.rowRadius;border.color: Theme.accent
            Label {anchors.fill: parent;anchors.margins: 8;text: root.detailsText;elide: Text.ElideRight;verticalAlignment: Text.AlignVCenter}}
    }
    signal taskDropped(string taskId,real version)
    DropArea {
        id: dropTarget;objectName: root.objectName+"DropTarget";anchors.fill: parent;enabled: root.acceptsTaskDrop || root.canDragCatalog
        function inspect(drag){
            const task=drag.source as TaskRow
            if(task){root.catalogDropActive=false;root.catalogDropAllowed=false;drag.accepted=root.acceptsTaskDrop && task.canDrag;return}
            const source=drag.source as NavRow
            root.catalogDropActive=!!source
            root.catalogDropPlacement=drag.y<height*0.3 ? "before" : drag.y>height*0.7 ? "after" : "inside"
            // Keep tracking a category over an invalid centre so its next move
            // to the same row's edge can become a valid sibling insertion.
            drag.accepted=!!source && source!==root && source.canDragCatalog && root.canDragCatalog
            root.catalogDropAllowed=drag.accepted && (root.catalogDropPlacement!=="inside" || (!root.catalogParentId && !source.hasChildren)) && !(source.hasChildren && root.catalogParentId)
        }
        onEntered: function(drag){inspect(drag)}
        onExited: {root.catalogDropActive=false;root.catalogDropAllowed=false}
        onPositionChanged: function(drag){inspect(drag)}
        onDropped: function(drop){
            const task=drop.source as TaskRow
            if(task && task.canDrag && root.acceptsTaskDrop){root.taskDropped(task.taskId,task.rowVersion);drop.acceptProposedAction();return}
            inspect(drop);const source=drop.source as NavRow
            if(root.catalogDropAllowed && source){root.catalogDropped(source.catalogId,source.catalogVersion,root.catalogDropPlacement);drop.acceptProposedAction()}else drop.accepted=false
        }
    }
    Timer {interval:650;running:dropTarget.containsDrag && root.catalogDropAllowed && root.catalogDropPlacement==="inside" && root.hasChildren && !root.branchExpanded;onTriggered:root.expansionRequested(true)}
    Accessible.description:root.catalogDropActive && !root.catalogDropAllowed ? qsTr("此位置不能接收分类；最多支持两级，可拖到上下缘排序。") : ""
    topPadding: 0;bottomPadding: 0
    implicitHeight: Math.max(Theme.navigationRowHeight,contentItem.implicitHeight)
    highlighted: selected
    Accessible.name: detailsText
    contentItem: RowLayout {
        spacing: 6
        Item {visible: root.showBranchSlot || root.hasChildren;Layout.preferredWidth: 12;Layout.preferredHeight: 32;WidgetAction {anchors.centerIn: parent;visible: root.hasChildren;symbol: root.branchExpanded ? "down" : "right";description: (root.branchExpanded ? qsTr("收起分类：") : qsTr("展开分类："))+root.text;onClicked: root.expansionRequested(!root.branchExpanded)}}
        Item {
            Layout.preferredWidth: root.iconBadge ? 26 : Math.min(24,16*Theme.scale)
            Layout.preferredHeight: root.iconBadge ? 26 : Math.min(24,16*Theme.scale)
            Rectangle {anchors.fill: parent;visible: root.iconBadge;radius: 7;color: root.selected || root.taskDropActive ? Theme.primaryFill : Theme.content}
            AppIcon {anchors.centerIn: parent;name: root.symbol;ink: root.selected || root.taskDropActive ? Theme.accent : root.symbolColor;implicitWidth: Math.min(22,16*Theme.scale);implicitHeight: implicitWidth}
        }
        Label { id: navText;objectName: "navigationLabel";HoverHandler {id: navTextHover} font.weight: root.selected ? Font.DemiBold : Font.Normal;text: root.catalogDropActive && root.catalogDropAllowed && root.catalogDropPlacement==="inside" ? qsTr("移入：%1").arg(root.text) : root.text; color: Theme.text; font.pixelSize: Theme.navigationSize; Layout.fillWidth: true; elide: Text.ElideRight }
        Rectangle {visible: root.ancestorSelected;implicitWidth: 5;implicitHeight: 5;radius: 3;color: Theme.accent}
        WidgetAction {visible: root.showCatalogActions;opacity: root.hovered || root.activeFocus ? 1 : 0;symbol: "more";description: qsTr("%1 的选项").arg(root.text);hintText:qsTr("分类或标签选项");onClicked: root.optionsRequested(root.width,root.height)}
        Label { visible: root.count>=0; text: root.count; color: Theme.secondary; font.pixelSize: Theme.captionSize }
    }
    Keys.onEscapePressed: function(event){if(root.catalogDragging){catalogProxy.Drag.cancel();root.catalogDragging=false;categoryDrag.enabled=false;categoryDrag.enabled=Qt.binding(function(){return root.canDragCatalog})}else event.accepted=false}
    background: Rectangle {
        Rectangle {visible: dropTarget.containsDrag && root.catalogDropAllowed && root.catalogDropPlacement!=="inside";height: 2;width: parent.width;y: root.catalogDropPlacement==="before" ? 0 : parent.height-2;color: Theme.accent}
        radius: Theme.rowRadius; color: (dropTarget.containsDrag && (!root.catalogDropActive || (root.catalogDropAllowed && root.catalogDropPlacement==="inside"))) || root.selected ? Theme.selected : root.hovered ? Theme.hover : "transparent"; border.width: (root.activeFocus && !root.catalogDragging) || (dropTarget.containsDrag && root.catalogDropAllowed && root.catalogDropPlacement==="inside") ? 1 : 0; border.color: Theme.accent }
}
