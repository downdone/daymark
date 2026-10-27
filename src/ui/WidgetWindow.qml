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
    minimumWidth: 260
    minimumHeight: controller.mini ? Math.max(48,Theme.controlHeight+12) : Math.max(190,header.implicitHeight+footer.implicitHeight+(widgetError.visible ? widgetError.implicitHeight : 0)+2*Theme.panelPadding+4*Theme.fieldGap+48)
    x: 80;y: 80;visible: false;color: "transparent"
    title: qsTr("Daymark 今日任务")
    readonly property string errorMessage: workspace.errorText || controller.errorText
    readonly property bool reduceMotion: workspace.preferences.followSystemAccessibility ? workspace.systemAccessibility.reduceMotion : workspace.preferences.reduceMotion
    readonly property bool interacting: headerDrag.active || miniDrag.active
    readonly property bool miniMode: controller.mini
    onMiniModeChanged: if(active)Qt.callLater(function(){if(widget.controller.mini)miniSummary.forceActiveFocus();else if(todayList.count>0)todayList.forceActiveFocus();else addTask.forceActiveFocus()})
    function verifyMenu() {
        if(!widgetMenu.visible)return "closed"
        if(widgetMenu.verifyGrouping()!=="PASS")return widgetMenu.verifyGrouping()
        const source=controller.mini ? miniMore : widgetMore
        if(widgetMenu.anchorItem!==source || widgetMenu.parent!==source)return "stale menu anchor"
        const background=widgetMenu.background as Rectangle
        if(widgetMenu.opacity!==1 || !background || background.color.a!==1)return "translucent menu surface"
        if(widgetMenu.menuWindow!==widget && widgetMenu.menuWindow.color.a!==1)return "translucent menu window"
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
    function openVerificationMenu() {widgetMenu.popupAt(controller.mini ? miniMore : widgetMore)}
    function closeVerificationMenu() {widgetMenu.close()}
    function verifyLayout() {
        if(controller.mini) {
            if(miniSummary.width<80 || miniSummary.height<Theme.controlHeight || miniTitle.width<20)return "mini content clipped"
        } else if(todayList.width<100 || todayList.height<24 || footer.y+footer.height>surface.height+1 || header.width<header.implicitWidth-1)return "floating layout clipped"
        return "PASS"
    }
    Shortcut {sequences: [StandardKey.New];enabled: widget.active && !widget.controller.mini;onActivated: widget.newTaskRequested()}
    Control {
        anchors.fill: parent
        font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize
        palette.window: Theme.content;palette.windowText: Theme.text;palette.text: Theme.text
        palette.button: Theme.sidebar;palette.buttonText: Theme.text;palette.highlight: Theme.accent
        contentItem: Rectangle {
            id: surface
            objectName: "widgetBackground"
            radius: widget.controller.mini ? Math.min(height/2,22) : Theme.panelRadius
            color: Qt.rgba(Theme.content.r,Theme.content.g,Theme.content.b,widget.workspace.preferences.solidWidget || Theme.highContrast ? 1 : widget.controller.backgroundOpacity)
            border.color: Theme.separator
            ColumnLayout {
                visible: !widget.controller.mini
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
                                Label {text: qsTr("今天");font.pixelSize: Theme.headingSize;font.weight: Font.DemiBold;color: Theme.text}
                                Label {Layout.fillWidth: true;text: widget.workspace.todayTasks.count;font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight;Accessible.name: qsTr("%1 项待办").arg(text)}
                            }
                            Label {
                                width: parent.width
                                text: Qt.formatDate(new Date(widget.workspace.dateOffset(0)+"T12:00:00"),qsTr("M月d日"))
                                font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight
                            }
                        }
                        DragHandler {id: headerDrag;target: null;enabled: !widget.controller.locked;onActiveChanged: if(active)widget.controller.startMove();cursorShape: Qt.OpenHandCursor}
                    }
                    WidgetAction {objectName: "widgetPin";symbol: "pin";checked: widget.controller.alwaysOnTop;Accessible.checkable: true;Accessible.checked: checked;description: checked ? qsTr("取消置顶") : qsTr("置顶组件");onClicked: widget.controller.setAlwaysOnTop(!widget.controller.alwaysOnTop)}
                    WidgetAction {objectName: "widgetCollapse";symbol: "up";description: qsTr("收起为迷你条");onClicked: widget.controller.setMini(true)}
                    WidgetAction {
                        id:widgetMore
                        objectName: "widgetMore";description: qsTr("组件菜单");hintText: qsTr("调整组件外观与行为")
                        onClicked: widget.openVerificationMenu()
                        DS.Menu {
                            id: widgetMenu
                            objectName: "widgetMenu"
                            popupType: Popup.Window
                            MenuItem {text: qsTr("打开主窗口");onTriggered: widget.openMainRequested()}
                            DS.MenuSeparator {}
                            MenuItem {objectName:"widgetDesktopPinnedMenu";text:qsTr("固定在桌面");checkable:true;checked:widget.controller.desktopPinned;onTriggered:widget.controller.setDesktopPinned(!widget.controller.desktopPinned)}
                            MenuItem {text:qsTr("保持置顶");checkable:true;checked:widget.controller.alwaysOnTop;onTriggered:widget.controller.setAlwaysOnTop(!widget.controller.alwaysOnTop)}
                            DS.MenuSeparator {}
                            MenuItem {text: qsTr("锁定位置与大小");checkable: true;checked: widget.controller.locked;onTriggered: widget.controller.setLocked(checked)}
                            MenuItem {text: qsTr("鼠标穿透（托盘/主窗恢复）");enabled: widget.controller.trayAvailable;onTriggered: widget.controller.setClickThrough(true)}
                            DS.MenuSeparator {}
                            MenuItem {text: qsTr("外观与组件设置…");onTriggered: widget.settingsRequested()}
                            DS.MenuSeparator {}
                            MenuItem {text: qsTr("隐藏组件");onTriggered: widget.controller.setVisible(false)}
                        }
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
                            width: todayList.width
                            workspace: widget.workspace
                            reduceMotion: widget.reduceMotion
                            highlighted: todayList.activeFocus && todayList.currentIndex===index
                            onOpenRequested: function(id){widget.openTaskRequested(id)}
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
            RowLayout {
                visible: widget.controller.mini
                anchors.fill: parent;anchors.margins: 6
                spacing: 2
                ToolButton {
                    id: miniSummary
                    objectName: "widgetMiniSummary"
                    Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumWidth: 0
                    padding: 4
                    Accessible.name: (widget.workspace.ready ? qsTr("今天 %1 项，%2").arg(widget.workspace.todayTasks.count).arg(widget.workspace.firstTodayTitle || qsTr("暂无待办")) : qsTr("正在加载"))+(widget.errorMessage.length ? ", "+widget.errorMessage : "")+qsTr("。展开任务组件")
                    onClicked: widget.controller.setMini(false)
                    Keys.onReturnPressed: widget.controller.setMini(false)
                    DragHandler {id: miniDrag;target: null;enabled: !widget.controller.locked;onActiveChanged: if(active)widget.controller.startMove();cursorShape: Qt.OpenHandCursor}
                    contentItem: RowLayout {
                        spacing: 6
                        AppIcon {name: widget.errorMessage.length ? "info" : "today";ink: widget.errorMessage.length ? Theme.danger : Theme.accent;Layout.preferredWidth: Theme.iconSize;Layout.preferredHeight: Theme.iconSize}
                        Label {text: widget.workspace.todayTasks.count;font.pixelSize: Theme.bodySize;font.weight: Font.DemiBold;color: Theme.text;visible: widget.workspace.ready}
                        Label {id: miniTitle;objectName: "widgetMiniTitle";Layout.fillWidth: true;Layout.minimumWidth: 0;text: widget.workspace.ready ? (widget.workspace.firstTodayTitle || qsTr("今天暂无待办")) : qsTr("正在加载…");textFormat: Text.PlainText;font.pixelSize: Theme.bodySize;color: Theme.text;elide: Text.ElideRight}
                    }
                    ToolTip.visible: !Theme.hintsBlocked && hovered && !down && !widget.interacting && (miniTitle.truncated || widget.errorMessage.length>0)
                    ToolTip.delay: Theme.hintDelay
                    ToolTip.timeout: Theme.hintTimeout
                    ToolTip.text: widget.errorMessage || widget.workspace.firstTodayTitle
                }
                WidgetAction {objectName: "widgetExpand";symbol: "down";description: qsTr("展开任务组件");onClicked: widget.controller.setMini(false)}
                WidgetAction {id:miniMore;objectName:"miniMore";symbol:"more";description:qsTr("组件菜单");onClicked:widget.openVerificationMenu()}
            }
            MouseArea {
                anchors.right: parent.right;anchors.bottom: parent.bottom
                width: 12;height: 12
                visible: !widget.controller.mini && !widget.controller.locked
                cursorShape: Qt.SizeFDiagCursor
                onPressed: widget.controller.startResize()
                Accessible.name: qsTr("调整组件大小")
            }
        }
    }
}
