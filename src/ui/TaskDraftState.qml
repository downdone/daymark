pragma ComponentBehavior: Bound
import QtQuick

QtObject {
    id: root
    required property var titleField
    required property var notesField
    required property var timeFields
    required property var details

    function capture(meta) {
        const patch=details.detailsPatch()
        return {
            title:titleField.text,notes:notesField.text,
            arrangement:{start:timeFields.startField.text,end:timeFields.endField.text,
                datesEdited:timeFields.datesEdited,timing:timeFields.timingFields.saveDraft(),
                reminder:{text:timeFields.reminderField.text,
                    frequency:timeFields.reminderField.repeatFrequency,
                    interval:timeFields.reminderField.repeatInterval},meta:meta},
            organization:{projectId:patch.projectId,areaId:patch.areaId,
                headingId:patch.headingId,parentTaskId:patch.parentTaskId},
            tags:patch.tagIds.slice().sort(),important:patch.important,pinned:patch.pinned
        }
    }
    function apply(value, latest) {
        titleField.text=value.title
        notesField.text=value.notes
        const parent=value.organization.parentTaskId ?
            details.workspace.task(value.organization.parentTaskId) : ({})
        details.loadTask(Object.assign({},latest,value.organization,{
            parentTitle:parent.title || "",tagIds:value.tags,
            important:value.important,pinned:value.pinned}))
        timeFields.restoring=true
        timeFields.startField.text=value.arrangement.start
        timeFields.endField.text=value.arrangement.end
        timeFields.timingFields.restoreDraft(value.arrangement.timing)
        timeFields.reminderField.text=value.arrangement.reminder.text
        timeFields.reminderField.repeatFrequency=value.arrangement.reminder.frequency
        timeFields.reminderField.repeatInterval=value.arrangement.reminder.interval
        timeFields.datesEdited=value.arrangement.datesEdited
        timeFields.restoring=false
    }
    function comparable(value) {
        if(Array.isArray(value))return value.map(comparable)
        if(value!==null && typeof value==="object"){
            const result={}
            for(const key of Object.keys(value).sort()){
                if(["datesEdited","timeDirty","repeatDirty","advanced"].indexOf(key)<0)
                    result[key]=comparable(value[key])
            }
            return result
        }
        return value
    }
    function same(a,b){return JSON.stringify(comparable(a))===JSON.stringify(comparable(b))}
    function describe(key,value) {
        if(key==="important" || key==="pinned")return value?qsTr("已开启"):qsTr("已关闭")
        if(key==="tags")return value.map(id=>(details.tags.find(tag=>tag.id===id) || {}).title || qsTr("已移除的标签")).join("、") || qsTr("无标签")
        if(key==="organization"){
            const catalog=details.projects.concat(details.areas).find(item=>item.id===(value.projectId || value.areaId))
            const parent=value.parentTaskId ? details.workspace.task(value.parentTaskId) : ({})
            return (catalog ? catalog.path || catalog.title : qsTr("收集箱"))+
                (parent.title ? qsTr(" · 父任务：%1").arg(parent.title) : "")
        }
        if(key==="arrangement"){
            const timing=value.timing
            const cadence=[qsTr("每天"),qsTr("每周"),qsTr("每月"),qsTr("每年"),qsTr("自定义")]
            return (value.start || value.end || qsTr("未安排"))+
                (value.end && value.end!==value.start ? " — "+value.end : "")+
                (timing.specific ? " · "+timing.start+"–"+timing.end : "")+
                (timing.repeat ? " · "+cadence[timing.frequency]+" ×"+timing.interval : "")+
                (value.reminder.text ? qsTr(" · 提醒：%1").arg(value.reminder.text) :
                    timing.alert>=0 ? qsTr(" · 开始前%1分钟提醒").arg(timing.alert) : qsTr(" · 不提醒"))
        }
        return value || qsTr("空")
    }
}
