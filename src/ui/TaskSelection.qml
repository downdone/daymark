pragma ComponentBehavior: Bound
import QtQuick

QtObject {
    id: root
    required property TaskWorkspace workspace
    property var items: []
    property string currentId: ""
    property string anchorId: ""
    property int revision: 0
    readonly property int count: items.length
    readonly property var membership: {
        const result={}
        for(const item of items)result[item.taskId]=true
        return result
    }
    function contains(id) {return membership[id]===true}
    function entry(id) {
        const task=workspace.task(id)
        return task.taskId ? {taskId:id,rowVersion:task.rowVersion} : null
    }
    function single(id) {
        const task=entry(id)
        items=task ? [task] : [];currentId=task ? id : "";anchorId=currentId
    }
    function clear() {items=[];currentId="";anchorId=""}
    function toggle(id) {
        const task=entry(id);if(!task)return
        items=contains(id) ? items.filter(value=>value.taskId!==id) : items.concat([task])
        currentId=id;anchorId=id
    }
    function range(id, additive) {
        const target=workspace.taskIndex(id);if(target<0)return
        const source=workspace.taskIndex(anchorId)
        if(source<0){single(id);return}
        const next=additive ? items.slice() : [],seen={}
        for(const value of next)seen[value.taskId]=true
        for(const value of workspace.tasks.selectionEntries(Math.min(source,target),Math.max(source,target))){
            if(!seen[value.taskId]){next.push(value);seen[value.taskId]=true}
        }
        items=next;currentId=id
    }
    function all() {
        const next=workspace.tasks.selectionEntries(0,workspace.tasks.count-1)
        items=next
        if(!anchorId && next.length)anchorId=next[0].taskId
        if(workspace.taskIndex(currentId)<0)currentId=next.length ? next[0].taskId : ""
    }
    function prune() {
        ++revision
        if(!items.length && !currentId && !anchorId)return
        const visible={}
        for(let i=0;i<workspace.tasks.count;++i)visible[workspace.tasks.taskIdAt(i)]=true
        const next=items.filter(value=>visible[value.taskId]===true)
        if(next.length!==items.length)items=next
        if(!visible[currentId])currentId=""
        if(!visible[anchorId])anchorId=""
    }
}
