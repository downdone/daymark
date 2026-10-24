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
    readonly property bool twoColumns: width-24>=440*Theme.scale
    property bool infoExpanded: false
    readonly property var destinations: [{kind:"",id:"",title:qsTr("不分类")}].concat(projects.map(item=>({kind:"project",id:item.id,title:qsTr("项目 · %1").arg(item.title)}))).concat(areas.map(item=>({kind:"area",id:item.id,title:qsTr("领域 · %1").arg(item.title)})))
    readonly property var selectedDestination: destinations[destinationChoice.currentIndex] || destinations[0]
    property var tagIds: []
    property var relatedEvents: []
    property int lastDestinationIndex: 0
    signal createCatalogRequested(string kind)
    signal draftChanged()
    signal openEvent(string eventId)
    signal focusRequested(Item item)
    spacing: 16
    function loadTask(item) {
        taskData=item
        projects=workspace.catalogOf("project");areas=workspace.catalogOf("area");tags=workspace.catalogOf("tag")
        relatedEvents=workspace.eventsForTask(item.taskId)
        tagIds=item.tagIds.slice()
        infoExpanded=false
        destinationChoice.currentIndex=Math.max(0,destinations.findIndex(value=>item.projectId ? value.kind==="project" && value.id===item.projectId : item.areaId ? value.kind==="area" && value.id===item.areaId : value.kind===""))
        lastDestinationIndex=destinationChoice.currentIndex
        loadHeadings()
        headingChoice.currentIndex=headings.findIndex(value=>value.id===item.headingId)+1
        checklist.clear()
        for(const entry of item.checklist)checklist.append({itemId:entry.id,itemTitle:entry.title,itemDone:entry.done})
        newItem.clear()
    }
    function refreshCatalog() {
        const previous=selectedDestination
        const heading=headingChoice.currentIndex>0 ? headings[headingChoice.currentIndex-1].id : ""
        projects=workspace.catalogOf("project");areas=workspace.catalogOf("area");tags=workspace.catalogOf("tag")
        destinationChoice.currentIndex=Math.max(0,destinations.findIndex(item=>item.kind===previous.kind && item.id===previous.id))
        lastDestinationIndex=destinationChoice.currentIndex
        loadHeadings();headingChoice.currentIndex=headings.findIndex(item=>item.id===heading)+1
    }
    function applyCreated(kind,id) {
        refreshCatalog()
        if(kind==="tag")toggleTag(id,true);else selectDestination(kind,id,"")
    }
    function toggleTag(id,value) {
        const next=tagIds.slice();const at=next.indexOf(id)
        if(value && at<0)next.push(id);else if(!value && at>=0)next.splice(at,1)
        tagIds=next;draftChanged()
    }
    function selectDestinationForVerification(kind,id,headingId){return selectDestination(kind,id,headingId)}
    function loadHeadings() {
        headings=selectedDestination.kind==="project" ? workspace.catalogOf("heading",selectedDestination.id) : []
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
        return {projectId:selectedDestination.kind==="project" ? selectedDestination.id : "",
            areaId:selectedDestination.kind==="area" ? selectedDestination.id : "",
            headingId:headingChoice.currentIndex>0 ? headings[headingChoice.currentIndex-1].id : "",
            tagIds:tagIds,checklist:items}
    }
    function selectDestination(kind,id,headingId) {
        const index=destinations.findIndex(item=>item.kind===kind && item.id===id)
        if(index<0)return false
        destinationChoice.currentIndex=index;destinationChoice.activated(index)
        if(headingId) {
            const at=headings.findIndex(item=>item.id===headingId)
            if(at<0)return false
            headingChoice.currentIndex=at+1;headingChoice.activated(at+1)
        }
        return true
    }
    function addChecklistItem() {
        if(!newItem.text.trim())return
        checklist.append({itemId:"",itemTitle:newItem.text.trim(),itemDone:false})
        newItem.clear();root.draftChanged();newItem.forceActiveFocus()
    }
    ListModel {id: checklist}
    DetailSection {
        heading: qsTr("整理");symbol: "project"
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("项目管理多步骤目标，领域管理长期职责；标签便于跨项目查找。都可不选。")}
        GridLayout {
            objectName: "editorAssignmentGrid"
            Layout.fillWidth: true
            columns: root.twoColumns ? 2 : 1
            columnSpacing: 16
            rowSpacing: 8
            ColumnLayout {
                Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                Label {text: qsTr("放入项目或领域");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {
                    id: destinationChoice;objectName: "editorDestination"
                    Layout.fillWidth: true
                    Accessible.name: qsTr("任务归属，项目或领域只能选择一个")
                    model: root.destinations.map(item=>item.title).concat([qsTr("新建项目…"),qsTr("新建领域…")])
                    onActivated: {if(currentIndex>=root.destinations.length){const kind=currentIndex===root.destinations.length ? "project" : "area";currentIndex=root.lastDestinationIndex;root.createCatalogRequested(kind)}else{root.lastDestinationIndex=currentIndex;root.loadHeadings();root.draftChanged()}}
                }
            }
            ColumnLayout {
                objectName: "editorHeadingRow"
                visible: root.selectedDestination.kind==="project" && root.headings.length>0
                Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                Label {text: qsTr("项目分组");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {
                    id: headingChoice;objectName: "editorHeading"
                    Layout.fillWidth: true
                    Accessible.name: qsTr("任务项目分组")
                    model: [qsTr("无分组")].concat(root.headings.map(item=>item.title))
                    onActivated: root.draftChanged()
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true;spacing: 12
            Label {text: qsTr("标签");Layout.alignment: Qt.AlignTop;Layout.topMargin: 7;font.pixelSize: Theme.captionSize;color: Theme.secondary}
            Flow {
                id: tagFlow;Layout.fillWidth: true;spacing: 6
                Label {visible: root.tags.length===0;text: qsTr("按地点或场景分类");topPadding: 7;bottomPadding: 7;color: Theme.secondary;font.pixelSize: Theme.captionSize}
                Repeater {
                    model: root.tags
                    CheckBox {
                        required property var modelData
                        width: Math.min(implicitWidth,tagFlow.width)
                        text: modelData.title
                        checked: root.tagIds.indexOf(modelData.id)>=0
                        onClicked: {
                            root.toggleTag(modelData.id,checked)
                        }
                    }
                }
                ActionButton {objectName: "newTagHere";symbol: "plus";text: qsTr("新建标签");onClicked: root.createCatalogRequested("tag")}
            }
        }
    }
    DetailSection {
        heading: qsTr("检查清单");symbol: "list";summary: checklist.count>0 ? qsTr("%1项").arg(checklist.count) : ""
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
            IconToolButton {contentItem: AppIcon {name: "up"} Accessible.name: qsTr("上移清单项");enabled: row.index>0;onClicked: {checklist.move(row.index,row.index-1,1);root.draftChanged()}}
            IconToolButton {contentItem: AppIcon {name: "trash";ink: Theme.danger} Accessible.name: qsTr("删除清单项");onClicked: {checklist.remove(row.index);root.draftChanged()}}
        }
    }
    RowLayout {
        Layout.fillWidth: true
        TextField {id: newItem;objectName: "editorNewChecklistItem";Layout.fillWidth: true;placeholderText: qsTr("新增步骤，Enter 添加");Accessible.name: qsTr("新增清单项");onTextEdited: root.draftChanged();onAccepted: root.addChecklistItem();onActiveFocusChanged: if(activeFocus)root.focusRequested(this)}
        IconToolButton {contentItem: AppIcon {name: "plus"} Accessible.name: qsTr("添加清单项");enabled: newItem.text.trim().length>0;onClicked: root.addChecklistItem()}
    }
    }
    DetailSection {
        heading: qsTr("关联事件");symbol: "calendar";visible: root.relatedEvents.length>0
    Repeater {
        model: root.relatedEvents
        ItemDelegate {
            required property var modelData
            Layout.fillWidth: true
            text: modelData.title+" · "+modelData.startDate
            onClicked: root.openEvent(modelData.eventId)
        }
    }
    }
    ActionButton {
        objectName: "taskInfoToggle";symbol: root.infoExpanded ? "up" : "down";text: qsTr("任务信息")
        checkable: true;checked: root.infoExpanded
        onClicked: root.infoExpanded=checked
    }
    ColumnLayout {
        objectName: "taskInfoContent";visible: root.infoExpanded;Layout.fillWidth: true;spacing: 4
    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("创建 %1\n修改 %2").arg(root.workspace.localReminder(root.taskData.createdAtUtc || "")).arg(root.workspace.localReminder(root.taskData.updatedAtUtc || ""));font.pixelSize: Theme.captionSize;color: Theme.secondary}
    Label {visible: !!root.taskData.completedAtUtc;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("完成 %1").arg(root.workspace.localReminder(root.taskData.completedAtUtc || ""));font.pixelSize: Theme.captionSize;color: Theme.secondary}
    }
}
