pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: widget
    required property WidgetViewModel controller
    required property TaskWorkspace workspace
    signal newTaskRequested()
    signal openMainRequested()
    signal openTaskRequested(string taskId)
    signal settingsRequested()
    objectName: "widgetWindow"
    width: 340;height: 360
    readonly property int ballDiameter: Math.max(36,Theme.iconSize+12)
    readonly property int miniBarHeight: Math.max(48,Theme.controlHeight+2*Theme.panelPadding)
    onMiniBarHeightChanged: Qt.callLater(syncMiniLayout)
    onBallDiameterChanged: Qt.callLater(syncMiniLayout)
    Component.onCompleted: {syncMiniLayout();controller.enableVisualTransitions();Qt.callLater(setMiniAnimation)}
    minimumWidth: controller.mini && ((controller.enabledForms & 4)!==0) ? ballDiameter : 260
    minimumHeight: controller.mini && ((controller.enabledForms & 4)!==0) ? ballDiameter : controller.mini ? Math.max(48,Theme.controlHeight+2*Theme.panelPadding) : Math.max(190,header.implicitHeight+footer.implicitHeight+(widgetError.visible ? widgetError.implicitHeight : 0)+2*Theme.panelPadding+4*Theme.fieldGap+48+Theme.listInset*2+ (width<300 ? footer.implicitHeight : 0))
    x: 80;y: 80;visible: false;color: "transparent"
    title: qsTr("Daymark 今日任务")
    property string taskError: ""
    readonly property string errorMessage: taskError || controller.errorText
    readonly property string primaryActionName: (controller.enabledForms & 1)!==0 ? qsTr("打开任务小窗") : controller.miniCollapsed && (controller.enabledForms & 2)!==0 ? qsTr("显示 Mini 条") : qsTr("打开主窗口")
    readonly property bool reduceMotion: workspace.preferences.followSystemAccessibility ? workspace.systemAccessibility.reduceMotion : workspace.preferences.reduceMotion
    readonly property bool interacting: headerDrag.active || miniDrag.active || ballDrag.active
    readonly property bool miniMode: controller.mini
    property bool miniKeyboardInteraction: false
    property bool pendingMiniKeyboardFocus: false
    property real miniReveal: controller.targetBall ? 0 : 1
    readonly property bool miniTransitionRunning: miniAnimation.running
    readonly property bool panelTransitionRunning: panelAnimation.running
    property real visualWidth: width
    property real visualHeight: height
    property real panelReveal: controller.mini ? 0 : 1
    property real panelContentOpacity: controller.mini ? 0 : 1
    property int visualTransition: -1
    property int collapsingTransition: -1
    readonly property Control miniFocusControl: activeFocusItem as Control
    readonly property bool miniKeyboardFocus: active && (miniKeyboardInteraction || (!!miniFocusControl && [Qt.TabFocusReason,Qt.BacktabFocusReason,Qt.ShortcutFocusReason].indexOf(miniFocusControl.focusReason)>=0))
    readonly property bool interactionPressed: ballPress.active || miniPress.active || headerPress.active || miniSummary.down || miniMore.pressed || workspace.busy || errorMessage.length>0
    onActiveChanged: if(!active && !widgetMenu.visible)miniKeyboardInteraction=false
    onMiniKeyboardFocusChanged: reportInteraction()
    onInteractionPressedChanged: reportInteraction()
    onInteractingChanged: reportInteraction()
    function reportInteraction() {controller.setInteraction(miniHover.hovered,interactionPressed || interacting,widgetMenu.visible,miniKeyboardFocus)}
    function syncMiniLayout() {controller.setMiniLayout(miniBarHeight,ballDiameter)}
    function syncSurfaceSize() {
        if(controller.geometryChanging || controller.panelMotion)return
        visualWidth=controller.mini ? ballDiameter+(controller.miniWidth-ballDiameter)*miniReveal : width
        visualHeight=controller.mini ? ballDiameter+(miniBarHeight-ballDiameter)*miniReveal : height
        panelReveal=controller.mini ? 0 : 1
        panelContentOpacity=panelReveal
        syncInputRegion()
    }
    function setMiniAnimation() {
        if(controller.geometryChanging || visualTransition===controller.transitionId)return
        const previous=visualTransition
        visualTransition=controller.transitionId
        const target=controller.targetBall ? 0 : 1
        const inFlight=miniAnimation.running
        miniAnimation.stop()
        panelAnimation.stop()
        collapsingTransition=visualTransition
        if(controller.panelMotion && !reduceMotion && visible) {
            miniReveal=target
            const opening=!controller.targetBall
            const duration=Math.max(1,Math.round((opening ? 180 : 150)*Math.abs(panelReveal-target)))
            panelWidthAnimation.to=opening ? width : ballDiameter
            panelHeightAnimation.to=opening ? height : ballDiameter
            panelProgressAnimation.to=target
            panelWidthAnimation.duration=duration;panelHeightAnimation.duration=duration;panelProgressAnimation.duration=duration
            panelFadeDelay.duration=opening && panelContentOpacity<0.001 ? Math.min(36,duration/5) : 0
            panelFadeAnimation.to=target
            panelFadeAnimation.duration=opening ? Math.max(1,duration-panelFadeDelay.duration) : Math.min(60,duration)
            panelAnimation.start()
            return
        }
        if(!controller.mini || controller.miniCollapsed || reduceMotion || !visible){miniReveal=target;controller.finishTransition(visualTransition);syncSurfaceSize();return}
        if(!inFlight && previous<0)miniReveal=controller.revealFrom
        else if(!inFlight && !controller.targetBall && controller.revealFrom===0)miniReveal=0
        if(Math.abs(miniReveal-target)<0.001){miniReveal=target;controller.finishTransition(visualTransition);return}
        miniAnimation.to=target
        miniAnimation.duration=Math.max(1,Math.round(160*Math.abs(miniReveal-target)))
        miniAnimation.start()
    }
    function syncInputRegion() {
        controller.setRevealRegion(Qt.rect(Math.round(surface.x),Math.round(surface.y),Math.ceil(surface.width),Math.ceil(surface.height)),surface.radius)
    }
    onMiniRevealChanged: syncSurfaceSize()
    onVisualWidthChanged: syncInputRegion()
    onVisualHeightChanged: syncInputRegion()
    onWidthChanged: Qt.callLater(syncSurfaceSize)
    onHeightChanged: Qt.callLater(syncSurfaceSize)
    NumberAnimation {id:miniAnimation;target:widget;property:"miniReveal";easing.type:Easing.OutCubic;onFinished:{widget.controller.finishTransition(widget.collapsingTransition);widget.syncInputRegion()}}
    ParallelAnimation {
        id:panelAnimation
        NumberAnimation {id:panelWidthAnimation;target:widget;property:"visualWidth";easing.type:Easing.OutCubic}
        NumberAnimation {id:panelHeightAnimation;target:widget;property:"visualHeight";easing.type:Easing.OutCubic}
        NumberAnimation {id:panelProgressAnimation;target:widget;property:"panelReveal";easing.type:Easing.OutCubic}
        SequentialAnimation {
            PauseAnimation {id:panelFadeDelay}
            NumberAnimation {id:panelFadeAnimation;target:widget;property:"panelContentOpacity";easing.type:Easing.OutCubic}
        }
        onFinished:{widget.controller.finishTransition(widget.collapsingTransition);widget.syncSurfaceSize()}
    }
    Connections {
        target:widget.controller
        function onStateChanged(){widget.setMiniAnimation();widget.syncSurfaceSize();Qt.callLater(widget.syncInputRegion)}
    }
    Connections {target:widget.workspace;function onStateChanged(){if(!widget.workspace.errorText.length)widget.taskError=""}}
    onReduceMotionChanged: if(reduceMotion && (miniAnimation.running || panelAnimation.running)){miniAnimation.stop();panelAnimation.stop();miniReveal=controller.targetBall ? 0 : 1;controller.finishTransition(collapsingTransition);syncSurfaceSize()}
    onVisibleChanged:{reportInteraction();Qt.callLater(setMiniAnimation)}
    onMiniModeChanged: {
        if(!(controller.effectiveForm===0 && controller.selectedForm!==0))miniKeyboardInteraction=false
        if(active)Qt.callLater(function(){if(widget.controller.miniCollapsed)miniBall.forceActiveFocus();else if(widget.controller.mini)miniSummary.forceActiveFocus();else if(todayList.count>0)todayList.forceActiveFocus();else addTask.forceActiveFocus()})
    }
    function closeOrCompact() {if(widgetMenu.visible)widgetMenu.close();else if(!controller.mini)controller.returnToCompact()}
    Shortcut {sequence:"Escape";enabled:widget.active;onActivated:widget.closeOrCompact()}
    Shortcut {sequence:"Shift+F10";enabled:widget.active;onActivated:widget.openVerificationMenu()}
    function verifyMenu() {
        if(!widgetMenu.visible)return "closed"
        if(widgetMenu.verifyGrouping()!=="PASS")return widgetMenu.verifyGrouping()
        const source=controller.miniCollapsed ? miniBall : controller.mini ? miniMore : widgetMore
        if(widgetMenu.anchorItem!==source || widgetMenu.parent!==widget.contentItem)return "stale menu anchor"
        let item=widgetMenu.contentItem
        while(item){if(item.opacity<0.999 || !item.enabled)return "menu inherits fading or disabled ancestor";item=item.parent}
        const background=widgetMenu.background as Rectangle
        if(widgetMenu.opacity!==1 || !background || background.color.a!==1)return "translucent menu surface"
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
    function openVerificationMenu() {
        miniAnimation.stop();panelAnimation.stop()
        miniReveal=controller.targetBall ? 0 : 1
        controller.finishTransition(controller.transitionId)
        syncSurfaceSize()
        widgetMenu.popupAt(controller.miniCollapsed ? miniBall : controller.mini ? miniMore : widgetMore)
    }
    function closeVerificationMenu() {widgetMenu.close()}
    DS.Menu {
        id:widgetMenu;objectName:"widgetMenu"
        parent:widget.contentItem
        popupType:Popup.Window
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
            if(controller.miniCollapsed)return width===height && width>=36 && miniBall.visible ? "PASS" : "mini ball geometry"
            if(width!==controller.miniWidth || height!==miniBarHeight)return "mini target size drift: "+width+"x"+height+" expected "+controller.miniWidth+"x"+miniBarHeight
            if(miniSummary.width<60 || miniSummary.height<Theme.controlHeight || miniTitle.width<20)return "mini content clipped"
        } else if(todayList.width<100 || todayList.height<24 || footer.y+footer.height>surface.height+1 || header.width<header.implicitWidth-1){
            const details="width="+Math.round(header.width)+" implicit="+Math.round(header.implicitWidth)+" list="+Math.round(todayList.width)+"x"+Math.round(todayList.height)+" footerBottom="+Math.round(footer.y+footer.height)+" surfaceH="+Math.round(surface.height)
            return "floating layout clipped ("+details+")"
        }
        return "PASS"
    }
    Shortcut {sequences: [StandardKey.New];enabled: widget.active && !widget.controller.mini;onActivated: widget.newTaskRequested()}
    Control {
        anchors.fill: parent
        font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize
        palette.window: Theme.content;palette.windowText: Theme.text;palette.text: Theme.text
        palette.button: Theme.sidebar;palette.buttonText: Theme.text;palette.highlight: Theme.accent
        Keys.onPressed:function(event){widget.miniKeyboardInteraction=true;widget.reportInteraction();event.accepted=false}
        contentItem: Item {
          Rectangle {
            id: surface
            x:widget.controller.growLeft ? widget.width-width : 0
            y:widget.controller.growUp ? widget.height-height : 0
            onXChanged:widget.syncInputRegion()
            onYChanged:widget.syncInputRegion()
            width: widget.visualWidth
            height: widget.visualHeight
            clip: widget.controller.mini || widget.controller.panelMotion
            objectName: "widgetBackground"
            radius: widget.controller.panelMotion ? Math.min(height/2,22)+(Theme.panelRadius-Math.min(height/2,22))*widget.panelReveal : widget.controller.mini ? Math.min(height/2,22) : Theme.panelRadius
            onRadiusChanged:widget.syncInputRegion()
            color: Qt.rgba(Theme.content.r,Theme.content.g,Theme.content.b,widget.workspace.preferences.solidWidget || Theme.highContrast ? 1 : widget.controller.backgroundOpacity)
            border.color: Theme.separator
            HoverHandler {id:miniHover;onHoveredChanged:widget.reportInteraction()}
            TapHandler {acceptedButtons:Qt.RightButton;onTapped:widget.openVerificationMenu()}
            ToolButton {
                id:miniBall;objectName:"widgetMiniBall"
                visible:widget.controller.panelMotion ? widget.panelReveal<0.999 : widget.controller.mini && widget.miniReveal<0.999
                width:widget.ballDiameter;height:widget.ballDiameter
                x:widget.controller.growLeft ? surface.width-width : 0
                y:widget.controller.growUp ? surface.height-height : 0
                padding:8
                opacity:Math.max(0,1-(widget.controller.panelMotion ? widget.panelReveal : widget.miniReveal)*3)
                Accessible.name:qsTr("今天 %1 项").arg(widget.workspace.todayTasks.count)+"，"+widget.primaryActionName
                onClicked:if(!ballDrag.active)widget.controller.activatePrimary()
                Keys.onReturnPressed:widget.controller.activatePrimary()
                onActiveFocusChanged:if(activeFocus && [Qt.TabFocusReason,Qt.BacktabFocusReason,Qt.ShortcutFocusReason].indexOf(focusReason)>=0)widget.miniKeyboardInteraction=true
                PointHandler {id:ballPress;acceptedButtons:Qt.LeftButton}
                DragHandler {id:ballDrag;target:null;enabled:!widget.controller.locked;onActiveChanged:{if(active)widget.controller.startMove();else widget.controller.finishMove()} cursorShape:Qt.OpenHandCursor}
                contentItem:AppIcon {name:widget.controller.errorText.length ? "info" : "today";ink:widget.controller.errorText.length ? Theme.danger : Theme.accent}
                background:Rectangle {radius:width/2;color:miniBall.down ? Theme.pressed : miniBall.hovered ? Theme.hover : "transparent";border.width:miniBall.visualFocus ? 2 : 0;border.color:Theme.accent}
            }
            Item {
                id:panelContent;objectName:"widgetPanelContent"
                visible: !widget.controller.mini
                width:widget.width;height:widget.height
                x:widget.controller.growLeft ? surface.width-width : 0
                y:widget.controller.growUp ? surface.height-height : 0
                opacity:widget.panelContentOpacity
                enabled:!widget.controller.targetBall
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
                            required property int index
                            width: todayList.width-Theme.scrollGutter
                            workspace: widget.workspace
                            reduceMotion: widget.reduceMotion
                            highlighted: todayList.activeFocus && todayList.currentIndex===index
                            onOpenRequested: function(id){widget.openTaskRequested(id)}
                            onActionStarted:widget.taskError=""
                            onErrorReported:function(message){widget.taskError=message}
                        }
                    }
                }
                Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator}
                RowLayout {
                    id: footer
                    Layout.fillWidth: true
                    spacing: 4
                    ActionButton {id: addTask;objectName: "widgetAdd";symbol: "plus";text: qsTr("添加任务");enabled: widget.workspace.ready && !widget.workspace.busy;onClicked: widget.newTaskRequested()}
                    Item {Layout.fillWidth: true}
                    WidgetAction {objectName: "widgetOpenMain";symbol: "window";description: qsTr("打开主窗口");onClicked: widget.openMainRequested()}
                }
            }
            }
            RowLayout {
                id:miniRow
                layoutDirection:widget.controller.growLeft ? Qt.RightToLeft : Qt.LeftToRight
                visible: widget.controller.mini && widget.miniReveal>0.001
                enabled:widget.miniReveal>0.99
                x:Theme.panelPadding;y:Theme.panelPadding
                width:widget.controller.miniWidth-2*Theme.panelPadding;height:widget.miniBarHeight-2*Theme.panelPadding
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
            MouseArea {
                anchors.right: parent.right;anchors.bottom: parent.bottom
                width: 12;height: 12
                visible: !widget.controller.mini && !widget.controller.panelMotion && !widget.controller.locked
                cursorShape: Qt.SizeFDiagCursor
                onPressed: widget.controller.startResize()
                Accessible.name: qsTr("调整组件大小")
            }
          }
        }
    }
}
