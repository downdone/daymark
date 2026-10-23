pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property TaskWorkspace workspace
    property var taskData: ({})
    property var projects: []
    property var areas: []
    property var headings: []
    property var tagIds: []
    property bool dirty: false
    signal openEvent(string eventId)
    property string pendingEvent: ""
    property var relatedEvents: []
    title: qsTr("归属、标签和清单")
    modal: true
    closePolicy: Popup.NoAutoClose
    width: Math.min(620, parent.width - 40)
    height: Math.min(610, parent.height - 40)
    function openTask(id) {
        taskData = workspace.task(id)
        relatedEvents=workspace.eventsForTask(id);pendingEvent=""
        projects = workspace.catalogOf("project")
        areas = workspace.catalogOf("area")
        tagIds = taskData.tagIds.slice()
        projectChoice.currentIndex = 0
        areaChoice.currentIndex = 0
        for (let i=0; i<projects.length; ++i) if (projects[i].id === taskData.projectId) projectChoice.currentIndex=i+1
        for (let i=0; i<areas.length; ++i) if (areas[i].id === taskData.areaId) areaChoice.currentIndex=i+1
        loadHeadings()
        for (let i=0; i<headings.length; ++i) if (headings[i].id === taskData.headingId) headingChoice.currentIndex=i+1
        checklist.clear()
        for (const item of taskData.checklist) checklist.append({itemId:item.id, itemTitle:item.title, itemDone:item.done})
        evening.checked = taskData.evening
        reminder.text=workspace.localReminder(taskData.remindAtUtc)
        dirty=false
        open()
    }
    function loadHeadings() {
        headings = projectChoice.currentIndex > 0 ? workspace.catalogOf("heading", projects[projectChoice.currentIndex-1].id) : []
        headingChoice.currentIndex=0
    }
    function save() {
        const items=[]
        for (let i=0; i<checklist.count; ++i) { const item=checklist.get(i); items.push({id:item.itemId,title:item.itemTitle,done:item.itemDone}) }
        workspace.saveDetails(taskData.taskId,taskData.rowVersion,{
            projectId:projectChoice.currentIndex>0?projects[projectChoice.currentIndex-1].id:"",
            areaId:areaChoice.currentIndex>0?areas[areaChoice.currentIndex-1].id:"",
            headingId:headingChoice.currentIndex>0?headings[headingChoice.currentIndex-1].id:"",
            tagIds:tagIds,checklist:items,evening:evening.checked,remindAtLocal:reminder.text})
    }
    ListModel { id: checklist }
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: root.availableWidth - 16
            spacing: Theme.fieldGap
            enabled: !root.workspace.busy
            Label { text: root.taskData.title || ""; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: qsTr("创建 %1\n修改 %2").arg(root.workspace.localReminder(root.taskData.createdAtUtc || "")).arg(root.workspace.localReminder(root.taskData.updatedAtUtc || "")); color: Theme.secondary; font.pixelSize: Theme.captionSize }
            Label { visible: !!root.taskData.completedAtUtc; text: qsTr("完成 %1").arg(root.workspace.localReminder(root.taskData.completedAtUtc || "")); color: Theme.secondary; font.pixelSize: Theme.captionSize }
            SectionLabel {text: qsTr("关联事件");visible: root.relatedEvents.length>0}
            Repeater {
                model: root.relatedEvents
                ItemDelegate {
                    id: related
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.title+" · "+modelData.startDate
                    onClicked: {if(root.dirty){root.pendingEvent=modelData.eventId;discard.open()}else{root.close();root.openEvent(modelData.eventId)}}
                }
            }
            SectionLabel { text: qsTr("归属") }
            Label {text: qsTr("项目 / 直接领域（二选一）");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            ComboBox {
                id: projectChoice
                Layout.fillWidth: true
                model: [qsTr("无项目")].concat(root.projects.map(function(item){return item.title}))
                onActivated: { if(currentIndex>0) areaChoice.currentIndex=0; root.loadHeadings(); root.dirty=true }
            }
            ComboBox {
                id: areaChoice
                Layout.fillWidth: true
                model: [qsTr("无直接领域")].concat(root.areas.map(function(item){return item.title}))
                onActivated: {if(currentIndex>0){projectChoice.currentIndex=0;root.loadHeadings()}root.dirty=true}
            }
            ComboBox { id: headingChoice; Layout.fillWidth: true; model: [qsTr("无分组")].concat(root.headings.map(function(item){return item.title})); onActivated: root.dirty=true }
            CheckBox { id: evening; text: qsTr("今晚（仅Today中的任务显示该分组）"); onClicked: root.dirty=true }
            Label { text: qsTr("本地时间提醒（yyyy-MM-dd HH:mm，留空清除）") }
            TextField { id: reminder; Layout.fillWidth: true; placeholderText: "2026-10-23 09:00"; selectByMouse: true; onTextEdited: root.dirty=true }
            Label { text: qsTr("程序运行时检查提醒；已触发记录保留在提醒中心。系统是否展示取决于通知权限。"); Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
            SectionLabel { text: qsTr("标签") }
            Flow {
                Layout.fillWidth: true
                Repeater {
                    model: root.workspace.catalogOf("tag")
                    CheckBox {
                        required property var modelData
                        text: modelData.title
                        checked: root.tagIds.indexOf(modelData.id)>=0
                        onClicked: {
                            const next=root.tagIds.slice();const at=next.indexOf(modelData.id)
                            if(checked && at<0)next.push(modelData.id);else if(!checked && at>=0)next.splice(at,1)
                            root.tagIds=next;root.dirty=true
                        }
                    }
                }
            }
            SectionLabel { text: qsTr("检查清单") }
            Repeater {
                model: checklist
                RowLayout {
                    id: checklistRow
                    required property int index
                    required property string itemTitle
                    required property bool itemDone
                    Layout.fillWidth: true
                    CheckBox { checked: checklistRow.itemDone; onClicked: {checklist.setProperty(checklistRow.index,"itemDone",checked);root.dirty=true} }
                    TextField { text: checklistRow.itemTitle; Layout.fillWidth: true; onTextEdited: {checklist.setProperty(checklistRow.index,"itemTitle",text);root.dirty=true} }
                    ToolButton {contentItem: AppIcon {name: "up"} Accessible.name: qsTr("上移清单项");enabled: checklistRow.index>0; onClicked: {checklist.move(checklistRow.index,checklistRow.index-1,1);root.dirty=true} }
                    ToolButton {contentItem: AppIcon {name: "trash";ink: Theme.danger} Accessible.name: qsTr("删除清单项"); onClicked: {checklist.remove(checklistRow.index);root.dirty=true} }
                }
            }
            RowLayout {
                TextField { id: newItem; Layout.fillWidth: true; placeholderText: qsTr("新增清单项，Enter添加"); onAccepted: addItem.clicked() }
                Button { id: addItem; text: qsTr("添加"); onClicked: { if(newItem.text.trim()){checklist.append({itemId:"",itemTitle:newItem.text.trim(),itemDone:false});newItem.clear();root.dirty=true} } }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.errorText; color: Theme.danger }
        }
    }
    footer: DialogButtonBox {
        Button { text: qsTr("返回"); enabled: !root.workspace.busy; onClicked: {if(root.dirty)discard.open();else root.close()} }
        Button { text: qsTr("保存"); highlighted: true; enabled: !root.workspace.busy; onClicked: root.save() }
    }
    Dialog {
        id: discard
        anchors.centerIn: parent
        modal: true
        title: qsTr("放弃未保存的修改？")
        standardButtons: Dialog.Ok|Dialog.Cancel
        onAccepted: {root.dirty=false;root.close();if(root.pendingEvent){const id=root.pendingEvent;root.pendingEvent="";root.openEvent(id)}}
        onRejected: root.pendingEvent=""
    }
    Connections { target: root.workspace; function onOperationFinished(operation,success){if(operation==="details" && success){root.dirty=false;root.close()}} }
}
