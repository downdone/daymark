pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: widget
    required property WidgetViewModel controller
    required property TaskWorkspace workspace
    signal openMainRequested()
    signal openTaskRequested(string taskId)
    signal settingsRequested()
    objectName: "widgetWindow"
    width: 340;height: 360
    readonly property int ballDiameter: Math.max(36,Theme.iconSize+12)
    readonly property int miniBarHeight: Math.max(48,Theme.controlHeight+2*Theme.panelPadding)
    onMiniBarHeightChanged: Qt.callLater(syncMiniLayout)
    onBallDiameterChanged: Qt.callLater(syncMiniLayout)
    Component.onCompleted: {syncMiniLayout();controller.enableVisualTransitions();Qt.callLater(syncPanelMinimum);Qt.callLater(setMiniAnimation)}
    minimumWidth:ballDiameter;minimumHeight:ballDiameter
    readonly property int panelMinimumHeight:Math.ceil(Math.max(190,header.implicitHeight+footer.implicitHeight+(widgetError.visible?widgetError.implicitHeight:0)+2*Theme.panelPadding+4*Theme.fieldGap+48+Theme.listInset*2+(controller.panelRect.width<300?footer.implicitHeight:0)))
    onPanelMinimumHeightChanged:Qt.callLater(syncPanelMinimum)
    function syncPanelMinimum(){controller.setPanelMinimumSize(260,panelMinimumHeight)}
    x: 80;y: 80;visible: false;color: "transparent"
    title: qsTr("Daymark 今日任务")
    property string taskError: ""
    property bool addingTask: false
    property bool addingPending: false
    property bool taskDragging: false
    property point taskDragPoint: Qt.point(0,0)
    property var contextTask: ({})
    property string taskOperation: ""
    property bool localDeletion: false
    readonly property bool commandMenuOpen: widgetMenu.visible || taskMenu.visible
    property bool commandInputBlocked: false
    function releaseCommandInput() {if(!commandMenuOpen)commandInputBlocked=false}
    onCommandMenuOpenChanged: {
        if(commandMenuOpen)commandInputBlocked=true
        // A native Popup can replay the dismissal press to its host while it
        // closes. Keep that dispatch turn shielded; a fresh gesture works next.
        else Qt.callLater(releaseCommandInput)
    }
    function submitTask() {
        if(addingPending || !quickTitle.text.trim().length)return
        taskError="";addingPending=workspace.createTodayTask(quickTitle.text)
        if(!addingPending)taskError=workspace.errorText || qsTr("暂时无法添加，请稍后重试。")
    }
    function openTaskMenu(id,source,position) {
        contextTask=workspace.task(id)
        if(!contextTask.taskId)return
        taskMenu.popupAtPoint(source,position)
    }
    function taskAction(action) {
        taskError=""
        const id=contextTask.taskId,version=contextTask.rowVersion
        let accepted=false
        if(action==="pin" || action==="important" || action==="cancelReminder") {
            const patch=action==="pin" ? {pinned:!contextTask.pinned} : action==="important" ? {important:!contextTask.important} : {remindAtUtc:"",reminderRepeat:"NONE"}
            taskOperation="details";accepted=workspace.saveDetails(id,version,patch)
        } else if(action==="up" || action==="down") {
            taskOperation="reorder";accepted=workspace.moveTodayTask(id,action==="up" ? -1 : 1)
        } else {
            taskOperation=action;accepted=workspace.changeState(id,version,action)
            if(accepted && action==="delete")localDeletion=true
        }
        if(!accepted){taskOperation="";taskError=workspace.errorText || qsTr("任务已变化，请重新打开菜单后重试。")}
    }
    function requestTaskDelete() {
        if(workspace.batchImpact([{taskId:contextTask.taskId,rowVersion:contextTask.rowVersion}],"delete")>1)deleteTaskConfirmation.open()
        else taskAction("delete")
    }
    readonly property string errorMessage: taskError || controller.errorText
    readonly property string primaryActionName: (controller.enabledForms & 1)!==0 ? qsTr("打开任务小窗") : controller.miniCollapsed && (controller.enabledForms & 2)!==0 ? qsTr("显示 Mini 条") : qsTr("打开主窗口")
    readonly property bool reduceMotion: workspace.preferences.followSystemAccessibility ? workspace.systemAccessibility.reduceMotion : workspace.preferences.reduceMotion
    readonly property bool interacting: headerDrag.active || miniDrag.active || ballDrag.active
    readonly property bool miniMode: controller.mini
    property bool miniKeyboardInteraction: false
    property bool pendingMiniKeyboardFocus: false
    property real motionProgress:1
    property rect fromRect:Qt.rect(0,0,340,360)
    property rect toRect:fromRect
    property real fromPanel:1
    property real toPanel:1
    property real fromOpacity:1
    property real fromMini:0
    property real toMini:0
    property real fromRadius:Theme.panelRadius
    property real toRadius:Theme.panelRadius
    readonly property rect frameRect:Qt.rect(fromRect.x+(toRect.x-fromRect.x)*motionProgress,fromRect.y+(toRect.y-fromRect.y)*motionProgress,fromRect.width+(toRect.width-fromRect.width)*motionProgress,fromRect.height+(toRect.height-fromRect.height)*motionProgress)
    readonly property real visualWidth:frameRect.width
    readonly property real visualHeight:frameRect.height
    readonly property real frameRadius:fromRadius+(toRadius-fromRadius)*motionProgress
    readonly property real panelReveal:fromPanel+(toPanel-fromPanel)*motionProgress
    readonly property real panelContentOpacity:toPanel>fromPanel ? fromOpacity+(1-fromOpacity)*Math.max(0,(motionProgress-0.2)/0.8) : fromOpacity*Math.max(0,1-motionProgress*2.5)
    readonly property real miniReveal:fromMini+(toMini-fromMini)*motionProgress
    readonly property bool miniTransitionRunning:motionAnimation.running && !controller.panelMotion
    readonly property bool panelTransitionRunning:motionAnimation.running && controller.panelMotion
    readonly property bool motionInputBlocked:motionAnimation.running || controller.transitionPending
    property int visualTransition:-1
    property bool pendingWidgetMenu:false
    property Item transitionFocusItem:null
    onMotionInputBlockedChanged:{
        if(motionInputBlocked && active){transitionFocusItem=activeFocusItem;inputShield.forceActiveFocus()}
        else if(!motionInputBlocked){const previous=transitionFocusItem;transitionFocusItem=null;if(active && previous && previous.visible && previous.enabled)previous.forceActiveFocus();else if(active)Qt.callLater(focusCurrentForm)}
    }
    onFrameRectChanged:syncInputRegion()
    onFrameRadiusChanged:syncInputRegion()
    readonly property Control miniFocusControl: activeFocusItem as Control
    readonly property bool miniKeyboardFocus: active && (miniKeyboardInteraction || (!!miniFocusControl && [Qt.TabFocusReason,Qt.BacktabFocusReason,Qt.ShortcutFocusReason].indexOf(miniFocusControl.focusReason)>=0))
    readonly property bool interactionPressed: ballPress.active || miniPress.active || headerPress.active || miniSummary.down || miniMore.pressed || workspace.busy || errorMessage.length>0 || addingTask || taskDragging
    onActiveChanged: {
        if(!active && !commandMenuOpen && !deleteTaskConfirmation.visible)miniKeyboardInteraction=false
        if(!active && addingTask && !addingPending && !quickTitle.text.length)addingTask=false
    }
    onMiniKeyboardFocusChanged: reportInteraction()
    onInteractionPressedChanged: reportInteraction()
    onInteractingChanged: reportInteraction()
    function reportInteraction() {controller.setInteraction(miniHover.hovered,interactionPressed || interacting,widgetMenu.visible || taskMenu.visible || deleteTaskConfirmation.visible,miniKeyboardFocus)}
    function syncMiniLayout() {controller.setMiniLayout(miniBarHeight,ballDiameter)}
    function targetRect(){return controller.effectiveForm===0?controller.panelRect:controller.effectiveForm===1?controller.miniRect:controller.ballRect}
    function syncSurfaceSize(){setMiniAnimation()}
    function syncInputRegion(){controller.setRevealRegion(Qt.rect(Math.floor(frameRect.x),Math.floor(frameRect.y),Math.ceil(frameRect.x+frameRect.width)-Math.floor(frameRect.x),Math.ceil(frameRect.y+frameRect.height)-Math.floor(frameRect.y)),frameRadius)}
    function finishVisual(immediate){
        motionAnimation.stop();motionProgress=1;syncInputRegion()
        if(immediate)controller.finishTransition(visualTransition)
        else controller.presentTransition(visualTransition)
    }
    function setMiniAnimation(){
        if(!controller.ready || controller.geometryChanging)return
        if(visualTransition===controller.transitionId){
            if((reduceMotion || !visible || controller.clickThrough) && motionAnimation.running)finishVisual(!visible || controller.clickThrough)
            if(pendingWidgetMenu && !controller.transitionPending){pendingWidgetMenu=false;Qt.callLater(showWidgetMenu)}
            return
        }
        widgetMenu.close();taskMenu.close();pendingWidgetMenu=false
        const currentRect=frameRect,currentPanel=panelReveal,currentMini=miniReveal,currentRadius=frameRadius,currentOpacity=panelContentOpacity
        motionAnimation.stop();visualTransition=controller.transitionId
        fromRect=currentRect;fromPanel=currentPanel;fromMini=currentMini;fromRadius=currentRadius;fromOpacity=currentOpacity
        toRect=targetRect();toPanel=controller.effectiveForm===0?1:0;toMini=controller.effectiveForm===1?1:0
        toRadius=controller.effectiveForm===0?Theme.panelRadius:controller.effectiveForm===2?ballDiameter/2:Math.min(controller.miniRect.height/2,22)
        motionProgress=0
        if(!controller.transitionPending || reduceMotion || !visible || controller.clickThrough){finishVisual(!controller.transitionPending || !visible || controller.clickThrough);return}
        const distance=Math.max(Math.abs(toRect.width-fromRect.width),Math.abs(toRect.height-fromRect.height),Math.abs(toRect.x-fromRect.x),Math.abs(toRect.y-fromRect.y))
        const span=Math.max(1,controller.panelRect.width-ballDiameter,controller.panelRect.height-ballDiameter,controller.miniRect.width-ballDiameter)
        const duration=controller.panelMotion?(controller.targetBall?150:180):160
        motionAnimation.duration=Math.max(1,Math.round(duration*Math.min(1,distance/span)))
        motionAnimation.start()
    }
    NumberAnimation {id:motionAnimation;target:widget;property:"motionProgress";to:1;easing.type:Easing.OutCubic;onFinished:widget.controller.presentTransition(widget.visualTransition)}
    Connections {target:widget.controller;function onStateChanged(){widget.setMiniAnimation()}}
    Connections {target:widget.workspace;function onStateChanged(){if(!widget.workspace.errorText.length)widget.taskError=""}}
    Connections {
        target:widget.workspace
        function onOperationFinished(operation,success) {
            if(operation==="widgetCreate" && widget.addingPending){widget.addingPending=false;if(success){quickTitle.clear();if(widget.active)quickTitle.forceActiveFocus();else widget.addingTask=false}else widget.taskError=widget.workspace.errorText}
            if(operation===widget.taskOperation){widget.taskOperation="";if(!success)widget.taskError=widget.workspace.errorText}
        }
    }
    onReduceMotionChanged:if(reduceMotion && motionAnimation.running)finishVisual(!visible || controller.clickThrough)
    onVisibleChanged:{reportInteraction();Qt.callLater(setMiniAnimation)}
    function focusCurrentForm(){
        if(!active || !visible || motionInputBlocked)return
        if(controller.miniCollapsed)miniBall.forceActiveFocus()
        else if(controller.mini)miniSummary.forceActiveFocus()
        else if(addingTask)quickTitle.forceActiveFocus()
        else if(todayList.count>0)todayList.forceActiveFocus()
        else addTask.forceActiveFocus()
    }
    onMiniModeChanged:{
        if(!(controller.effectiveForm===0 && controller.selectedForm!==0))miniKeyboardInteraction=false
        if(active)Qt.callLater(focusCurrentForm)
    }
    function closeOrCompact() {if(taskMenu.visible)taskMenu.close();else if(deleteTaskConfirmation.visible)deleteTaskConfirmation.close();else if(taskDragging){const row=todayList.currentItem as WidgetTaskRow;if(row)row.finishDrag(false)}else if(addingTask){if(!quickTitle.text.length){addingTask=false;addTask.forceActiveFocus()}}else if(widgetMenu.visible)widgetMenu.close();else if(!controller.mini)controller.returnToCompact()}
    Shortcut {sequence:"Escape";enabled:widget.active;onActivated:widget.closeOrCompact()}
    function focusedTaskRow(){
        let item=activeFocusItem
        while(item && item!==todayList){if(item instanceof WidgetTaskRow)return item;item=item.parent}
        return activeFocusItem===todayList ? todayList.currentItem : null
    }
    Shortcut {sequences:["Shift+F10","Menu"];enabled:widget.active && !widget.motionInputBlocked;onActivated:{const row=widget.controller.mini?null:widget.focusedTaskRow();if(row && !widget.addingTask)widget.openTaskMenu(row.taskId,row,Qt.point(row.width/2,row.height/2));else widget.openVerificationMenu()}}
    function verifyMenu() {
        if(!widgetMenu.visible)return "closed"
        if(widgetMenu.verifyGrouping()!=="PASS")return widgetMenu.verifyGrouping()
        const source=controller.miniCollapsed ? miniBall : controller.mini ? miniMore : widgetMore
        if(widgetMenu.anchorItem!==source || widgetMenu.parent!==widget.contentItem)return "stale menu anchor"
        let item=widgetMenu.contentItem
        while(item){if(item.opacity<0.999 || !item.enabled)return "menu inherits fading or disabled ancestor";item=item.parent}
        const background=widgetMenu.background as Rectangle
        if(widgetMenu.opacity!==1 || !background || background.color.a!==1)return "translucent menu surface"
        if(widgetMenu.nativePresented)return "PASS" // OS pixels/input are verified separately.
        if(widgetMenu.menuWindow===widget)return "menu did not create an independent window"
        if(widgetMenu.menuWindow.color.a!==1 || widgetMenu.menuWindow.opacity!==1)return "translucent menu window"
        if(!(widgetMenu.menuWindow.flags & Qt.WindowStaysOnTopHint) || (widgetMenu.menuWindow.flags & Qt.WindowStaysOnBottomHint))return "menu window is not topmost"
        const at=source.mapToGlobal(source.width,source.height)
        const edge=widgetMenu.contentItem.mapToGlobal(widgetMenu.contentItem.width+widgetMenu.rightPadding,-widgetMenu.topPadding)
        const expectedRight=Math.max(Screen.virtualX+6,Math.min(at.x-widgetMenu.width,Screen.virtualX+Screen.width-widgetMenu.width-6))+widgetMenu.width
        if(Math.abs(edge.x-expectedRight)>8 || Math.abs(edge.y-at.y-4)>8)return "menu is detached from button "+at+" / "+edge+" expectedRight="+expectedRight+" menuWidth="+widgetMenu.width+" screen="+Screen.virtualX+","+Screen.width
        for(let i=0;i<widgetMenu.count;i++) {
            const item=widgetMenu.itemAt(i) as MenuItem
            if(item && item.text){if(item.width<item.implicitWidth-1)return "clipped item "+i;const label=item.contentItem as Text;if(item.font.pixelSize!==Theme.bodySize || !label || label.font.pixelSize!==Theme.bodySize || label.font.weight!==Font.Normal)return "menu font mismatch "+i;if(label.lineCount!==1)return "unexpected menu wrap "+i}
        }
        return "PASS"
    }
    function hintForVerification() {const tip=miniSummary.ToolTip.toolTip;return {visible:tip ? tip.visible && tip.parent===miniSummary : false,text:tip ? tip.text : ""}}
    // How far the footer extends past the surface, in DIP; 0 when it fits. Used by the
    // smoke probe to grow a window whose declared minimum undercounted a wrapped row.
    function verificationOverflow() {
        return Math.ceil(Math.max(0, footer.y + footer.height - surface.height))
    }
    function showWidgetMenu(){if(!visible || controller.clickThrough || !controller.enabledForms || widget.motionInputBlocked)return;widgetMenu.popupAt(controller.miniCollapsed?miniBall:controller.mini?miniMore:widgetMore)}
    function openVerificationMenu(){
        if(motionAnimation.running || controller.transitionPending){pendingWidgetMenu=true;finishVisual(false)}
        else showWidgetMenu()
    }
    function closeVerificationMenu() {widgetMenu.close()}
    DS.Menu {
        id:taskMenu;objectName:"widgetTaskMenu";parent:widget.contentItem;popupType:Qt.platform.pluginName==="windows" ? Popup.Native : Popup.Window
        onVisibleChanged:widget.reportInteraction()
        MenuItem {text:qsTr("任务详情");onTriggered:widget.openTaskRequested(widget.contextTask.taskId)}
        MenuItem {objectName:"widgetTaskPin";text:widget.contextTask.pinned ? qsTr("取消置顶") : qsTr("置顶");onTriggered:widget.taskAction("pin")}
        MenuItem {objectName:"widgetTaskImportant";text:widget.contextTask.important ? qsTr("取消特别关注") : qsTr("特别关注");onTriggered:widget.taskAction("important")}
        DS.MenuSeparator {}
        MenuItem {objectName:"widgetTaskUp";text:qsTr("上移");enabled:widget.workspace.canMoveTodayTask(widget.contextTask.taskId || "",-1);onTriggered:widget.taskAction("up")}
        MenuItem {objectName:"widgetTaskDown";text:qsTr("下移");enabled:widget.workspace.canMoveTodayTask(widget.contextTask.taskId || "",1);onTriggered:widget.taskAction("down")}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("取消提醒");visible:!!widget.contextTask.remindAtUtc;onTriggered:widget.taskAction("cancelReminder")}
        MenuItem {objectName:"widgetTaskComplete";text:qsTr("完成任务");onTriggered:widget.taskAction("complete")}
        DS.MenuItem {objectName:"widgetTaskDelete";text:qsTr("删除任务…");destructive:true;onTriggered:widget.requestTaskDelete()}
    }
    Dialog {
        id:deleteTaskConfirmation;objectName:"widgetTaskDeleteConfirmation";parent:widget.contentItem;anchors.centerIn:parent
        palette.window:Theme.content;palette.windowText:Theme.text;palette.text:Theme.text;palette.buttonText:Theme.text
        title:qsTr("删除任务及其子任务？");modal:true;width:Math.min(420,widget.width-24);standardButtons:Dialog.Ok|Dialog.Cancel
        contentItem:Label {text:qsTr("所含子任务会一起移入最近删除，可撤销本次删除。");wrapMode:Text.Wrap}
        onVisibleChanged:widget.reportInteraction()
        onAccepted:widget.taskAction("delete")
    }
    Timer {
        interval:30;repeat:true;running:widget.taskDragging
        onTriggered:{const at=widget.contentItem.mapToItem(todayList,widget.taskDragPoint.x,widget.taskDragPoint.y);if(at.y<32)todayList.contentY=Math.max(todayList.originY,todayList.contentY-8);else if(at.y>todayList.height-32)todayList.contentY=Math.min(Math.max(todayList.originY,todayList.originY+todayList.contentHeight-todayList.height),todayList.contentY+8)}
    }
    DS.Menu {
        id:widgetMenu;objectName:"widgetMenu"
        parent:widget.contentItem
        popupType:Qt.platform.pluginName==="windows" ? Popup.Native : Popup.Window
        onVisibleChanged:{widget.reportInteraction();if(visible)widget.miniKeyboardInteraction=false}
        MenuItem {objectName:"widgetFormPanel";visible:widget.controller.formCount>1 && ((widget.controller.enabledForms & 1)!==0);text:qsTr("小窗");checkable:true;checked:widget.controller.selectedForm===0;onTriggered:widget.controller.selectForm(0)}
        MenuItem {objectName:"widgetFormMini";visible:widget.controller.formCount>1 && ((widget.controller.enabledForms & 2)!==0);text:qsTr("Mini 条");checkable:true;checked:widget.controller.selectedForm===1;onTriggered:widget.controller.selectForm(1)}
        MenuItem {objectName:"widgetFormBall";visible:widget.controller.formCount>1 && ((widget.controller.enabledForms & 4)!==0);text:qsTr("悬浮球");checkable:true;checked:widget.controller.selectedForm===2;onTriggered:widget.controller.selectForm(2)}
        DS.MenuSeparator {groupEnabled:widget.controller.formCount>1}
        MenuItem {text:qsTr("打开主窗口");onTriggered:widget.openMainRequested()}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("普通层级");checkable:true;checked:widget.controller.layer===0;onTriggered:widget.controller.setLayer(0)}
        MenuItem {text:qsTr("始终置顶");checkable:true;checked:widget.controller.layer===1;onTriggered:widget.controller.setLayer(1)}
        MenuItem {objectName:"widgetDesktopPinnedMenu";text:qsTr("桌面层");checkable:true;checked:widget.controller.layer===2;onTriggered:widget.controller.setLayer(2)}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("锁定位置与大小");checkable:true;checked:widget.controller.locked;onTriggered:widget.controller.setLocked(checked)}
        MenuItem {text:qsTr("鼠标穿透（托盘/主窗恢复）");enabled:widget.controller.trayAvailable;onTriggered:{widgetMenu.close();widget.controller.setClickThrough(true)}}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("外观与组件设置…");onTriggered:widget.settingsRequested()}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("隐藏组件");onTriggered:widget.controller.setVisible(false)}
    }
    function verifyLayout() {
        if(controller.mini) {
            if(controller.miniCollapsed)return Math.abs(visualWidth-ballDiameter)<1 && Math.abs(visualHeight-ballDiameter)<1 && miniBall.visible ? "PASS" : "mini ball geometry"
            if(Math.abs(visualWidth-controller.miniRect.width)>1 || Math.abs(visualHeight-controller.miniRect.height)>1)return "mini visual size drift: "+visualWidth+"x"+visualHeight+" expected "+controller.miniRect+" phase="+motionProgress+" target="+toRect+" transition="+visualTransition+"/"+controller.transitionId
            if(miniSummary.width<60 || miniSummary.height<Theme.controlHeight || miniTitle.width<20)return "mini content clipped"
        } else if(todayList.width<100 || todayList.height<24 || footer.y+footer.height>surface.height+1 || header.width<header.implicitWidth-1){
            const details="width="+Math.round(header.width)+" implicit="+Math.round(header.implicitWidth)+" list="+Math.round(todayList.width)+"x"+Math.round(todayList.height)+" footerBottom="+Math.round(footer.y+footer.height)+" surfaceH="+Math.round(surface.height)
            return "floating layout clipped ("+details+")"
        }
        return "PASS"
    }
    Shortcut {sequences: [StandardKey.New];enabled: widget.active && !widget.controller.mini && !widget.commandInputBlocked && !widget.motionInputBlocked;onActivated:{widget.addingTask=true;quickTitle.forceActiveFocus()}}
    Control {
        anchors.fill: parent
        font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize
        palette.window: Theme.content;palette.windowText: Theme.text;palette.text: Theme.text
        palette.button: Theme.sidebar;palette.buttonText: Theme.text;palette.highlight: Theme.accent
        Keys.onPressed:function(event){widget.miniKeyboardInteraction=true;widget.reportInteraction();event.accepted=false}
        contentItem: Item {
          Rectangle {
            id: surface
            x:widget.frameRect.x;y:widget.frameRect.y
            width: widget.visualWidth
            height: widget.visualHeight
            clip:true
            objectName: "widgetBackground"
            radius:widget.frameRadius
            color: Qt.rgba(Theme.content.r,Theme.content.g,Theme.content.b,widget.workspace.preferences.solidWidget || Theme.highContrast ? 1 : widget.controller.backgroundOpacity)
            border.color: Theme.separator
            HoverHandler {id:miniHover;onHoveredChanged:widget.reportInteraction()}
            ToolButton {
                id:miniBall;objectName:"widgetMiniBall"
                visible:opacity>0.001 || widget.controller.effectiveForm===2
                width:widget.ballDiameter;height:widget.ballDiameter
                x:widget.controller.ballRect.x-surface.x
                y:widget.controller.ballRect.y-surface.y
                padding:8
                opacity:Math.max(0,1-(widget.panelReveal+widget.miniReveal)*3)
                Accessible.name:qsTr("今天 %1 项").arg(widget.workspace.todayTasks.count)+"，"+widget.primaryActionName
                onClicked:if(!ballDrag.active)widget.controller.activatePrimary()
                Keys.onReturnPressed:widget.controller.activatePrimary()
                onActiveFocusChanged:if(activeFocus && [Qt.TabFocusReason,Qt.BacktabFocusReason,Qt.ShortcutFocusReason].indexOf(focusReason)>=0)widget.miniKeyboardInteraction=true
                TapHandler {acceptedButtons:Qt.RightButton;enabled:!widget.commandInputBlocked && !widget.motionInputBlocked;onTapped:widget.openVerificationMenu()}
                PointHandler {id:ballPress;acceptedButtons:Qt.LeftButton}
                DragHandler {id:ballDrag;target:null;enabled:!widget.controller.locked;onActiveChanged:{if(active)widget.controller.startMove();else widget.controller.finishMove()} cursorShape:Qt.OpenHandCursor}
                contentItem:Item {
                    implicitWidth:24;implicitHeight:24
                    BrandMark {objectName:"widgetBrandMark";anchors.fill:parent;visible:!widget.controller.errorText.length;Accessible.ignored:true}
                    AppIcon {anchors.fill:parent;visible:widget.controller.errorText.length>0;name:"info";ink:Theme.danger}
                }
                background:Rectangle {radius:width/2;color:miniBall.down ? Theme.pressed : miniBall.hovered ? Theme.hover : "transparent";border.width:miniBall.visualFocus ? 2 : 0;border.color:Theme.accent}
            }
            Item {
                id:panelContent;objectName:"widgetPanelContent"
                visible:widget.panelContentOpacity>0.001 || widget.controller.effectiveForm===0
                width:widget.controller.panelRect.width;height:widget.controller.panelRect.height
                x:widget.controller.panelRect.x-surface.x
                y:widget.controller.panelRect.y-surface.y
                opacity:widget.panelContentOpacity
              ColumnLayout {
                anchors.fill: parent;anchors.margins: Theme.panelPadding
                spacing: Theme.fieldGap
                RowLayout {
                    id: header
                    objectName: "widgetHeader"
                    Layout.fillWidth: true
                    spacing: 2
                    Item {
                        Layout.fillWidth: true;Layout.minimumWidth: 0
                        implicitHeight: headerText.implicitHeight
                        Column {
                            id: headerText
                            width: parent.width
                            RowLayout {
                                width: parent.width
                                spacing: 6
                                Label {Layout.maximumWidth: parent.width*0.7;text: qsTr("今天");elide: Text.ElideRight;font.pixelSize: Theme.headingSize;font.weight: Font.DemiBold;color: Theme.text}
                                Label {Layout.fillWidth: true;text: widget.workspace.todayTasks.count;font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight;Accessible.name: qsTr("%1 项待办").arg(text)}
                            }
                            Label {
                                width: parent.width
                                text: Qt.formatDate(new Date(widget.workspace.dateOffset(0)+"T12:00:00"),qsTr("M月d日"))
                                font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight
                            }
                        }
                        PointHandler {id:headerPress;acceptedButtons:Qt.LeftButton}
                        DragHandler {id: headerDrag;target: null;enabled: !widget.controller.locked;onActiveChanged:{if(active)widget.controller.startMove();else widget.controller.finishMove()} cursorShape: Qt.OpenHandCursor}
                    }
                    WidgetAction {Layout.alignment:Qt.AlignTop;objectName: "widgetPin";symbol: "pin";checked: widget.controller.alwaysOnTop;Accessible.checkable: true;Accessible.checked: checked;description: checked ? qsTr("取消置顶") : qsTr("置顶组件");onClicked: widget.controller.setAlwaysOnTop(!widget.controller.alwaysOnTop)}
                    WidgetAction {Layout.alignment:Qt.AlignTop;objectName: "widgetCollapse";visible:widget.controller.canCompact;symbol: widget.controller.growUp ? "down" : "up";description: widget.controller.selectedForm===2 ? qsTr("返回悬浮球") : qsTr("收起任务组件");onClicked: widget.controller.returnToCompact()}
                    WidgetAction {
                        id:widgetMore
                        Layout.alignment:Qt.AlignTop
                        objectName: "widgetMore";description: qsTr("组件菜单");hintText: qsTr("调整组件外观与行为")
                        onClicked: widget.openVerificationMenu()
                    }
                }
                Label {
                    id: widgetError
                    objectName: "widgetError"
                    Layout.fillWidth: true
                    visible: widget.errorMessage.length>0
                    text: widget.errorMessage;wrapMode: Text.WordWrap
                    font.pixelSize: Theme.captionSize;color: Theme.danger
                    maximumLineCount: 3;elide: Text.ElideRight
                    Accessible.name: widget.errorMessage
                    ToolTip.visible: !Theme.hintsBlocked && errorHover.hovered && truncated;ToolTip.text: widget.errorMessage
                    ToolTip.delay: Theme.hintDelay
                    ToolTip.timeout: Theme.hintTimeout
                    HoverHandler {id: errorHover}
                }
                Item {
                    Layout.fillWidth: true;Layout.fillHeight: true
                    // A wrapped error row plus a taller CJK header can squeeze this to
                    // zero height; keep at least one row visible so verifyLayout()'s
                    // todayList.height>=24 check reflects a usable widget.
                    Layout.minimumHeight: 24
                    Column {
                        anchors.centerIn: parent;width: parent.width
                        visible: todayList.count===0 && todayList.contentHeight<1
                        spacing: 6
                        AppIcon {anchors.horizontalCenter: parent.horizontalCenter;name: widget.workspace.ready ? "check" : "clock";ink: Theme.secondary}
                        Label {width: parent.width;horizontalAlignment: Text.AlignHCenter;text: widget.workspace.ready ? qsTr("今天暂无待办") : qsTr("正在加载…");color: Theme.secondary;wrapMode: Text.Wrap}
                    }
                    ListView {
                        id: todayList
                        objectName: "widgetTasks"
                        anchors.fill: parent
                        cacheBuffer: 0;clip: true;spacing: 2
                        model: widget.workspace.todayTasks
                        activeFocusOnTab: count>0
                        DS.ScrollPhysics {target: todayList}
                        ScrollBar.vertical: ScrollBar {}
                        Keys.onReturnPressed: {const row=currentItem as WidgetTaskRow;if(row)widget.openTaskRequested(row.taskId)}
                        Keys.onSpacePressed: {const row=currentItem as WidgetTaskRow;if(row)row.complete()}
                        delegate: WidgetTaskRow {
                            width: todayList.width-Theme.scrollGutter
                            workspace: widget.workspace
                            reduceMotion: widget.reduceMotion
                            dragLayer:widget.contentItem
                            highlighted: todayList.activeFocus && todayList.currentIndex===index
                            onOpenRequested: function(id){widget.openTaskRequested(id)}
                            onActionStarted:widget.taskError=""
                            onErrorReported:function(message){widget.taskError=message}
                            onContextRequested:function(id,source,position){todayList.currentIndex=index;widget.openTaskMenu(id,source,position)}
                            onDragStateChanged:function(active,position){if(active)todayList.currentIndex=index;widget.taskDragging=active;widget.taskDragPoint=position}
                            onReorderStarted:widget.taskOperation="reorder"
                        }
                    }
                }
                Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator}
                RowLayout {
                    id: footer
                    Layout.fillWidth: true
                    spacing: 4
                    ActionButton {id: addTask;objectName: "widgetAdd";Layout.fillWidth:true;Layout.minimumWidth:32;contentAlignment:Qt.AlignLeft;visible:!widget.addingTask;symbol: "plus";text: qsTr("添加任务");enabled: widget.workspace.ready && !widget.workspace.busy;onClicked:{widget.addingTask=true;quickTitle.forceActiveFocus()}}
                    TextField {
                        id:quickTitle;objectName:"widgetQuickTitle";Layout.fillWidth:true;Layout.minimumWidth:0;visible:widget.addingTask
                        placeholderText:qsTr("添加到今天，Enter 保存");Accessible.name:qsTr("小窗快速添加任务")
                        readOnly:widget.addingPending;enabled:widget.workspace.ready
                        onAccepted:widget.submitTask()
                        Keys.onEscapePressed:if(!text.length && !widget.addingPending){widget.addingTask=false;addTask.forceActiveFocus()}
                    }
                    WidgetAction {objectName:"widgetQuickSubmit";visible:widget.addingTask;symbol:"plus";description:qsTr("添加到今天");enabled:quickTitle.text.trim().length>0 && !widget.workspace.busy && !widget.addingPending;onClicked:widget.submitTask()}
                    WidgetAction {objectName:"widgetUndoDelete";symbol:"undo";description:qsTr("撤销删除");visible:widget.localDeletion && widget.workspace.canUndo && !widget.addingTask;enabled:!widget.workspace.busy;onClicked:{widget.taskOperation="restore";if(!widget.workspace.undoDelete())widget.taskError=widget.workspace.errorText}}
                    WidgetAction {objectName: "widgetOpenMain";symbol: "window";description: qsTr("打开主窗口");onClicked: widget.openMainRequested()}
                }
            }
            }
            RowLayout {
                id:miniRow
                layoutDirection:widget.controller.growLeft ? Qt.RightToLeft : Qt.LeftToRight
                visible:widget.miniReveal>0.001 || widget.controller.effectiveForm===1
                x:widget.controller.miniRect.x-surface.x+Theme.panelPadding;y:widget.controller.miniRect.y-surface.y+Theme.panelPadding
                width:widget.controller.miniRect.width-2*Theme.panelPadding;height:widget.controller.miniRect.height-2*Theme.panelPadding
                opacity:widget.miniReveal
                spacing: 2
                ToolButton {
                    id:miniGrip;objectName:"widgetMiniGrip"
                    Layout.preferredWidth:Math.max(32,Theme.iconSize+8);Layout.fillHeight:true
                    focusPolicy:Qt.NoFocus;padding:4
                    Accessible.name:widget.controller.selectedForm===2 ? qsTr("打开任务或拖动 Mini 条") : qsTr("拖动 Mini 条")
                    onClicked:if(widget.controller.selectedForm===2)widget.controller.activatePrimary()
                    contentItem:AppIcon {name:"today";ink:Theme.secondary}
                    background:Rectangle {radius:Theme.controlRadius;color:miniGrip.down ? Theme.pressed : miniGrip.hovered ? Theme.hover : "transparent"}
                    PointHandler {id:miniPress;acceptedButtons:Qt.LeftButton}
                    DragHandler {id:miniDrag;target:null;enabled:!widget.controller.locked;onActiveChanged:{if(active)widget.controller.startMove();else widget.controller.finishMove()} cursorShape:Qt.OpenHandCursor}
                }
                ToolButton {
                    id: miniSummary
                    objectName: "widgetMiniSummary"
                    Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumWidth: 0
                    padding: 4
                    Accessible.name: (widget.workspace.ready ? qsTr("今天 %1 项，%2").arg(widget.workspace.todayTasks.count).arg(widget.workspace.firstTodayTitle || qsTr("暂无待办")) : qsTr("正在加载"))+(widget.errorMessage.length ? ", "+widget.errorMessage : "")+"。"+widget.primaryActionName
                    onClicked: widget.controller.activatePrimary()
                    Keys.onReturnPressed: widget.controller.activatePrimary()
                    contentItem: RowLayout {
                        spacing: 6
                        Label {text: widget.workspace.todayTasks.count;font.pixelSize: Theme.bodySize;font.weight: Font.DemiBold;color: Theme.text;visible: widget.workspace.ready}
                        Label {id: miniTitle;objectName: "widgetMiniTitle";Layout.fillWidth: true;Layout.minimumWidth: 0;text: widget.workspace.ready ? (widget.workspace.firstTodayTitle || qsTr("今天暂无待办")) : qsTr("正在加载…");textFormat: Text.PlainText;font.pixelSize: Theme.bodySize;color: Theme.text;elide: Text.ElideRight}
                    }
                    ToolTip.visible: !Theme.hintsBlocked && hovered && !down && !widget.interacting && (miniTitle.truncated || widget.errorMessage.length>0)
                    ToolTip.delay: Theme.hintDelay
                    ToolTip.timeout: Theme.hintTimeout
                    ToolTip.text: widget.errorMessage || widget.workspace.firstTodayTitle
                }
                WidgetAction {Layout.alignment:Qt.AlignTop;id:miniMore;objectName:"miniMore";symbol:"more";description:qsTr("组件菜单");onPressedChanged:if(pressed)widget.miniKeyboardInteraction=false;onClicked:widget.openVerificationMenu()}
            }
            // Block pointer delivery without changing content enabled/palette
            // or rendering a dimmer over the transparent desktop surface.
            MouseArea {
                id:inputShield;objectName:"widgetCommandInputShield"
                Keys.onPressed:function(event){event.accepted=true}
                anchors.fill:parent;z:1000
                visible:widget.commandInputBlocked || widget.motionInputBlocked
                acceptedButtons:Qt.AllButtons
                preventStealing:true
                onWheel:function(wheel){wheel.accepted=true}
            }
            MouseArea {
                anchors.right: parent.right;anchors.bottom: parent.bottom
                enabled:!widget.commandInputBlocked && !widget.motionInputBlocked
                width: 12;height: 12
                visible: !widget.controller.mini && !widget.controller.panelMotion && !widget.controller.locked
                cursorShape: Qt.SizeFDiagCursor
                onPressed:widget.controller.startResize()
                onPositionChanged:function(mouse){if(pressed)widget.controller.resizePanel(mapToGlobal(mouse.x,mouse.y))}
                onReleased:widget.controller.finishMove()
                onCanceled:widget.controller.finishMove()
                Accessible.name: qsTr("调整组件大小")
            }
          }
        }
    }
}
