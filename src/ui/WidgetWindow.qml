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
    width: 340
    height: 360
    minimumWidth: 260
    minimumHeight: widget.controller.mini ? 48 : 190
    x: 80
    y: 80
    visible: false
    color: "transparent"
    title: qsTr("Daymark 今日任务")
    function verifyMenu() {
        if(!widgetMenu.visible)return "closed"
        for(let i=0;i<widgetMenu.count;i++) {
            const item=widgetMenu.itemAt(i) as MenuItem
            if(item && item.text && item.width<item.implicitWidth-1)return "clipped item "+i+": "+item.width+"/"+item.implicitWidth
        }
        return "PASS"
    }
    function openVerificationMenu() {widgetMenu.open()}
    function closeVerificationMenu() {widgetMenu.close()}

    Control {
        anchors.fill: parent
        font.family: Theme.fontFamily
        font.pixelSize: Theme.bodySize
        palette.window: Theme.content
        palette.windowText: Theme.text
        palette.text: Theme.text
        palette.button: Theme.sidebar
        palette.buttonText: Theme.text
        palette.highlight: Theme.accent
        contentItem: Rectangle {
        radius: Theme.panelRadius
        color: Qt.rgba(Theme.content.r, Theme.content.g, Theme.content.b, widget.workspace.preferences.solidWidget || Theme.highContrast ? 1 : widget.controller.backgroundOpacity)
        border.color: Theme.separator

        ColumnLayout {
            visible: !widget.controller.mini
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8
            RowLayout {
                objectName: "widgetHeader"
                Layout.fillWidth: true
                spacing: 2
                Item {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: Math.max(32,headerText.implicitHeight)
                    Column {
                        id: headerText
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        Label {width: parent.width;text: qsTr("今天");font.bold: true;color: Theme.text;elide: Text.ElideRight}
                        Label {width: parent.width;text: qsTr("待办 %1 · 完成 %2").arg(widget.workspace.todayTasks.count).arg(widget.workspace.completedToday);font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight}
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.SizeAllCursor
                        onPressed: widget.controller.startMove()
                    }
                }
                WidgetAction {objectName: "widgetAdd";symbol: "plus";description: qsTr("添加今日任务");onClicked: widget.newTaskRequested()}
                WidgetAction {
                    objectName: "widgetPin"
                    symbol: "pin"
                    checked: widget.controller.alwaysOnTop
                    Accessible.checkable: true
                    Accessible.checked: checked
                    description: checked ? qsTr("取消置顶") : qsTr("置顶组件")
                    onClicked: widget.controller.setAlwaysOnTop(!widget.controller.alwaysOnTop)
                }
                WidgetAction {objectName: "widgetCollapse";symbol: "collapse";description: qsTr("收起为迷你条");onClicked: widget.controller.setMini(true)}
                WidgetAction {
                    objectName: "widgetMore"
                    description: qsTr("组件菜单")
                    onClicked: widgetMenu.open()
                    Menu {
                        popupType: Popup.Window
                        id: widgetMenu
                        objectName: "widgetMenu"
                        x: parent.width-width
                        y: parent.height
                        MenuItem {text: qsTr("打开主窗口");onTriggered: widget.openMainRequested()}
                        MenuSeparator {}
                        MenuItem {text: qsTr("保持置顶");checkable: true;checked: widget.controller.alwaysOnTop;onTriggered: widget.controller.setAlwaysOnTop(checked)}
                        MenuItem {text: qsTr("锁定拖动");checkable: true;checked: widget.controller.locked;onTriggered: widget.controller.setLocked(checked)}
                        MenuItem {text: qsTr("鼠标穿透（托盘/主窗恢复）");enabled: widget.controller.trayAvailable;onTriggered: widget.controller.setClickThrough(true)}
                        MenuItem {text: qsTr("外观与组件设置…");onTriggered: widget.settingsRequested()}
                        MenuSeparator {}
                        MenuItem {text: qsTr("隐藏组件");onTriggered: widget.controller.setVisible(false)}
                    }
                }
            }
            Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator}
            Label {
                Layout.fillWidth: true
                visible: widget.workspace.todayTasks.count === 0
                text: widget.workspace.ready ? qsTr("今天暂无任务") : qsTr("正在加载本地任务…")
                color: Theme.secondary
                lineHeight: 1.4
            }
            Button {text:qsTr("添加任务");visible:widget.workspace.ready && todayList.count===0;onClicked:widget.newTaskRequested()}
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: widget.controller.errorText.length > 0 || widget.workspace.errorText.length > 0
                text: widget.workspace.errorText || widget.controller.errorText
                font.pixelSize: Theme.captionSize
                color: Theme.danger
            }
            ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
                DS.ScrollPhysics {target: todayList}
                id: todayList
                objectName: "widgetTasks"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: widget.workspace.todayTasks
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property string taskId
                    required property string title
                    required property real rowVersion
                    id: todayRow
                    width: todayList.width
                    IconToolButton {
                        Layout.preferredWidth: Math.max(32,Theme.iconSize+12)
                        Layout.preferredHeight: Math.max(32,Theme.iconSize+12)
                        contentItem: Item {
                            implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize
                            AppIcon {anchors.centerIn: parent;width: Theme.iconSize;height: width;name: "circle"}
                        }
                        Accessible.name: qsTr("完成任务")
                        enabled: widget.workspace.ready && !widget.workspace.busy
                        onClicked: widget.workspace.changeState(todayRow.taskId, todayRow.rowVersion, "complete")
                    }
                    ToolButton {Layout.fillWidth:true;text:todayRow.title;Accessible.name:qsTr("任务详情：%1").arg(todayRow.title);onClicked:widget.openTaskRequested(todayRow.taskId);contentItem:Label{text:todayRow.title;elide:Text.ElideRight;color:Theme.text}}
                }
            }
        }

        RowLayout {
            visible: widget.controller.mini
            anchors.fill: parent
            anchors.margins: 6
            spacing: 6
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Label {
                    anchors.centerIn: parent
                    width: parent.width
                    elide: Text.ElideRight
                    text: qsTr("今日 %1 项").arg(widget.workspace.todayTasks.count)+(widget.workspace.todayTasks.count>0 ? " · "+widget.workspace.firstTodayTitle : "")
                    font.pixelSize: Theme.captionSize
                    color: Theme.text
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    onPressed: widget.controller.startMove()
                }
            }
            WidgetAction {objectName: "widgetExpand";symbol: "expand";description: qsTr("展开任务组件");onClicked: widget.controller.setMini(false)}
        }
        MouseArea {
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 16
            height: 16
            visible: !widget.controller.mini && !widget.controller.locked
            cursorShape: Qt.SizeFDiagCursor
            onPressed: widget.controller.startResize()
            Accessible.name: qsTr("调整组件大小")
        }
        }
    }
}
