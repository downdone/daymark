pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id:root
    required property TaskWorkspace workspace
    property Item source:null
    property var original:({})
    property string profile:""
    property bool loading:false
    property bool dirty:false
    property bool saving:false
    property string afterSave:""
    property string localError:""
    property int selectedTab:0
    property bool hadWindowFocus:false
    property var children:[]
    signal fullDetailsRequested(string taskId)
    popupType:Popup.Window
    modal:false;focus:true;padding:14
    width:Math.min(420*Theme.scale,workspace.reminderPopupArea().width-24)
    height:Math.min(body.implicitHeight+padding*2,workspace.reminderPopupArea().height-24,640*Theme.scale)
    closePolicy:Popup.NoAutoClose
    background:PopoverSurface {}
    readonly property bool pickersOpen:timing.modalOpen || start.pickerVisible || end.pickerVisible || reminder.pickerVisible || extra.parentPickerVisible || extra.tagPickerVisible
    Connections {
        target:root.contentItem.Window.window
        function onActiveChanged(){
            if(!root.visible)return
            if(root.contentItem.Window.window.active){root.hadWindowFocus=true;return}
            if(root.hadWindowFocus)Qt.callLater(function(){if(root.visible && !root.pickersOpen && !menu.visible && !root.saving)root.finish("close")})
        }
    }
    function markDirty(){if(!loading)dirty=true}
    function loadTask(id){
        const item=workspace.task(id)
        if(!item.taskId)return false
        loading=true;original=item;title.text=item.title;notes.text=item.notes || ""
        start.text=item.startOn || "";end.text=item.deadlineOn || ""
        reminder.text=workspace.localReminder(item.remindAtUtc);reminder.repeatFrequency=item.reminderRepeat || "NONE";reminder.repeatInterval=item.reminderEvery || 1
        timing.loadTask(item);extra.loadTask(item)
        selectedTab=0;children=workspace.subtaskChoices(id);dirty=false;loading=false;localError=""
        return true
    }
    function openFor(id,item){
        if(visible){if(dirty){localError=qsTr("请先完成当前编辑，或从更多菜单放弃更改。");return}close()}
        if(!loadTask(id))return
        source=item;profile=workspace.widgetProfileToken
        hadWindowFocus=false
        const at=item.mapToItem(parent,0,0),global=item.mapToGlobal(item.width,0),area=workspace.reminderPopupArea()
        x=global.x+width+8<area.x+area.width?at.x+item.width+8:at.x-width-8;y=at.y
        open();Qt.callLater(function(){title.forceActiveFocus()})
    }
    function finish(action){
        if(saving || workspace.busy)return
        if(profile!==workspace.widgetProfileToken){localError=qsTr("资料已切换，请复制未保存内容后关闭。");return}
        afterSave=action
        if(!dirty){performAction();return}
        const patch=Object.assign({},extra.detailsPatch(),timing.detailsPatch(),{remindAtLocal:reminder.text,reminderRepeat:reminder.repeatFrequency,reminderEvery:reminder.repeatInterval,reminderZone:original.reminderZone || workspace.systemTimeZone()})
        saving=workspace.saveTaskWithDetails(original.taskId,original.rowVersion,title.text,notes.text,original.planningState || "ACTIVE",start.text,end.text,!!original.todayFocus,patch)
        if(!saving)localError=workspace.errorText || qsTr("未能保存，请重试。")
    }
    function performAction(){
        const action=afterSave;afterSave=""
        if(action==="full"){const id=original.taskId;close();fullDetailsRequested(id)}
        else if(action==="complete"){if(workspace.changeState(original.taskId,original.rowVersion,"complete"))close();else localError=workspace.errorText}
        else if(action.startsWith("child:"))loadTask(action.slice(6))
        else if(action==="children"){selectedTab=1;children=workspace.subtaskChoices(original.taskId)}
        else close()
    }
    onClosed:{timing.closePickers();start.closePicker();end.closePicker();reminder.closePicker();if(source && source.visible)source.forceActiveFocus()}
    Shortcut {sequence:"Escape";enabled:root.visible && !root.pickersOpen;onActivated:root.finish("close")}
    Connections {
        target:root.workspace
        function onOperationFinished(operation,success){
            if(operation==="save" && root.saving){root.saving=false;if(success){root.dirty=false;root.original=root.workspace.task(root.original.taskId);root.performAction()}else root.localError=root.workspace.errorText}
            else if(operation==="createSubtask" && root.visible){if(success){childTitle.clear();root.children=root.workspace.subtaskChoices(root.original.taskId)}else root.localError=root.workspace.errorText}
        }
    }
    contentItem:ColumnLayout {
        id:body;spacing:8
        RowLayout {
            Layout.fillWidth:true
            Label {Layout.fillWidth:true;text:root.original.parentTaskId?qsTr("子任务"):qsTr("任务");font.pixelSize:Theme.sectionSize;color:Theme.secondary}
            BusyIndicator {running:root.saving;visible:running;Layout.preferredWidth:24;Layout.preferredHeight:24}
            IconToolButton {id:more;Accessible.name:qsTr("任务更多操作");contentItem:AppIcon{name:"more"} onClicked:menu.popupAt(more)}
            IconToolButton {objectName:"widgetInspectorClose";Accessible.name:qsTr("保存并关闭");enabled:!root.saving;contentItem:AppIcon{name:"close"} onClicked:root.finish("close")}
        }
        InspectorTabs {
            Layout.fillWidth:true;visible:!root.original.parentTaskId || root.children.length>0
            model:[qsTr("详情"),qsTr("子任务")]
            currentIndex:root.selectedTab
            onActivated:function(index){if(index===1)root.finish("children");else root.selectedTab=0}
        }
        Label {Layout.fillWidth:true;visible:!!root.localError;text:root.localError;color:Theme.danger;wrapMode:Text.Wrap}
        DS.ScrollView {
            Layout.fillWidth:true;Layout.fillHeight:true
            implicitHeight:root.selectedTab===0?fields.implicitHeight:childFields.implicitHeight
            contentWidth:availableWidth
            ColumnLayout {
                id:fields;visible:root.selectedTab===0;width:parent.width;spacing:8
                TextField {id:title;objectName:"widgetInspectorTitle";Layout.fillWidth:true;padding:0;font.pixelSize:Theme.listTitleSize;background:Item{} Accessible.name:qsTr("任务标题");onTextEdited:root.markDirty();onAccepted:root.finish("close")}
                TextArea {id:notes;Layout.fillWidth:true;padding:0;implicitHeight:Math.max(28,Math.min(100*Theme.scale,contentHeight));wrapMode:TextEdit.Wrap;placeholderText:qsTr("添加备注…");Accessible.name:qsTr("备注");background:Item{} onTextChanged:root.markDirty()}
                Rectangle {Layout.fillWidth:true;implicitHeight:1;color:Theme.detailDivider}
                FormRow {label:qsTr("开始");contextual:true;DateField {id:start;workspace:root.workspace;Layout.fillWidth:true;onTextEdited:{timing.datesEdited=true;root.markDirty()} onDateChosen:{timing.datesEdited=true;root.markDirty()}}}
                FormRow {label:qsTr("结束");contextual:true;DateField {id:end;workspace:root.workspace;Layout.fillWidth:true;onTextEdited:{timing.datesEdited=true;root.markDirty()} onDateChosen:{timing.datesEdited=true;root.markDirty()}}}
                FormRow {label:qsTr("提醒");contextual:true;ReminderField {id:reminder;workspace:root.workspace;Layout.fillWidth:true;onTextChanged:root.markDirty();onRepeatFrequencyChanged:root.markDirty();onRepeatIntervalChanged:root.markDirty()}}
                TaskTimingFields {
                    id:timing;workspace:root.workspace;objectPrefix:"widgetInspector";Layout.fillWidth:true
                    taskId:root.original.taskId || "";compactPresentation:true
                    startDateValue:start.text;endDateValue:end.text
                    onDraftChanged:root.markDirty()
                    onDateDefaultsRequested:{if(!start.text)start.text=root.workspace.dateOffset(0);if(!end.text)end.text=start.text}
                    onRangeDatesChosen:function(a,b){start.text=a;end.text=b;root.markDirty()}
                }
                TaskDetailsFields {
                    id:extra;workspace:root.workspace;objectPrefix:"widgetInspector";Layout.fillWidth:true;inspectorMode:true
                    onDraftChanged:root.markDirty()
                    onCreateCatalogRequested:root.localError=qsTr("请在主窗口侧栏新建分类或标签，再返回选择。")
                }
            }
            ColumnLayout {
                id:childFields;visible:root.selectedTab===1;width:parent.width;spacing:4
                TextField {id:childTitle;Layout.fillWidth:true;placeholderText:qsTr("添加子任务，Enter 确认");enabled:!root.workspace.busy;onAccepted:if(text.trim().length && !root.workspace.createSubtask(text,root.original.taskId))root.localError=root.workspace.errorText}
                Repeater {model:root.children;delegate:ItemDelegate {required property var modelData;Layout.fillWidth:true;text:modelData.title;onClicked:root.finish("child:"+modelData.taskId)}}
            }
        }
    }
    DS.Menu {
        id:menu;parent:root.contentItem
        MenuItem {text:qsTr("完成任务");onTriggered:root.finish("complete")}
        MenuItem {text:qsTr("在主窗口展开");onTriggered:root.finish("full")}
        DS.MenuSeparator {}
        MenuItem {text:qsTr("放弃更改并关闭");enabled:!root.saving;onTriggered:{root.dirty=false;root.close()}}
    }
}
