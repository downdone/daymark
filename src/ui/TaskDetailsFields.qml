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
    property bool tagExpanded:false
    property bool parentExpanded:false
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
        taskData=item;parentExpanded=false;tagExpanded=false;detachedParentTitle=""
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
        heading: qsTr("分类与标签");symbol: "folder"
        GridLayout {
            objectName: root.objectPrefix+"editorAssignmentGrid"
            Layout.fillWidth: true
            columns: root.twoColumns ? 2 : 1
            columnSpacing: 16
            rowSpacing: 8
            ColumnLayout {
                Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                Label {text: qsTr("分类");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {
                    id: destinationChoice;objectName: root.objectPrefix+"editorDestination"
                    Layout.fillWidth: true
                    Accessible.name: qsTr("任务分类")
                    model: root.destinations.map(item=>item.title).concat(root.creationMode ? [] : [qsTr("新建分类…")])
                    onActivated: {if(currentIndex>=root.destinations.length){const kind=currentIndex===root.destinations.length ? "project" : "area";currentIndex=root.lastDestinationIndex;root.createCatalogRequested(kind)}else{root.lastDestinationIndex=currentIndex;root.loadHeadings();if(root.parentTaskId){const parent=root.workspace.task(root.parentTaskId);const selected=root.selectedDestination;const parentId=parent.projectId || parent.areaId || "";if(selected.id!==parentId){root.detachedParentTitle=root.parentTaskTitle;root.parentTaskId="";root.parentTaskTitle=""}}root.draftChanged()}}
                }
            }
            ColumnLayout {
                objectName: root.objectPrefix+"editorHeadingRow"
                visible: root.selectedDestination.kind==="project" && root.headings.length>0
                Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                Label {text: qsTr("分类分组");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                ComboBox {
                    id: headingChoice;objectName: root.objectPrefix+"editorHeading"
                    Layout.fillWidth: true
                    Accessible.name: qsTr("分类分组")
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
                Repeater {
                    model: root.inspectorMode ? root.tags.filter(item=>root.tagIds.indexOf(item.id)>=0).slice(0,4) : root.tags
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
                ActionButton {symbol:"tag";text:root.tagExpanded ? qsTr("收起标签") : qsTr("选择标签…");onClicked:root.tagExpanded=!root.tagExpanded}
                ActionButton {objectName: root.objectPrefix+"newTagHere";visible:!root.creationMode;symbol: "plus";text: qsTr("新建标签");onClicked: root.createCatalogRequested("tag")}
            }
        }
        ColumnLayout {
            visible:root.tagExpanded;Layout.fillWidth:true
            TextField {id:tagSearch;Layout.fillWidth:true;placeholderText:qsTr("搜索标签…")}
            ListView {Layout.fillWidth:true;Layout.preferredHeight:Math.min(240,Math.max(40,count*36));clip:true;model:root.tags.filter(item=>!tagSearch.text || item.title.toLowerCase().indexOf(tagSearch.text.toLowerCase())>=0)
                delegate:CheckBox {required property var modelData;width:ListView.view.width;text:modelData.title;checked:root.tagIds.indexOf(modelData.id)>=0;onClicked:root.toggleTag(modelData.id,checked)}
                ScrollBar.vertical:ScrollBar{}
            }
        }
        Label {visible:!!root.detachedParentTitle;Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("保存后将移出父任务：%1").arg(root.detachedParentTitle);font.pixelSize:Theme.captionSize;color:Theme.secondary}
        GridLayout {
            Layout.fillWidth: true;columns: root.twoColumns ? 2 : 1;columnSpacing: 24;rowSpacing: 4
            SettingSwitch {id: important;objectName: root.objectPrefix+"editorImportant";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("特别关注");symbol: "flag";onClicked: root.draftChanged()}
            SettingSwitch {id: pinned;objectName: root.objectPrefix+"editorPinned";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("置顶显示");symbol: "up";onClicked: root.draftChanged()}
        }
    }
    DetailSection {
        visible:!root.creationMode;heading: qsTr("任务关系");symbol: "list"
        Label {text: qsTr("父任务");font.pixelSize: Theme.captionSize;color: Theme.secondary}
        Button {objectName: root.objectPrefix+"parentTaskButton";Layout.fillWidth: true;text: root.parentTaskId ? (root.taskData.parentDeleted ? qsTr("原父任务（已删除）：%1").arg(root.parentTaskTitle) : root.parentTaskTitle) : qsTr("无父任务");onClicked: {parentSearch.clear();root.refreshParentChoices();if(root.inspectorMode)root.parentExpanded=!root.parentExpanded;else parentPicker.open()}}
        ColumnLayout {
            visible:root.parentExpanded;Layout.fillWidth:true
            TextField {Layout.fillWidth:true;placeholderText:qsTr("搜索父任务…");onTextEdited:{parentSearch.text=text;root.refreshParentChoices()}}
            Button {text:qsTr("移出父任务");visible:!!root.parentTaskId;onClicked:{root.parentTaskId="";root.parentTaskTitle="";root.draftChanged();root.parentExpanded=false}}
            ListView {Layout.fillWidth:true;Layout.preferredHeight:Math.min(220,Math.max(40,count*36));clip:true;model:root.parentChoices
                delegate:ItemDelegate {required property var modelData;width:ListView.view.width;text:modelData.title;onClicked:{const parent=root.workspace.task(modelData.id);root.selectDestination(parent.projectId ? "project" : parent.areaId ? "area" : "",parent.projectId || parent.areaId || "",parent.headingId || "");root.parentTaskId=modelData.id;root.parentTaskTitle=modelData.title;root.detachedParentTitle="";root.draftChanged();root.parentExpanded=false}}
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
                    delegate: ItemDelegate {required property var modelData;width: ListView.view.width;text: modelData.title;highlighted: root.parentTaskId===modelData.id;onClicked: {root.parentTaskId=modelData.id;root.parentTaskTitle=modelData.title;const parent=root.workspace.task(modelData.id);root.selectDestination(parent.projectId ? "project" : parent.areaId ? "area" : "",parent.projectId || parent.areaId || "",parent.headingId || "");root.draftChanged();parentPicker.close()}}
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
