pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id:root
    required property TaskWorkspace workspace
    property bool opened:false
    property bool previewing:false
    property bool submitting:false
    property string context:"inbox"
    property string profile:""
    property var candidates:[]
    property string error:""
    readonly property bool popupOpen:editor.visible
    readonly property int selectedCount:candidates.filter(item=>item.selected).length
    signal closeRequested()
    signal created(int count)
    visible:opened;color:Theme.content;radius:Theme.panelRadius
    function begin(currentContext,text,sourceProfile){
        if(text){raw.text=text;candidates=[];previewing=false;context=currentContext==="today"?"inbox":currentContext;profile=sourceProfile || workspace.widgetProfileToken}
        if(!raw.text.length && !candidates.length){context=currentContext==="today"?"inbox":currentContext;profile=workspace.widgetProfileToken}
        opened=true;error="";Qt.callLater(function(){if(!previewing)raw.forceActiveFocus()})
    }
    function replaceCandidate(index,patch){const next=candidates.slice();next[index]=Object.assign({},next[index],patch);candidates=next}
    function showPreview(){
        const result=workspace.previewActivityInput(raw.text)
        if(!result.valid){error=result.error;return}
        candidates=result.candidates;previewing=true;error=""
    }
    function editCandidate(index,source){
        editor.index=index;const item=candidates[index]
        candidateTitle.text=item.title;candidateStart.text=item.startOn || "";candidateEnd.text=item.endOn || "";candidateDeadline.text=item.deadlineOn || ""
        candidateStartTime.text=item.startTime || "";candidateEndTime.text=item.endTime || "";candidateNotes.text=item.notes
        const at=source.mapToItem(editor.parent,0,source.height);editor.x=at.x;editor.y=at.y;editor.open()
    }
    Connections {
        target:root.workspace
        function onOperationFinished(operation,success){
            if(operation!=="activityCreate" || !root.submitting)return
            root.submitting=false
            if(!success){root.error=root.workspace.errorText;return}
            const count=root.selectedCount;root.candidates=[];root.previewing=false;raw.clear();root.opened=false;root.created(count)
        }
    }
    MouseArea {anchors.fill:parent;acceptedButtons:Qt.AllButtons;onWheel:function(event){event.accepted=true}}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:Theme.panelPadding;spacing:8
        RowLayout {
            Layout.fillWidth:true
            Label {text:root.previewing?qsTr("确认提取结果"):qsTr("从文本提取任务");font.pixelSize:Theme.panelTitleSize;Layout.fillWidth:true}
            IconToolButton {Accessible.name:qsTr("返回任务列表，保留草稿");enabled:!root.submitting;contentItem:AppIcon{name:"close"} onClicked:root.closeRequested()}
        }
        Label {Layout.fillWidth:true;text:root.previewing?qsTr("确认后才创建，点击摘要可修改"):qsTr("本机离线提取，含糊时间由你确认");color:Theme.secondary;font.pixelSize:Theme.captionSize;wrapMode:Text.Wrap}
        Label {Layout.fillWidth:true;visible:!!root.error;text:root.error;color:Theme.danger;wrapMode:Text.Wrap}
        ScrollView {
            visible:!root.previewing;enabled:!root.submitting;Layout.fillWidth:true;Layout.fillHeight:true
            TextArea {id:raw;objectName:"widgetActivitySource";placeholderText:qsTr("粘贴会议安排或活动通知…");wrapMode:TextEdit.Wrap;selectByMouse:true;Accessible.name:qsTr("待提取的原文")}
        }
        ListView {
            objectName:"widgetActivityCandidates";enabled:!root.submitting;visible:root.previewing;Layout.fillWidth:true;Layout.fillHeight:true
            clip:true;model:root.candidates;spacing:8;ScrollBar.vertical:ScrollBar{}
            delegate:RowLayout {
                id:row;required property var modelData;required property int index
                width:ListView.view.width-Theme.scrollGutter;spacing:4
                CheckBox {checked:row.modelData.selected;Accessible.name:qsTr("创建%1").arg(row.modelData.title);onClicked:root.replaceCandidate(row.index,{selected:checked});Layout.alignment:Qt.AlignTop}
                ColumnLayout {
                    Layout.fillWidth:true;spacing:2
                    TextField {Layout.fillWidth:true;text:row.modelData.title;Accessible.name:qsTr("候选标题");onTextEdited:root.replaceCandidate(row.index,{title:text})}
                    ActionButton {objectName:"widgetCandidateTime-"+row.index;Layout.fillWidth:true;contentAlignment:Qt.AlignLeft;symbol:"calendar";text:row.modelData.issue || (row.modelData.startOn?(row.modelData.startOn+(row.modelData.startTime?" "+row.modelData.startTime+"—"+row.modelData.endTime:"")):row.modelData.deadlineOn?qsTr("截止 ")+row.modelData.deadlineOn:qsTr("未安排时间"));onClicked:root.editCandidate(row.index,this)}
                    Label {Layout.fillWidth:true;visible:!!row.modelData.place;text:row.modelData.place;elide:Text.ElideRight;font.pixelSize:Theme.captionSize;color:Theme.secondary}
                }
            }
        }
        ComboBox {
            id:target;enabled:!root.submitting;visible:root.previewing;Layout.fillWidth:true;textRole:"title";valueRole:"key"
            model:[{key:"inbox",title:qsTr("收集箱")}].concat(root.workspace.widgetCategories.map(item=>({key:item.key,title:item.path})))
            currentIndex:Math.max(0,model.findIndex(item=>item.key===root.context));Accessible.name:qsTr("创建任务的分类")
            onActivated:root.context=currentValue
        }
        RowLayout {
            Layout.fillWidth:true
            Button {visible:root.previewing;text:qsTr("修改原文");flat:true;enabled:!root.submitting;onClicked:root.previewing=false}
            Item {Layout.fillWidth:true}
            Button {
                objectName:"widgetActivityConfirm";text:root.previewing?qsTr("创建%1项").arg(root.selectedCount):qsTr("预览结果")
                highlighted:true;enabled:!root.workspace.busy && !root.submitting && (!root.previewing || root.selectedCount>0)
                onClicked:{if(!root.previewing){root.showPreview();return}root.submitting=root.workspace.createActivityTasks(root.candidates,root.context,root.profile);if(!root.submitting)root.error=root.workspace.widgetBrowseError || root.workspace.errorText || qsTr("请检查所选任务的标题和时间。")}
            }
        }
    }
    Popup {
        id:editor;objectName:"widgetCandidateEditor";property int index:-1
        popupType:Popup.Window;modal:false;focus:true;padding:12
        width:Math.min(380*Theme.scale,root.workspace.reminderPopupArea().width-24)
        height:Math.min(editorFields.implicitHeight+padding*2,root.workspace.reminderPopupArea().height-24)
        background:PopoverSurface{}
        contentItem:DS.ScrollView {
            implicitHeight:editorFields.implicitHeight;contentWidth:availableWidth
            ColumnLayout {
                id:editorFields;width:parent.width;spacing:6
                TextField {id:candidateTitle;objectName:"widgetCandidateTitle";Layout.fillWidth:true;Accessible.name:qsTr("候选任务标题")}
                FormRow {label:qsTr("开始");DateField {id:candidateStart;Layout.fillWidth:true;workspace:root.workspace}}
                FormRow {label:qsTr("结束");DateField {id:candidateEnd;Layout.fillWidth:true;workspace:root.workspace}}
                FormRow {label:qsTr("开始时刻");TimeField {id:candidateStartTime;Layout.fillWidth:true}}
                FormRow {label:qsTr("结束时刻");TimeField {id:candidateEndTime;Layout.fillWidth:true}}
                FormRow {label:qsTr("截止");DateField {id:candidateDeadline;Layout.fillWidth:true;workspace:root.workspace}}
                TextArea {id:candidateNotes;Layout.fillWidth:true;wrapMode:TextEdit.Wrap;Accessible.name:qsTr("原文与备注")}
                RowLayout {
                    Button {text:qsTr("只保留原文");flat:true;onClicked:{root.replaceCandidate(editor.index,{startOn:"",endOn:"",deadlineOn:"",startTime:"",endTime:"",issue:""});editor.close()}}
                    Item {Layout.fillWidth:true}
                    Button {objectName:"widgetCandidateApply";text:qsTr("应用");onClicked:{root.replaceCandidate(editor.index,{title:candidateTitle.text,notes:candidateNotes.text,startOn:candidateStart.text,endOn:candidateEnd.text,deadlineOn:candidateDeadline.text,startTime:candidateStartTime.text,endTime:candidateEndTime.text,issue:""});editor.close()}}
                }
            }
        }
    }
}
