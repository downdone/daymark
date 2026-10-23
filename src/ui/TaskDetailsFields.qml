pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property var taskData: ({})
    property var projects: []
    property var areas: []
    property var headings: []
    property var tags: []
    property var tagIds: []
    property var relatedEvents: []
    signal draftChanged()
    signal openEvent(string eventId)
    signal focusRequested(Item item)
    spacing: Theme.fieldGap
    function loadTask(item) {
        taskData=item
        projects=workspace.catalogOf("project");areas=workspace.catalogOf("area");tags=workspace.catalogOf("tag")
        relatedEvents=workspace.eventsForTask(item.taskId)
        tagIds=item.tagIds.slice()
        projectChoice.currentIndex=projects.findIndex(value=>value.id===item.projectId)+1
        areaChoice.currentIndex=areas.findIndex(value=>value.id===item.areaId)+1
        loadHeadings()
        headingChoice.currentIndex=headings.findIndex(value=>value.id===item.headingId)+1
        checklist.clear()
        for(const entry of item.checklist)checklist.append({itemId:entry.id,itemTitle:entry.title,itemDone:entry.done})
        newItem.clear();evening.checked=item.evening
        reminder.text=workspace.localReminder(item.remindAtUtc)
    }
    function loadHeadings() {
        headings=projectChoice.currentIndex>0 ? workspace.catalogOf("heading",projects[projectChoice.currentIndex-1].id) : []
        headingChoice.currentIndex=0
    }
    function detailsPatch() {
        const items=[]
        for(let i=0;i<checklist.count;i++) {
            const item=checklist.get(i)
            items.push({id:item.itemId,title:item.itemTitle,done:item.itemDone})
        }
        // Unsubmitted checklist text belongs to the same draft and is included on Save.
        if(newItem.text.trim())items.push({id:"",title:newItem.text.trim(),done:false})
        return {projectId:projectChoice.currentIndex>0 ? projects[projectChoice.currentIndex-1].id : "",
            areaId:areaChoice.currentIndex>0 ? areas[areaChoice.currentIndex-1].id : "",
            headingId:headingChoice.currentIndex>0 ? headings[headingChoice.currentIndex-1].id : "",
            tagIds:tagIds,checklist:items,evening:evening.checked,remindAtLocal:reminder.text}
    }
    function addChecklistItem() {
        if(!newItem.text.trim())return
        checklist.append({itemId:"",itemTitle:newItem.text.trim(),itemDone:false})
        newItem.clear();root.draftChanged();newItem.forceActiveFocus()
    }
    ListModel {id: checklist}
    SectionLabel {text: qsTr("归属")}
    Label {text: qsTr("项目");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    ComboBox {
        id: projectChoice
        objectName: "editorProject"
        Layout.fillWidth: true
        Accessible.name: qsTr("任务项目")
        model: [qsTr("无项目")].concat(root.projects.map(item=>item.title))
        onActivated: {if(currentIndex>0)areaChoice.currentIndex=0;root.loadHeadings();root.draftChanged()}
    }
    Label {text: qsTr("直接领域");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    ComboBox {
        id: areaChoice
        Layout.fillWidth: true
        Accessible.name: qsTr("任务直接领域，与项目互斥")
        model: [qsTr("无直接领域")].concat(root.areas.map(item=>item.title))
        onActivated: {if(currentIndex>0){projectChoice.currentIndex=0;root.loadHeadings()}root.draftChanged()}
    }
    Label {text: qsTr("项目分组");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    ComboBox {
        id: headingChoice
        Layout.fillWidth: true
        Accessible.name: qsTr("任务项目分组")
        enabled: root.headings.length>0
        model: [qsTr("无分组")].concat(root.headings.map(item=>item.title))
        onActivated: root.draftChanged()
    }
    SectionLabel {text: qsTr("标签")}
    Label {visible: root.tags.length===0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("暂无标签，可在设置与工具中创建。");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: root.tags
            CheckBox {
                required property var modelData
                text: modelData.title
                checked: root.tagIds.indexOf(modelData.id)>=0
                onClicked: {
                    const next=root.tagIds.slice();const at=next.indexOf(modelData.id)
                    if(checked && at<0)next.push(modelData.id);else if(!checked && at>=0)next.splice(at,1)
                    root.tagIds=next;root.draftChanged()
                }
            }
        }
    }
    SectionLabel {text: qsTr("检查清单")}
    Label {visible: checklist.count===0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("把任务拆成可勾选的小步骤。");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    Repeater {
        model: checklist
        RowLayout {
            id: row
            required property int index
            required property string itemTitle
            required property bool itemDone
            Layout.fillWidth: true
            spacing: 4
            CheckBox {checked: row.itemDone;Accessible.name: qsTr("完成清单项：")+row.itemTitle;onClicked: {checklist.setProperty(row.index,"itemDone",checked);root.draftChanged()}}
            TextField {text: row.itemTitle;Layout.fillWidth: true;Accessible.name: qsTr("清单项标题");onTextEdited: {checklist.setProperty(row.index,"itemTitle",text);root.draftChanged()}}
            ToolButton {contentItem: AppIcon {name: "up"} Accessible.name: qsTr("上移清单项");enabled: row.index>0;onClicked: {checklist.move(row.index,row.index-1,1);root.draftChanged()}}
            ToolButton {contentItem: AppIcon {name: "trash";ink: Theme.danger} Accessible.name: qsTr("删除清单项");onClicked: {checklist.remove(row.index);root.draftChanged()}}
        }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {id: newItem;objectName: "editorNewChecklistItem";Layout.fillWidth: true;placeholderText: qsTr("新增步骤，Enter 添加");Accessible.name: qsTr("新增清单项");onTextEdited: root.draftChanged();onAccepted: root.addChecklistItem();onActiveFocusChanged: if(activeFocus)root.focusRequested(this)}
        ToolButton {contentItem: AppIcon {name: "plus"} Accessible.name: qsTr("添加清单项");enabled: newItem.text.trim().length>0;onClicked: root.addChecklistItem()}
    }
    SectionLabel {text: qsTr("提醒与执行")}
    CheckBox {id: evening;text: qsTr("安排到今晚");onClicked: root.draftChanged()}
    Label {text: qsTr("提醒 · 本地时间");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    TextField {id: reminder;objectName: "editorReminder";Layout.fillWidth: true;placeholderText: "yyyy-MM-dd HH:mm";Accessible.name: qsTr("本地时间提醒，留空清除");onTextEdited: root.draftChanged()}
    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("留空清除。程序运行时检查提醒，系统展示取决于通知权限。");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    SectionLabel {text: qsTr("关联事件")}
    Label {visible: root.relatedEvents.length===0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("暂无关联事件，可在日历事件中关联此任务。");font.pixelSize: Theme.captionSize;color: Theme.secondary}
    Repeater {
        model: root.relatedEvents
        ItemDelegate {
            required property var modelData
            Layout.fillWidth: true
            text: modelData.title+" · "+modelData.startDate
            onClicked: root.openEvent(modelData.eventId)
        }
    }
    SectionLabel {text: qsTr("任务信息")}
    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("创建 %1\n修改 %2").arg(root.workspace.localReminder(root.taskData.createdAtUtc || "")).arg(root.workspace.localReminder(root.taskData.updatedAtUtc || ""));font.pixelSize: Theme.captionSize;color: Theme.secondary}
    Label {visible: !!root.taskData.completedAtUtc;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("完成 %1").arg(root.workspace.localReminder(root.taskData.completedAtUtc || ""));font.pixelSize: Theme.captionSize;color: Theme.secondary}
}
