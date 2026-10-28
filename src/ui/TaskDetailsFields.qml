pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property TaskWorkspace workspace
    property string objectPrefix: ""
    property var taskData: ({})
    property var projects: []
    property var areas: []
    property var headings: []
    property var tags: []
    readonly property bool twoColumns: width-24>=440*Theme.scale
    property bool infoExpanded: false
    property bool inspectorMode:false
    property bool creationMode:false
    readonly property bool tagPickerVisible: tagMenu.visible
    property bool parentExpanded:false
    readonly property bool importantValue: important.checked
    function setImportant(value){if(important.checked!==value){important.checked=value;draftChanged()}}
    property string detachedParentTitle:""
    readonly property var destinations: [{kind:"",id:"",title:qsTr("收集箱")}].concat(workspace.categoryTree.map(item=>({kind:item.kind,id:item.id,title:item.path,depth:item.depth,parentTitle:item.parentTitle})))
    readonly property var selectedDestination: destinations[destinationChoice.currentIndex] || destinations[0]
    property var tagIds: []
    property var relatedEvents: []
    property int lastDestinationIndex: 0
    property string parentTaskId: ""
    property string parentTaskTitle: ""
    property var parentChoices: []
    readonly property bool parentPickerVisible: parentPicker.visible || parentExpanded
    function refreshParentChoices(){parentChoices=workspace.parentTaskChoices(taskData.taskId,parentSearch.text)}
    signal createCatalogRequested(string kind)
    signal draftChanged()
    signal openEvent(string eventId)
    signal focusRequested(Item item)
    spacing: Theme.fieldGap
    function loadTask(item) {
        taskData=item;parentExpanded=false;tagMenu.close();detachedParentTitle=""
        projects=workspace.catalogOf("project");areas=workspace.catalogOf("area");tags=workspace.catalogOf("tag")
        relatedEvents=creationMode ? [] : workspace.eventsForTask(item.taskId).concat((item.links || []).map(link=>Object.assign({},workspace.task(link.taskId),{eventId:link.taskId})).filter(link=>!!link.taskId))
        tagIds=item.tagIds.slice()
        destinationChoice.currentIndex=Math.max(0,destinations.findIndex(value=>item.projectId ? value.kind==="project" && value.id===item.projectId : item.areaId ? value.kind==="area" && value.id===item.areaId : value.kind===""))
        lastDestinationIndex=destinationChoice.currentIndex
        loadHeadings()
        headingChoice.currentIndex=headings.findIndex(value=>value.id===item.headingId)+1
        parentTaskId=item.parentTaskId || "";parentTaskTitle=item.parentTitle || "";important.checked=!!item.important;pinned.checked=!!item.pinned
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
        return {projectId:selectedDestination.kind==="project" ? selectedDestination.id : "",
            areaId:selectedDestination.kind==="area" ? selectedDestination.id : "",
            headingId:headingChoice.currentIndex>0 ? headings[headingChoice.currentIndex-1].id : "",
            tagIds:tagIds,parentTaskId:parentTaskId,important:important.checked,pinned:pinned.checked}
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
    DetailSection {
        plain:root.inspectorMode
        heading: root.inspectorMode ? "" : qsTr("分类与标签");symbol: "folder"
        ColumnLayout {
            objectName:root.objectPrefix+"editorAssignmentGrid";Layout.fillWidth:true;spacing:Theme.fieldGap
            FormRow {contextual:root.inspectorMode;
                label:qsTr("分类")
                ComboBox {
                    id:destinationChoice;objectName:root.objectPrefix+"editorDestination";Layout.fillWidth:true
                    Accessible.name:qsTr("任务分类")
                    model:root.destinations.map(item=>item.title).concat(root.creationMode ? [] : [qsTr("新建分类…")])
                    onActivated:{if(currentIndex>=root.destinations.length){const kind=currentIndex===root.destinations.length ? "project" : "area";currentIndex=root.lastDestinationIndex;root.createCatalogRequested(kind)}else{root.lastDestinationIndex=currentIndex;root.loadHeadings();if(root.parentTaskId){const parent=root.workspace.task(root.parentTaskId);const selected=root.selectedDestination;const parentId=parent.projectId || parent.areaId || "";if(selected.id!==parentId){root.detachedParentTitle=root.parentTaskTitle;root.parentTaskId="";root.parentTaskTitle=""}}root.draftChanged()}}
                }
            }
            FormRow {contextual:root.inspectorMode;
                objectName:root.objectPrefix+"editorHeadingRow";label:qsTr("分组");visible:root.selectedDestination.kind==="project" && root.headings.length>0
                ComboBox {id:headingChoice;objectName:root.objectPrefix+"editorHeading";Layout.fillWidth:true;Accessible.name:qsTr("分类分组");model:[qsTr("无分组")].concat(root.headings.map(item=>item.title));onActivated:root.draftChanged()}
            }
        }
        FormRow {contextual:root.inspectorMode;
            label: qsTr("标签")
            ActionButton {
                id: tagButton;objectName: root.objectPrefix+"editorTags"
                Layout.fillWidth: true;symbol: "tag";contentAlignment:Qt.AlignLeft
                text: root.tags.filter(item=>root.tagIds.indexOf(item.id)>=0).map(item=>item.title).join("、") || qsTr("选择标签…")
                Accessible.name: qsTr("选择标签，已选%1个：%2").arg(root.tagIds.length).arg(text)
                ToolTip.visible: hovered && textTruncated && !down && !Theme.hintsBlocked
                ToolTip.delay: Theme.hintDelay;ToolTip.timeout: Theme.hintTimeout
                ToolTip.text: text
                onClicked: tagMenu.popup(tagButton,0,tagButton.height)
                TagMenu {
                    id: tagMenu;objectName: root.objectPrefix+"editorTagMenu"
                    options: root.tags;selectedIds: root.tagIds
                    onSelected: function(id, checked) {root.toggleTag(id, checked)}
                    onCreateRequested: root.createCatalogRequested("tag")
                }
            }
        }
        Label {visible:!!root.detachedParentTitle;Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("保存后将移出父任务：%1").arg(root.detachedParentTitle);font.pixelSize:Theme.captionSize;color:Theme.secondary}
        GridLayout {
            Layout.fillWidth: true;columns: root.width>=320*Theme.scale ? 2 : 1;columnSpacing: 12;rowSpacing: 4
            SettingSwitch {id: important;objectName: root.objectPrefix+"editorImportant";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("特别关注");symbol: "flag";onClicked: root.draftChanged()}
            SettingSwitch {id: pinned;objectName: root.objectPrefix+"editorPinned";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("置顶显示");symbol: "up";onClicked: root.draftChanged()}
        }
    }
    DetailSection {
        plain:root.inspectorMode;visible:!root.creationMode;heading: root.inspectorMode ? "" : qsTr("任务关系");symbol: "list"
        FormRow {contextual:root.inspectorMode;label:qsTr("父任务")
        Button {objectName: root.objectPrefix+"parentTaskButton";flat:root.inspectorMode;Layout.fillWidth: true;text: root.parentTaskId ? (root.taskData.parentDeleted ? qsTr("原父任务（已删除）：%1").arg(root.parentTaskTitle) : root.parentTaskTitle) : qsTr("无父任务");onClicked: {parentSearch.clear();root.refreshParentChoices();if(root.inspectorMode)root.parentExpanded=!root.parentExpanded;else parentPicker.open()}}
        }
        ColumnLayout {
            visible:root.parentExpanded;Layout.fillWidth:true
            TextField {Layout.fillWidth:true;placeholderText:qsTr("搜索父任务…");onTextEdited:{parentSearch.text=text;root.refreshParentChoices()}}
            Button {text:qsTr("移出父任务");visible:!!root.parentTaskId;onClicked:{root.parentTaskId="";root.parentTaskTitle="";root.draftChanged();root.parentExpanded=false}}
            ListView {Layout.fillWidth:true;Layout.preferredHeight:Math.min(220,Math.max(40,count*36));clip:true;model:root.parentChoices
                delegate:ItemDelegate {required property var modelData;width:ListView.view.width-Theme.scrollGutter;text:modelData.title;onClicked:{const parent=root.workspace.task(modelData.id);root.selectDestination(parent.projectId ? "project" : parent.areaId ? "area" : "",parent.projectId || parent.areaId || "",parent.headingId || "");root.parentTaskId=modelData.id;root.parentTaskTitle=modelData.title;root.detachedParentTitle="";root.draftChanged();root.parentExpanded=false}}
                ScrollBar.vertical:ScrollBar{}
            }
            Button {text:qsTr("收起");onClicked:root.parentExpanded=false}
        }
        Popup {
            id: parentPicker;objectName: root.objectPrefix+"parentTaskPicker";parent: Overlay.overlay;anchors.centerIn: parent
            width: Math.min(520,parent.width-32);height: Math.min(480,parent.height-32);padding: 12;modal: true;focus: true
            background: Rectangle {color: Theme.content;radius: Theme.panelRadius;border.color: Theme.separator}
            contentItem: ColumnLayout {
                TextField {id: parentSearch;Layout.fillWidth: true;placeholderText: qsTr("搜索父任务…");onTextEdited: root.refreshParentChoices()}
                Label {visible: root.parentChoices.length===0;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("没有可选父任务。仅能归入根任务；已有下级的任务需先移出下级。")}
                Button {text: qsTr("无父任务");onClicked: {root.parentTaskId="";root.parentTaskTitle="";root.draftChanged();parentPicker.close()}}
                ListView {
                    Layout.fillWidth: true;Layout.fillHeight: true;clip: true;model: root.parentChoices
                    delegate: ItemDelegate {required property var modelData;width: ListView.view.width-Theme.scrollGutter;text: modelData.title;highlighted: root.parentTaskId===modelData.id;onClicked: {root.parentTaskId=modelData.id;root.parentTaskTitle=modelData.title;const parent=root.workspace.task(modelData.id);root.selectDestination(parent.projectId ? "project" : parent.areaId ? "area" : "",parent.projectId || parent.areaId || "",parent.headingId || "");root.draftChanged();parentPicker.close()}}
                }
            }
        }
    }
    DetailSection {
        heading: qsTr("关联任务");symbol: "calendar";visible: root.relatedEvents.length>0
    Repeater {
        model: root.relatedEvents
        ItemDelegate {
            required property var modelData
            Layout.fillWidth: true
            text: modelData.title+(modelData.startDate ? " · "+modelData.startDate : "")
            onClicked: root.openEvent(modelData.eventId)
        }
    }
    }
}
