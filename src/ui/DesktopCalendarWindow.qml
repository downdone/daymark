pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ApplicationWindow {
    id: calendar
    objectName: "desktopCalendarWindow"
    required property TaskWorkspace workspace
    signal openTask(string taskId)
    signal taskAction(string taskId,string action)
    signal completeTask(string taskId,real version,string action)
    signal timeMoveRequested(string taskId,real version,string date,int minute,string edge)
    signal dateMoveRequested(string taskId,real version,string from,string date,string edge)
    width: 760;height: 720
    minimumWidth: 420;minimumHeight: 480
    visible: workspace.ready && workspace.preferences.desktopCalendarVisible
    transientParent: null
    flags: Qt.Window | Qt.FramelessWindowHint
    Component.onCompleted:workspace.attachDesktopCalendarWindow(calendar)
    title: qsTr("Daymark · 桌面日历")
    color: "transparent"
    font.family: Theme.fontFamily;font.pixelSize: Theme.bodySize
    palette.windowText: Theme.text;palette.text: Theme.text;palette.buttonText: Theme.text
    background: Rectangle {
        objectName: "desktopCalendarBackground"
        color: Qt.rgba(Theme.content.r,Theme.content.g,Theme.content.b,Theme.highContrast || calendar.workspace.preferences.solidWidget ? 1 : calendar.workspace.preferences.desktopCalendarOpacity/100)
        radius: Theme.panelRadius;border.color: Theme.separator
    }
    onClosing: function(event) {
        event.accepted=false
        if(!page.protectClose(function(){calendar.workspace.setPreference("desktopCalendarVisible",false)}))calendar.workspace.setPreference("desktopCalendarVisible",false)
    }
    readonly property bool dragging: page.timelineDragging
    function calendarCommand(action){page.command(action)}
    function protectClose(action) {
        if(!page.protectClose(action))return false
        workspace.setPreference("desktopCalendarVisible",true);requestActivate();return true
    }
    function resetPosition() {x=Screen.virtualX+Math.max(0,(Screen.width-width)/2);y=Screen.virtualY+Math.max(0,(Screen.height-height)/2)}
    function verifyIndependence() {
        const oldView=workspace.view
        const oldTitle=workspace.calendarTitle
        const original=page.anchorDate
        const originalSelection=page.selectedDate
        page.step(1)
        const changed=page.anchorDate!==original && page.days.length===42
        page.anchorDate=original;page.selectedDate=originalSelection
        return changed && workspace.view===oldView && workspace.calendarTitle===oldTitle
    }
    ColumnLayout {
        anchors.fill: parent;anchors.margins: Theme.panelPadding
        spacing: Theme.fieldGap
        RowLayout {
            Layout.fillWidth: true
            Label {text: qsTr("日历");font.weight: Font.DemiBold;Layout.fillWidth: true
                DragHandler {target: null;onActiveChanged: if(active)calendar.startSystemMove()}
            }
            WidgetAction {symbol: "pin";description: checked ? qsTr("取消日历置顶") : qsTr("日历置顶");checked: calendar.workspace.preferences.desktopCalendarTopmost;Accessible.checkable: true;Accessible.checked: checked;onClicked: calendar.workspace.setPreference("desktopCalendarTopmost",!checked)}
            WidgetAction {symbol: "settings";description: qsTr("日历透明度与位置");onClicked: options.open()
                Menu {
                        popupType: Popup.Window
                    id: options
                    MenuItem {text: qsTr("背景透明度 %1%").arg(calendar.workspace.preferences.desktopCalendarOpacity);enabled: false}
                    MenuItem {text: qsTr("100% · 实色");onTriggered: calendar.workspace.setPreference("desktopCalendarOpacity",100)}
                    MenuItem {text: "90%";onTriggered: calendar.workspace.setPreference("desktopCalendarOpacity",90)}
                    MenuItem {text: "70%";onTriggered: calendar.workspace.setPreference("desktopCalendarOpacity",70)}
                    MenuItem {text: "50%";onTriggered: calendar.workspace.setPreference("desktopCalendarOpacity",50)}
                    MenuSeparator {}
                    MenuItem {objectName:"calendarDesktopPinned";text:qsTr("固定在桌面");checkable:true;checked:calendar.workspace.preferences.desktopCalendarPinned;onTriggered:calendar.workspace.setPreference("desktopCalendarPinned",!calendar.workspace.preferences.desktopCalendarPinned)}
                    MenuItem {text:qsTr("保持置顶");checkable:true;checked:calendar.workspace.preferences.desktopCalendarTopmost;onTriggered:calendar.workspace.setPreference("desktopCalendarTopmost",!calendar.workspace.preferences.desktopCalendarTopmost)}
                    MenuSeparator {}
                    MenuItem {text: qsTr("重置位置");onTriggered: calendar.resetPosition()}
                }
            }
            WidgetAction {symbol: "close";description: qsTr("隐藏桌面日历");onClicked: calendar.close()}
        }
        CalendarPanel {
            id: page
            objectName: "desktopCalendarPanel"
            Layout.fillWidth: true;Layout.fillHeight: true
            workspace: calendar.workspace
            independent: true
            onOpenTask: function(taskId){calendar.openTask(taskId)}
            onTaskAction:function(taskId,action){calendar.taskAction(taskId,action)}
            onTimeMoveRequested:function(id,version,date,minute,edge){calendar.timeMoveRequested(id,version,date,minute,edge)}
            onDateMoveRequested:function(id,version,from,date,edge){calendar.dateMoveRequested(id,version,from,date,edge)}
            onCompleteTask: function(taskId,version,action){calendar.completeTask(taskId,version,action)}
        }
        Label {visible: calendar.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: calendar.workspace.errorText;color: Theme.danger}
    }
    MouseArea {anchors.right: parent.right;anchors.bottom: parent.bottom;width: 16;height: 16;cursorShape: Qt.SizeFDiagCursor;onPressed: calendar.startSystemResize(Qt.RightEdge|Qt.BottomEdge)}
}
