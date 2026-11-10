pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Templates as T

Window {
    id: widget
    component NavigationSurface: Rectangle {
        required property T.AbstractButton control
        property bool keyboardFocus: control.visualFocus
        radius: Theme.controlRadius
        color: !control.enabled ? "transparent" : control.down ? Theme.pressed
             : control.checked ? Theme.navigationSelection : control.hovered ? Theme.hover : "transparent"
        border.width: keyboardFocus ? 1 : 0
        border.color: Theme.accent
    }
    required property WidgetViewModel controller
    required property TaskWorkspace workspace
    signal openMainRequested()
    signal openTaskRequested(string taskId)
    signal settingsRequested()
    objectName: "widgetWindow"
    width: 340;height: 360
    readonly property int ballDiameter: Math.max(36,Theme.iconSize+12)
    readonly property real miniControlHeight: Math.max(Theme.controlHeight,miniMore.implicitHeight)
    readonly property int miniBarHeight: Math.ceil(miniControlHeight+2*Theme.panelPadding)
    readonly property real shellCorner: 12
    readonly property real shellInset: 2
    onMiniBarHeightChanged: Qt.callLater(syncMiniLayout)
    onBallDiameterChanged: Qt.callLater(syncMiniLayout)
    Component.onCompleted: {syncMiniLayout();controller.enableVisualTransitions();Qt.callLater(syncPanelMinimum);Qt.callLater(setMiniAnimation);Qt.callLater(syncBrowsePresentation)}
    minimumWidth:ballDiameter;minimumHeight:ballDiameter
    readonly property int panelMinimumHeight:Math.ceil(Math.max(190,header.implicitHeight+navigationBar.implicitHeight+footer.implicitHeight+(widgetError.visible?widgetError.implicitHeight:0)+(retryBrowse.visible?retryBrowse.implicitHeight:0)+2*Theme.panelPadding+6*Theme.fieldGap+48+Theme.listInset*2+(controller.panelRect.width<300?footer.implicitHeight:0)))
    onPanelMinimumHeightChanged:Qt.callLater(syncPanelMinimum)
    function syncPanelMinimum(){controller.setPanelMinimumSize(260,panelMinimumHeight)}
    x: 80;y: 80;visible: false;color: "transparent"
    title:qsTr("Daymark · %1").arg(workspace.widgetContextTitle)
    property string taskError: ""
    property bool addingTask: false
    property bool addingPending: false
    property bool categoryPickerOpen:false
    property bool managingShortcuts:false
    property var draftContext:null
    property var pendingBrowse:null
    property var browsePositions:({})
    property string browseProfile:""
    property string shownBrowseKey:""
    readonly property bool draftWrongProfile:!!draftContext && draftContext.profile!==workspace.widgetProfileToken
    readonly property string emptyTaskText:workspace.widgetContext==="today" ? qsTr("今天暂无待办") : workspace.widgetCategoryToday ? qsTr("该分类今天暂无待办") : qsTr("该分类暂无未完成任务")
    function captureDraftContext(){return {key:workspace.widgetContext,title:workspace.widgetContextTitle,todayOnly:workspace.widgetCategoryToday,profile:workspace.widgetProfileToken}}
    function applyBrowse(intent) {
        const accepted=intent.kind==="range" ? workspace.setWidgetCategoryToday(intent.todayOnly) : workspace.setWidgetContext(intent.key)
        if(accepted)categoryPickerOpen=false
        else taskError=workspace.widgetBrowseError || workspace.errorText || qsTr("暂时无法切换，请稍后重试。")
    }
    function requestBrowse(intent) {
        if(addingPending || workspace.busy || taskDragging || motionInputBlocked)return
        if(quickTitle.text.trim().length){pendingBrowse=intent;submitTask()}
        else {quickTitle.clear();applyBrowse(intent)}
    }
    function requestMenuBrowse(intent) {
        // A context change rebuilds the category actions. Close the cascade
        // before replacing its model or the triggering item can vanish mid-close.
        categoryMenu.close();widgetMenu.close()
        Qt.callLater(function(){widget.requestBrowse(intent)})
    }
    function openCategoryPicker(){
        if(controller.mini || motionInputBlocked || addingPending || taskDragging)return
        widgetMenu.close();taskMenu.close();categorySearch.clear();categoryPickerOpen=true
        Qt.callLater(function(){if(categoryPickerOpen)categorySearch.forceActiveFocus()})
    }
    onCategoryPickerOpenChanged: {
        reportInteraction()
        if(!categoryPickerOpen && active && !controller.mini)Qt.callLater(function(){if(!motionInputBlocked)(addingTask?quickTitle:categoryButton).forceActiveFocus()})
    }
    function stashBrowsePosition() {
        if(!shownBrowseKey || browseProfile!==workspace.widgetProfileToken)return
        const row=todayList.indexAt(1,todayList.contentY+1)
        const item=todayList.itemAtIndex(row) as WidgetTaskRow
        const current=todayList.currentItem as WidgetTaskRow
        browsePositions[shownBrowseKey]={anchor:item?item.taskId:"",offset:item?todayList.contentY-item.y:0,current:current?current.taskId:""}
    }
    function restoreBrowsePosition() {
        todayList.forceLayout()
        const state=browsePositions[workspace.widgetBrowseKey]
        if(!state){todayList.positionViewAtBeginning();todayList.currentIndex=todayList.count?0:-1;return}
        const anchor=workspace.widgetTaskIndex(state.anchor)
        if(anchor>=0){todayList.positionViewAtIndex(anchor,ListView.Beginning);todayList.contentY=Math.max(todayList.originY,Math.min(todayList.originY+Math.max(0,todayList.contentHeight-todayList.height),todayList.contentY+state.offset))}
        else todayList.positionViewAtBeginning()
        const current=workspace.widgetTaskIndex(state.current);todayList.currentIndex=current>=0?current:todayList.count?Math.max(0,anchor):-1
    }
    function syncBrowsePresentation() {
        if(browseProfile!==workspace.widgetProfileToken){browsePositions=({});browseProfile=workspace.widgetProfileToken;shownBrowseKey="";categoryPickerOpen=false;categorySearch.clear()}
        if(shownBrowseKey!==workspace.widgetBrowseKey){shownBrowseKey=workspace.widgetBrowseKey;Qt.callLater(restoreBrowsePosition)}
    }
    function focusBrowse(step) {
        const buttons=[todayButton]
        for(let i=0;i<shortcutButtons.count;i++)buttons.push(shortcutButtons.itemAt(i))
        buttons.push(categoryButton)
        const index=buttons.indexOf(activeFocusItem),next=(Math.max(0,index)+step+buttons.length)%buttons.length
        buttons[next].forceActiveFocus(Qt.TabFocusReason)
    }
    property bool taskDragging: false
    property point taskDragPoint: Qt.point(0,0)
    property var contextTask: ({})
    property var pendingTaskContext:null
    property Item pendingTaskSource:null
    onPendingTaskContextChanged:reportInteraction()
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
        if(addingPending || quickTitle.inputMethodComposing || !quickTitle.text.trim().length)return
        if(!draftContext)draftContext=captureDraftContext()
        taskError="";addingPending=workspace.createWidgetTask(quickTitle.text,draftContext.key,draftContext.todayOnly,draftContext.profile,widgetTimePreview.ignored,widgetTimePreview.overrides)
        if(!addingPending){pendingBrowse=null;categoryPickerOpen=false;taskError=workspace.widgetBrowseError || workspace.errorText || qsTr("暂时无法添加，请稍后重试。")}
    }
    function openTaskMenu(id,source,position) {
        if(commandInputBlocked || motionInputBlocked || !source)return
        pendingTaskSource=source
        pendingTaskContext={taskId:id,position:position,transition:controller.transitionId}
        Qt.callLater(presentTaskMenu)
    }
    function presentTaskMenu() {
        const request=pendingTaskContext,source=pendingTaskSource;pendingTaskContext=null;pendingTaskSource=null
        if(!request || !visible || controller.mini || controller.clickThrough || commandInputBlocked || motionInputBlocked || request.transition!==controller.transitionId || !source || !source.visible || !source.enabled)return
        contextTask=workspace.task(request.taskId)
        if(!contextTask.taskId)return
        source.forceActiveFocus(Qt.PopupFocusReason)
        taskMenu.popupAtPoint(source,request.position)
    }
    function restoreTaskMenuFocus() {
        const source=taskMenu.anchorItem
        if(active && visible && !controller.mini && !commandMenuOpen && !deleteTaskConfirmation.visible && source && source.visible && source.enabled)source.forceActiveFocus(Qt.PopupFocusReason)
    }
    function taskAction(action) {
        taskError=""
        const id=contextTask.taskId,version=contextTask.rowVersion
        let accepted=false
        if(action==="pin" || action==="important" || action==="cancelReminder") {
            const patch=action==="pin" ? {pinned:!contextTask.pinned} : action==="important" ? {important:!contextTask.important} : {remindAtUtc:"",reminderRepeat:"NONE"}
            taskOperation="details";accepted=workspace.saveDetails(id,version,patch)
        } else if(action==="up" || action==="down") {
            taskOperation="reorder";accepted=workspace.moveWidgetTask(id,action==="up" ? -1 : 1)
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
    readonly property string errorMessage: (draftWrongProfile ? qsTr("资料已切换，原输入未保存。请返回原资料，或复制内容后清空输入。") : "") || taskError || workspace.widgetBrowseError || controller.errorText
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
    property real fromRadius:shellCorner
    property real toRadius:shellCorner
    readonly property rect frameRect:Qt.rect(fromRect.x+(toRect.x-fromRect.x)*motionProgress,fromRect.y+(toRect.y-fromRect.y)*motionProgress,fromRect.width+(toRect.width-fromRect.width)*motionProgress,fromRect.height+(toRect.height-fromRect.height)*motionProgress)
    readonly property real visualWidth:frameRect.width
    readonly property real visualHeight:frameRect.height
    readonly property real frameRadius:fromRadius+(toRadius-fromRadius)*motionProgress
    readonly property real panelReveal:fromPanel+(toPanel-fromPanel)*motionProgress
    readonly property real panelContentOpacity:toPanel>fromPanel ? fromOpacity+(1-fromOpacity)*Math.max(0,(motionProgress-0.2)/0.8) : toPanel<fromPanel ? fromOpacity*Math.max(0,1-motionProgress*2.5) : fromOpacity+(toPanel-fromOpacity)*motionProgress
    readonly property real miniReveal:fromMini+(toMini-fromMini)*motionProgress
    readonly property real ballReveal: Math.max(0,1-panelReveal-miniReveal)
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
    function reportInteraction() {controller.setInteraction(miniHover.hovered,interactionPressed || interacting,widgetMenu.visible || taskMenu.visible || deleteTaskConfirmation.visible || pendingTaskContext!==null || categoryPickerOpen,miniKeyboardFocus)}
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
        widgetMenu.close();taskMenu.close();if(controller.effectiveForm!==0)categoryPickerOpen=false;pendingWidgetMenu=false;pendingTaskContext=null;pendingTaskSource=null
        const currentRect=frameRect,currentPanel=panelReveal,currentMini=miniReveal,currentRadius=frameRadius,currentOpacity=panelContentOpacity
        motionAnimation.stop();visualTransition=controller.transitionId
        fromRect=currentRect;fromPanel=currentPanel;fromMini=currentMini;fromRadius=currentRadius;fromOpacity=currentOpacity
        toRect=targetRect();toPanel=controller.effectiveForm===0?1:0;toMini=controller.effectiveForm===1?1:0
        toRadius=controller.effectiveForm===2?ballDiameter/2:shellCorner
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
        function onWidgetContextChanging(){widget.stashBrowsePosition()}
        function onWidgetBrowseChanged(){widget.syncBrowsePresentation()}
        function onWidgetCategoryRangeRequested(enabled){widget.requestBrowse({kind:"range",todayOnly:enabled})}
        function onOperationFinished(operation,success) {
            if(operation==="widgetCreate" && widget.addingPending){
                widget.addingPending=false
                const intent=widget.pendingBrowse;widget.pendingBrowse=null
                if(success){quickTitle.clear();if(intent)widget.applyBrowse(intent);if(widget.active && widget.addingTask)quickTitle.forceActiveFocus();else widget.addingTask=false}
                else {widget.categoryPickerOpen=false;widget.taskError=widget.workspace.errorText;if(widget.active)quickTitle.forceActiveFocus()}
            }
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
    function closeOrCompact() {if(categoryPickerOpen)categoryPickerOpen=false;else if(taskMenu.visible)taskMenu.close();else if(deleteTaskConfirmation.visible)deleteTaskConfirmation.close();else if(taskDragging){const row=todayList.currentItem as WidgetTaskRow;if(row)row.finishDrag(false)}else if(addingTask){if(!quickTitle.text.length){addingTask=false;addTask.forceActiveFocus()}}else if(widgetMenu.visible)widgetMenu.close();else if(!controller.mini)controller.returnToCompact()}
    Shortcut {sequence:"Escape";enabled:widget.active;onActivated:widget.closeOrCompact()}
    function focusedTaskRow(){
        let item=activeFocusItem
        while(item && item!==todayList){if(item instanceof WidgetTaskRow)return item;item=item.parent}
        return activeFocusItem===todayList ? todayList.currentItem : null
    }
    Shortcut {sequences:["Shift+F10","Menu"];enabled:widget.active && !widget.motionInputBlocked;onActivated:{const row=widget.controller.mini?null:widget.focusedTaskRow();if(row && !widget.addingTask)widget.openTaskMenu(row.taskId,row,Qt.point(row.width/2,row.height/2));else widget.openVerificationMenu()}}
    Rectangle {
        id:categoryPage;objectName:"widgetCategoryPage";parent:panelContent
        anchors.fill:parent;anchors.margins:widget.shellInset;z:5;visible:widget.categoryPickerOpen
        color:Theme.content;radius:widget.shellCorner-widget.shellInset;border.width:0
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:function(wheel){wheel.accepted=true}}
        ColumnLayout {
            anchors.fill:parent;anchors.margins:Theme.panelPadding;spacing:Theme.fieldGap
            RowLayout {
                Layout.fillWidth:true
                Label {Layout.fillWidth:true;text:widget.managingShortcuts?qsTr("常用分类"):qsTr("选择分类");font.pixelSize:Theme.panelTitleSize;font.weight:Font.DemiBold;color:Theme.text;elide:Text.ElideRight}
                WidgetAction {objectName:"widgetCategoryClose";symbol:"close";description:qsTr("关闭分类选择");onClicked:widget.categoryPickerOpen=false}
            }
            TextField {
                id:categorySearch;objectName:"widgetCategorySearch";Layout.fillWidth:true;Layout.minimumWidth:0
                placeholderText:qsTr("搜索分类名称或路径");Accessible.name:qsTr("搜索分类名称或路径")
                background: DS.FieldSurface {quietOutline:true;focused:categorySearch.activeFocus;enabledField:categorySearch.enabled}
                Keys.onDownPressed:if(categoryChoices.count){categoryChoices.currentIndex=0;categoryChoices.forceActiveFocus()}
                Keys.onEscapePressed:widget.categoryPickerOpen=false
            }
            Item {
                Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:32
                ListView {
                    id:categoryChoices;objectName:"widgetCategoryChoices";anchors.fill:parent;clip:true
                    readonly property var entries:widget.managingShortcuts?widget.workspace.widgetShortcuts:widget.workspace.widgetCategories
                    model:entries.filter(item=>!categorySearch.text || item.path.toLocaleLowerCase().indexOf(categorySearch.text.toLocaleLowerCase())>=0)
                    DS.ScrollPhysics {target:categoryChoices}
                    ScrollBar.vertical:ScrollBar {}
                    Keys.onReturnPressed:if(categoryChoices.currentIndex>=0 && categoryChoices.currentIndex<categoryChoices.count)widget.requestBrowse({kind:"context",key:categoryChoices.model[categoryChoices.currentIndex].key})
                    Keys.onEscapePressed:widget.categoryPickerOpen=false
                    delegate:ItemDelegate {
                        id:categoryEntry
                        required property var modelData
                        required property int index
                        objectName:"widgetCategoryChoice-"+modelData.key
                        width:categoryChoices.width-Theme.scrollGutter
                        leftPadding:8+(!categorySearch.text && !widget.managingShortcuts?modelData.depth*12*Theme.scale:0)
                        checkable:true;checked:widget.workspace.widgetContext===modelData.key
                        highlighted:activeFocus || (categoryChoices.activeFocus && categoryChoices.currentIndex===index)
                        Accessible.name:modelData.path
                        enabled:!widget.workspace.busy && !widget.addingPending
                        onClicked:widget.requestBrowse({kind:"context",key:modelData.key})
                        background: NavigationSurface {control:categoryEntry;keyboardFocus:categoryEntry.highlighted}
                        contentItem:RowLayout {
                            spacing:4
                            Label {id:categoryName;Layout.fillWidth:true;Layout.minimumWidth:0;text:categorySearch.text || widget.managingShortcuts?categoryEntry.modelData.path:categoryEntry.modelData.title;color:Theme.text;elide:Text.ElideRight;DS.ToolTip {visible:!Theme.hintsBlocked && categoryName.truncated && nameHover.hovered;text:categoryEntry.modelData.path} HoverHandler {id:nameHover}}
                            WidgetAction {visible:widget.managingShortcuts;symbol:"up";description:qsTr("上移常用分类");enabled:categoryEntry.index>0;onClicked:{widget.workspace.moveWidgetShortcut(categoryEntry.modelData.key,-1);Qt.callLater(function(){categoryChoices.forceActiveFocus()})}}
                            WidgetAction {visible:widget.managingShortcuts;symbol:"down";description:qsTr("下移常用分类");enabled:categoryEntry.index<categoryChoices.count-1;onClicked:{widget.workspace.moveWidgetShortcut(categoryEntry.modelData.key,1);Qt.callLater(function(){categoryChoices.forceActiveFocus()})}}
                            WidgetAction {objectName:"widgetCategoryPin-"+categoryEntry.modelData.key;symbol:"pin";checked:widget.managingShortcuts || !!categoryEntry.modelData.favorite;description:checked?qsTr("取消常用分类"):qsTr("固定为常用分类");onClicked:{widget.workspace.setWidgetShortcut(categoryEntry.modelData.key,!checked);Qt.callLater(function(){categorySearch.forceActiveFocus()})}}
                        }
                    }
                }
                Label {anchors.centerIn:parent;width:parent.width;text:categorySearch.text?qsTr("没有匹配的分类"):widget.managingShortcuts?qsTr("尚未固定常用分类"):qsTr("还没有可用分类，可在主窗口侧栏创建。");horizontalAlignment:Text.AlignHCenter;wrapMode:Text.Wrap;color:Theme.secondary;visible:categoryChoices.count===0}
            }
            RowLayout {
                Layout.fillWidth:true
                ToolButton {
                    id: manageShortcuts
                    objectName: "widgetManageShortcuts"
                    text: qsTr("常用管理")
                    checkable: true
                    checked: widget.managingShortcuts
                    font.pixelSize: Theme.captionSize
                    background: NavigationSurface {control:manageShortcuts}
                    onClicked: {
                        widget.managingShortcuts = !widget.managingShortcuts
                        categorySearch.clear()
                    }
                }
                Item {Layout.fillWidth:true;Layout.minimumWidth:0}
                WidgetAction {objectName:"widgetManageCategories";symbol:"window";description:qsTr("在主窗口管理分类");onClicked:{widget.categoryPickerOpen=false;widget.openMainRequested()}}
            }
        }
    }
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
        onClosed:Qt.callLater(widget.restoreTaskMenuFocus)
        MenuItem {text:qsTr("任务详情");onTriggered:widget.openTaskRequested(widget.contextTask.taskId)}
        MenuItem {objectName:"widgetTaskPin";text:widget.contextTask.pinned ? qsTr("取消置顶") : qsTr("置顶");onTriggered:widget.taskAction("pin")}
        MenuItem {objectName:"widgetTaskImportant";text:widget.contextTask.important ? qsTr("取消特别关注") : qsTr("特别关注");onTriggered:widget.taskAction("important")}
        DS.MenuSeparator {}
        MenuItem {objectName:"widgetTaskUp";text:qsTr("上移");enabled:widget.workspace.canMoveWidgetTask(widget.contextTask.taskId || "",-1);onTriggered:widget.taskAction("up")}
        MenuItem {objectName:"widgetTaskDown";text:qsTr("下移");enabled:widget.workspace.canMoveWidgetTask(widget.contextTask.taskId || "",1);onTriggered:widget.taskAction("down")}
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
        DS.Menu {
            id:categoryMenu;objectName:"widgetCategoryMenu";title:qsTr("分类：%1").arg(widget.workspace.widgetContextTitle)
            popupType:widgetMenu.popupType;anchorItem:widgetMenu.anchorItem;preserveSubmenuPosition:true
            MenuItem {objectName:"widgetMenuToday";text:qsTr("今天");checkable:true;checked:widget.workspace.widgetContext==="today";onTriggered:widget.requestMenuBrowse({kind:"context",key:"today"})}
            DS.MenuSeparator {}
            Instantiator {
                model:widget.workspace.widgetCategories
                delegate:MenuItem {
                    required property var modelData
                    objectName:"widgetMenuCategory-"+modelData.key;text:modelData.path
                    checkable:true;checked:widget.workspace.widgetContext===modelData.key
                    onTriggered:widget.requestMenuBrowse({kind:"context",key:modelData.key})
                }
                onObjectAdded:function(index,object){categoryMenu.insertItem(index+2,object)}
                onObjectRemoved:function(index,object){categoryMenu.removeItem(object)}
            }
            MenuItem {text:qsTr("尚无可用分类");enabled:false;visible:widget.workspace.widgetCategories.length===0}
            DS.MenuSeparator {}
            MenuItem {objectName:"widgetMenuCategoryToday";text:qsTr("分类只看今天");checkable:true;checked:widget.workspace.widgetCategoryToday;onTriggered:widget.requestMenuBrowse({kind:"range",todayOnly:!widget.workspace.widgetCategoryToday})}
        }
        DS.MenuSeparator {}
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
          Item {
            id: surface
            x:widget.frameRect.x;y:widget.frameRect.y
            width: widget.visualWidth
            height: widget.visualHeight
            clip:true
            objectName: "widgetBackground"
            readonly property color ballFill: miniBall.down ? Theme.pressed : miniBall.hovered ? Theme.primaryFill : Theme.content
            readonly property color fill: Qt.tint(Theme.content,Qt.rgba(ballFill.r,ballFill.g,ballFill.b,widget.ballReveal))
            readonly property real panelAlpha: widget.workspace.preferences.solidWidget || Theme.highContrast ? 1 : widget.controller.backgroundOpacity
            readonly property color color: Qt.rgba(fill.r,fill.g,fill.b,panelAlpha+(1-panelAlpha)*widget.ballReveal)
            // The native input region is integer-clipped. Keep the painted
            // antialiased edge inside it in every frame.
            Rectangle {
                objectName: "widgetShell"
                anchors.fill: parent
                anchors.margins: widget.shellInset
                radius: Math.max(0,widget.frameRadius-widget.shellInset)
                antialiasing: true
                color: surface.color
                border.color: Theme.highContrast ? Theme.separator : Qt.rgba(Theme.text.r,Theme.text.g,Theme.text.b,Theme.dark ? 0.18 : 0.12)
                border.pixelAligned: false
            }
            HoverHandler {id:miniHover;onHoveredChanged:widget.reportInteraction()}
            ToolButton {
                id:miniBall;objectName:"widgetMiniBall"
                visible:opacity>0.001 || widget.controller.effectiveForm===2
                width:widget.ballDiameter;height:widget.ballDiameter
                x:widget.controller.ballRect.x-surface.x
                y:widget.controller.ballRect.y-surface.y
                padding:8
                opacity:Math.max(0,1-(widget.panelReveal+widget.miniReveal)*3)
                Accessible.name:qsTr("%1 %2 项").arg(widget.workspace.widgetContextTitle).arg(widget.workspace.widgetTasks.count)+"，"+widget.primaryActionName
                onClicked:if(!ballDrag.active)widget.controller.activatePrimary()
                Keys.onReturnPressed:widget.controller.activatePrimary()
                onActiveFocusChanged:if(activeFocus && [Qt.TabFocusReason,Qt.BacktabFocusReason,Qt.ShortcutFocusReason].indexOf(focusReason)>=0)widget.miniKeyboardInteraction=true
                TapHandler {acceptedButtons:Qt.RightButton;enabled:!widget.commandInputBlocked && !widget.motionInputBlocked;onTapped:widget.openVerificationMenu()}
                PointHandler {id:ballPress;acceptedButtons:Qt.LeftButton}
                WidgetMoveHandle {id:ballDrag;controller:widget.controller}
                contentItem:Item {
                    implicitWidth:24;implicitHeight:24
                    BrandMark {objectName:"widgetBrandMark";anchors.fill:parent;monochrome:true;ink:Theme.primaryInk;visible:!widget.controller.errorText.length;Accessible.ignored:true}
                    AppIcon {anchors.fill:parent;visible:widget.controller.errorText.length>0;name:"info";ink:Theme.danger}
                }
                background: Rectangle {
                    anchors.fill: parent
                    anchors.margins: 5
                    radius: width/2
                    color: "transparent"
                    border.width: 2
                    border.color: Theme.accent
                    visible: miniBall.visualFocus
                }
            }
            Item {
                id:panelContent;objectName:"widgetPanelContent"
                visible:widget.panelContentOpacity>0.001 || widget.controller.effectiveForm===0
                width:widget.controller.panelRect.width;height:widget.controller.panelRect.height
                x:widget.controller.panelRect.x-surface.x
                y:widget.controller.panelRect.y-surface.y
                opacity:widget.panelContentOpacity
              ColumnLayout {
                enabled:!widget.categoryPickerOpen
                anchors.fill: parent;anchors.margins: Theme.panelPadding
                spacing: Theme.fieldGap
                RowLayout {
                    id: header
                    objectName: "widgetHeader"
                    Layout.fillWidth: true
                    spacing: 2
                    Item {
                        objectName:"widgetHeaderGrip"
                        Layout.fillWidth: true;Layout.minimumWidth: 0
                        implicitHeight: headerText.implicitHeight
                        Column {
                            id: headerText
                            width: parent.width
                            RowLayout {
                                width: parent.width
                                spacing: 6
                                Label {
                                    id: widgetTitleText
                                    objectName: "widgetContextTitle"
                                    Layout.maximumWidth: Math.max(0,headerText.width-Math.min(taskCount.implicitWidth,taskCount.Layout.maximumWidth)-12)
                                    Layout.minimumWidth: 0
                                    text: widget.workspace.widgetContextTitle
                                    elide: Text.ElideRight
                                    font.pixelSize: Theme.panelTitleSize
                                    font.weight: Font.DemiBold
                                    color: Theme.text
                                    DS.ToolTip {visible:!Theme.hintsBlocked && widgetTitleText.truncated && titleHover.hovered;text:widgetTitleText.text}
                                    HoverHandler {id:titleHover}
                                }
                                Label {id:taskCount;Layout.maximumWidth:48*Theme.scale;text: widget.workspace.widgetTasks.count;font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight;Accessible.name: qsTr("%1 项待办").arg(text)}
                                Item {Layout.fillWidth:true;Layout.minimumWidth:0}
                            }
                            Label {
                                width: parent.width
                                objectName:"widgetContextRange"
                                text: widget.workspace.widgetRangeLabel || Qt.formatDate(new Date(widget.workspace.dateOffset(0)+"T12:00:00"),qsTr("M月d日"))
                                font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight
                            }
                        }
                        PointHandler {id:headerPress;acceptedButtons:Qt.LeftButton}
                        WidgetMoveHandle {id:headerDrag;controller:widget.controller}
                    }
                    WidgetAction {Layout.alignment:Qt.AlignTop;visible:widget.controller.panelRect.width>=300*Theme.scale;objectName: "widgetPin";symbol: "pin";checked: widget.controller.alwaysOnTop;Accessible.checkable: true;Accessible.checked: checked;description: checked ? qsTr("取消置顶") : qsTr("置顶组件");onClicked: widget.controller.setAlwaysOnTop(!widget.controller.alwaysOnTop)}
                    WidgetAction {Layout.alignment:Qt.AlignTop;objectName: "widgetCollapse";visible:widget.controller.canCompact;symbol: widget.controller.growUp ? "down" : "up";description: widget.controller.selectedForm===2 ? qsTr("返回悬浮球") : qsTr("收起任务组件");onClicked: widget.controller.returnToCompact()}
                    WidgetAction {
                        id:widgetMore
                        Layout.alignment:Qt.AlignTop
                        objectName: "widgetMore";description: qsTr("组件菜单");hintText: qsTr("调整组件外观与行为")
                        onClicked: widget.openVerificationMenu()
                    }
                }
                RowLayout {
                    id:navigationBar;objectName:"widgetCategoryNavigation"
                    Layout.fillWidth:true;spacing:4
                    readonly property int shortcutCount:widget.workspace.widgetShowShortcuts ? Math.min(3,Math.max(0,Math.floor((width-todayButton.implicitWidth-categoryButton.implicitWidth-3*spacing)/(68*Theme.scale+spacing)))) : 0
                    Accessible.role:Accessible.ToolBar;Accessible.name:qsTr("小窗任务分类")
                    ToolButton {
                        id:todayButton;objectName:"widgetBrowseToday";text:qsTr("今天");font.pixelSize:Theme.captionSize
                        checkable:true;autoExclusive:true;checked:widget.workspace.widgetContext==="today"
                        enabled:widget.workspace.ready && !widget.workspace.busy && !widget.addingPending
                        onClicked:widget.requestBrowse({kind:"context",key:"today"})
                        background: NavigationSurface {control:todayButton}
                        Keys.onLeftPressed:widget.focusBrowse(-1);Keys.onRightPressed:widget.focusBrowse(1)
                        Keys.onReturnPressed:clicked()
                    }
                    Repeater {
                        id:shortcutButtons;model:widget.workspace.widgetShortcuts.slice(0,navigationBar.shortcutCount)
                        delegate:ToolButton {
                            id:shortcutControl
                            required property var modelData
                            objectName:"widgetShortcut-"+modelData.key
                            Layout.preferredWidth:68*Theme.scale;Layout.minimumWidth:0
                            checkable:true;autoExclusive:true;checked:widget.workspace.widgetContext===modelData.key
                            enabled:widget.workspace.ready && !widget.workspace.busy && !widget.addingPending
                            Accessible.name:modelData.path
                            onClicked:widget.requestBrowse({kind:"context",key:modelData.key})
                            background: NavigationSurface {control:shortcutControl}
                            Keys.onLeftPressed:widget.focusBrowse(-1);Keys.onRightPressed:widget.focusBrowse(1)
                            Keys.onReturnPressed:clicked()
                            contentItem:Label {id:shortcutLabel;text:shortcutControl.modelData.title;font.pixelSize:Theme.captionSize;color:Theme.text;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                            DS.ToolTip {visible:!Theme.hintsBlocked && shortcutControl.hovered && shortcutLabel.truncated;text:shortcutControl.modelData.path}
                        }
                    }
                    Item {Layout.fillWidth:true;Layout.minimumWidth:0}
                    ToolButton {
                        id:categoryButton;objectName:"widgetCategoryButton";text:qsTr("分类");font.pixelSize:Theme.captionSize
                        checked:widget.categoryPickerOpen || (widget.workspace.widgetContext!=="today" && !widget.workspace.widgetShortcuts.slice(0,navigationBar.shortcutCount).some(item=>item.key===widget.workspace.widgetContext))
                        enabled:widget.workspace.ready && !widget.workspace.busy && !widget.addingPending
                        Accessible.name:qsTr("选择小窗分类，当前%1").arg(widget.workspace.widgetContextTitle)
                        onClicked:if(widget.categoryPickerOpen)widget.categoryPickerOpen=false;else widget.openCategoryPicker()
                        background: NavigationSurface {control:categoryButton}
                        Keys.onLeftPressed:widget.focusBrowse(-1);Keys.onRightPressed:widget.focusBrowse(1)
                        Keys.onReturnPressed:clicked()
                        contentItem:RowLayout {spacing:4;Label {text:categoryButton.text;font:categoryButton.font;color:Theme.text} AppIcon {name:widget.categoryPickerOpen?"up":"down";Layout.preferredWidth:Theme.iconSize*0.65;Layout.preferredHeight:Theme.iconSize*0.65;ink:Theme.secondary}}
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
                ActionButton {id:retryBrowse;Layout.fillWidth:true;visible:widget.workspace.widgetPreferencesUnsaved;symbol:"reset";text:qsTr("重试保存浏览设置");onClicked:widget.workspace.retryWidgetBrowsePreferences()}
                Item {
                    Layout.fillWidth: true;Layout.fillHeight: true
                    // A wrapped error row plus a taller CJK header can squeeze this to
                    // zero height; keep at least one row visible so verifyLayout()'s
                    // todayList.height>=24 check reflects a usable widget.
                    Layout.minimumHeight: 24
                    Column {
                        anchors.centerIn: parent;width: parent.width
                        visible: !widget.categoryPickerOpen && todayList.count===0 && todayList.contentHeight<1
                        spacing: 6
                        AppIcon {anchors.horizontalCenter: parent.horizontalCenter;name: widget.workspace.ready ? "check" : "clock";ink: Theme.secondary}
                        Label {width: parent.width;horizontalAlignment: Text.AlignHCenter;text: widget.workspace.ready ? widget.emptyTaskText : qsTr("正在加载…");font.pixelSize:Theme.captionSize;color: Theme.secondary;wrapMode: Text.Wrap}
                    }
                    ListView {
                        id: todayList
                        objectName: "widgetTasks"
                        anchors.fill: parent
                        cacheBuffer: 0;clip: true;spacing: 2
                        model: widget.workspace.widgetTasks
                        enabled:!widget.categoryPickerOpen
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
                            showSeparator: index<todayList.count-1 && (!todayList.activeFocus || todayList.currentIndex!==index+1)
                            onOpenRequested: function(id){widget.openTaskRequested(id)}
                            onActionStarted:widget.taskError=""
                            onErrorReported:function(message){widget.taskError=message}
                            onContextRequested:function(id,source,position){todayList.currentIndex=index;widget.openTaskMenu(id,source,position)}
                            onDragStateChanged:function(active,position){if(active)todayList.currentIndex=index;widget.taskDragging=active;widget.taskDragPoint=position}
                            onReorderStarted:widget.taskOperation="reorder"
                        }
                    }
                }
                Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.detailDivider}
                ColumnLayout {
                    id:footer
                    Layout.fillWidth: true
                    spacing: 4
                    Label {objectName:"widgetDraftDestination";Layout.fillWidth:true;visible:widget.addingTask;text:widget.draftWrongProfile?qsTr("原资料中的未保存输入"):widget.draftContext?(widget.draftContext.key==="today"?(widgetTimePreview.hasTime?qsTr("按所选日期安排"):qsTr("添加到今天")):(widgetTimePreview.hasTime?qsTr("添加到 %1").arg(widget.draftContext.title):qsTr("添加到 %1 · %2").arg(widget.draftContext.title).arg(widget.draftContext.todayOnly?qsTr("今天"):qsTr("不设日期")))):qsTr("添加到 %1").arg(widget.workspace.widgetContextTitle);font.pixelSize:Theme.captionSize;color:Theme.secondary;elide:Text.ElideRight}
                  InputTimePreview {
                    id:widgetTimePreview;objectName:"widgetInputTimePreview";objectPrefix:"widgetInput"
                    workspace:widget.workspace;rawText:quickTitle.text;composing:quickTitle.inputMethodComposing
                    enabled:!widget.addingPending && !widget.workspace.busy && !widget.draftWrongProfile
                    visible:widget.addingTask && rawText.trim().length>0 && !composing && summary.length>0
                  }
                  RowLayout {
                    Layout.fillWidth:true;spacing:4
                    ActionButton {id: addTask;objectName: "widgetAdd";Layout.fillWidth:true;Layout.minimumWidth:32;contentAlignment:Qt.AlignLeft;visible:!widget.addingTask;symbol: "plus";text: qsTr("添加任务");enabled: widget.workspace.ready && !widget.workspace.busy;onClicked:{widget.categoryPickerOpen=false;widget.addingTask=true;quickTitle.forceActiveFocus()}}
                    TextField {
                        id:quickTitle;objectName:"widgetQuickTitle";Layout.fillWidth:true;Layout.minimumWidth:0;visible:widget.addingTask
                        background: DS.FieldSurface {quietOutline:true;focused:quickTitle.activeFocus;enabledField:quickTitle.enabled}
                        placeholderText:qsTr("添加到%1，Enter 保存").arg(widget.workspace.widgetContextTitle);Accessible.name:qsTr("小窗快速添加任务")
                        readOnly:widget.addingPending;enabled:widget.workspace.ready
                        onTextChanged:{if(!text.trim().length)widget.draftContext=null;else if(!widget.draftContext)widget.draftContext=widget.captureDraftContext()}
                        onAccepted:widget.submitTask()
                        Keys.onEscapePressed:if(!text.length && !widget.addingPending){widget.addingTask=false;addTask.forceActiveFocus()}
                    }
                    WidgetAction {objectName:"widgetQuickSubmit";visible:widget.addingTask;symbol:"plus";description:qsTr("添加到%1").arg(widget.workspace.widgetContextTitle);enabled:quickTitle.text.trim().length>0 && !widget.workspace.busy && !widget.addingPending && !widget.draftWrongProfile;onClicked:widget.submitTask()}
                    WidgetAction {objectName:"widgetUndoDelete";symbol:"undo";description:qsTr("撤销删除");visible:widget.localDeletion && widget.workspace.canUndo && !widget.addingTask;enabled:!widget.workspace.busy;onClicked:{widget.taskOperation="restore";if(!widget.workspace.undoDelete())widget.taskError=widget.workspace.errorText}}
                    WidgetAction {objectName: "widgetOpenMain";symbol: "window";description: qsTr("打开主窗口");onClicked: widget.openMainRequested()}
                  }
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
                    focusPolicy:Qt.StrongFocus;padding:4
                    Accessible.name:widget.primaryActionName+qsTr("，或拖动 Mini 条")
                    onClicked:widget.controller.activatePrimary()
                    contentItem:AppIcon {name:"today";ink:Theme.secondary}
                    background:NavigationSurface {control:miniGrip}
                    PointHandler {id:miniPress;acceptedButtons:Qt.LeftButton}
                    WidgetMoveHandle {id:miniDrag;controller:widget.controller}
                }
                ToolButton {
                    id: miniSummary
                    objectName: "widgetMiniSummary"
                    Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumWidth: 0
                    padding: 4
                    Accessible.name: (widget.workspace.ready ? qsTr("%1 %2 项，%3").arg(widget.workspace.widgetContextTitle).arg(widget.workspace.widgetTasks.count).arg(widget.workspace.firstWidgetTitle || qsTr("暂无待办")) : qsTr("正在加载"))+(widget.errorMessage.length ? ", "+widget.errorMessage : "")+"。"+widget.primaryActionName
                    onClicked: widget.controller.activatePrimary()
                    Keys.onReturnPressed: widget.controller.activatePrimary()
                    background: Rectangle {
                        radius: Theme.controlRadius
                        color: miniSummary.down ? Theme.pressed : "transparent"
                        border.width: miniSummary.visualFocus ? 1 : 0
                        border.color: Theme.accent
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Label {text: widget.workspace.widgetTasks.count;font.pixelSize: Theme.captionSize;color: Theme.secondary;visible: widget.workspace.ready}
                        Label {id: miniTitle;objectName: "widgetMiniTitle";Layout.fillWidth: true;Layout.minimumWidth: 0;text: widget.workspace.ready ? (widget.workspace.firstWidgetTitle || widget.emptyTaskText) : qsTr("正在加载…");textFormat: Text.PlainText;font.pixelSize: Theme.listTitleSize;color: miniSummary.hovered ? Theme.accent : widget.workspace.firstWidgetTitle ? Theme.text : Theme.secondary;elide: Text.ElideRight}
                    }
                    ToolTip.visible: !Theme.hintsBlocked && hovered && !down && !widget.interacting && (miniTitle.truncated || widget.errorMessage.length>0)
                    ToolTip.delay: Theme.hintDelay
                    ToolTip.timeout: Theme.hintTimeout
                    ToolTip.text: widget.errorMessage || widget.workspace.firstWidgetTitle
                }
                WidgetAction {Layout.alignment:Qt.AlignVCenter;id:miniMore;objectName:"miniMore";symbol:"more";description:qsTr("组件菜单");onPressedChanged:if(pressed)widget.miniKeyboardInteraction=false;onClicked:widget.openVerificationMenu()}
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
