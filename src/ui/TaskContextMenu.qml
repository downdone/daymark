pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import DaymarkStyle as DS
DS.Menu {
    id: menu
    required property TaskWorkspace workspace
    Connections {target:menu.workspace;function onStateChanged(){if(menu.visible && menu.target.taskId){const current=menu.workspace.task(menu.target.taskId);if(!current.taskId || current.rowVersion!==menu.target.rowVersion)menu.close()}}}
    property var target: ({taskId:"",rowVersion:0,isDone:false,pinned:false,important:false,hasReminder:false,inToday:false,evening:false,canActivate:false,canReorder:false,canMoveUp:false,canMoveDown:false})
    objectName: "taskContext-"+(target.taskId || "")
    signal commandRequested(string taskId,real version,string action,var value)
    function capture(row){target={taskId:row.taskId,rowVersion:row.rowVersion,isDone:row.isDone,pinned:row.pinned,important:row.important,hasReminder:row.hasReminder,inToday:row.inToday,evening:row.evening,canActivate:row.canActivate,canReorder:row.canReorder,canMoveUp:row.canMoveUp,canMoveDown:row.canMoveDown}}
    function dispatch(action,value){commandRequested(target.taskId,target.rowVersion,action,value)}
        MenuItem {text: qsTr("任务详情");onTriggered: menu.dispatch("edit",null)}
        MenuItem {objectName: "taskMenuState-"+menu.target.taskId;text: menu.target.isDone ? qsTr("重新打开") : qsTr("完成任务");onTriggered: menu.dispatch("complete",null)}
        DS.MenuSeparator {}
        MenuItem {objectName: "taskMenuPin-"+menu.target.taskId;text: menu.target.pinned ? qsTr("取消置顶") : qsTr("置顶");onTriggered: menu.dispatch("pin",!menu.target.pinned)}
        DS.MenuItem {objectName: "taskMenuImportant-"+menu.target.taskId;text: menu.target.important ? qsTr("取消特别关注") : qsTr("特别关注");shortcutText:Qt.platform.os==="osx" ? "⇧⌘F" : "Ctrl+Shift+F";onTriggered: menu.dispatch("important",!menu.target.important)}
        MenuItem {objectName: "taskMenuCancelReminder-"+menu.target.taskId;text: qsTr("取消提醒");visible: menu.target.hasReminder;onTriggered: menu.dispatch("cancelReminder",null)}
        DS.MenuSeparator {}
        DS.Menu {
            objectName:"taskMenuMove-"+menu.target.taskId;title: qsTr("移到");enabled: !menu.target.isDone
            MenuItem {text: qsTr("收集箱");onTriggered: menu.dispatch("view","inbox")}
            MenuItem {text: qsTr("今天");onTriggered: menu.dispatch("view","today")}
            MenuItem {text: qsTr("指定日期…");onTriggered: menu.dispatch("view","upcoming")}
        }
        MenuItem {text: menu.target.evening ? qsTr("安排到今天") : qsTr("安排到今晚");visible: menu.target.inToday && !menu.target.isDone;onTriggered: menu.dispatch("evening",!menu.target.evening)}
        MenuItem {text: qsTr("清除日期安排");visible: menu.target.canActivate;onTriggered: menu.dispatch("activate",null)}
        DS.MenuSeparator {}
        MenuItem {text: qsTr("上移");visible: menu.target.canReorder && menu.target.canMoveUp;onTriggered: menu.dispatch("move",-1)}
        MenuItem {text: qsTr("下移");visible: menu.target.canReorder && menu.target.canMoveDown;onTriggered: menu.dispatch("move",1)}
        DS.MenuSeparator {}
        DS.MenuItem {objectName: "taskMenuDelete-"+menu.target.taskId;text: qsTr("删除任务…");destructive: true;onTriggered: menu.dispatch("delete",null)}
    }
