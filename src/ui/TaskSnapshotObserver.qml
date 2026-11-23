pragma ComponentBehavior: Bound
import QtQuick

QtObject {
    id: root
    required property TaskWorkspace workspace
    required property string taskId
    required property string profile
    required property bool active
    required property bool dirty
    required property bool saving
    property bool pending: false
    signal latestAvailable(var snapshot)
    signal conflict(string message, bool canReload)

    function check() {
        if(!pending || saving || workspace.busy)return
        pending=false
        if(!active)return
        if(profile!==workspace.profileGeneration){conflict(qsTr("资料已切换。请复制未保存内容，或关闭编辑。"),false);return}
        const latest=workspace.task(taskId)
        if(!latest.taskId){conflict(qsTr("任务已删除。请复制未保存内容，或关闭编辑。"),false);return}
        if(dirty){conflict(qsTr("该任务已在另一处修改。重新载入会放弃此处未保存的修改。"),true);return}
        latestAvailable(latest)
    }
    function schedule(){pending=true;Qt.callLater(check)}
    onTaskIdChanged:pending=false
    onActiveChanged:if(!active)pending=false
    onSavingChanged:if(!saving && pending)Qt.callLater(check)
    property Connections changes: Connections {
        target: root.workspace
        function onSnapshotCommitted(event){
            if(root.active && event.profileGeneration===root.profile &&
               (event.changedTaskIds.indexOf(root.taskId)>=0 || event.changedCycleTaskIds.indexOf(root.taskId)>=0))root.schedule()
        }
        function onProfileGenerationChanged(){if(root.active && root.profile!==root.workspace.profileGeneration)root.schedule()}
        function onStateChanged(){if(root.pending && !root.workspace.busy)Qt.callLater(root.check)}
    }
}
