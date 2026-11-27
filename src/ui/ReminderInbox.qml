pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id:root
    required property TaskWorkspace workspace
    property Item source:null
    property string localError:""
    property string feedback:""
    property bool completing:false
    property var alerts:[]
    property int revision:0
    signal openTask(string taskId)
    signal historyRequested()
    popupType:Popup.Window
    modal:false;focus:true;padding:12
    property rect workArea:Qt.rect(0,0,800,600)
    function reposition(){
        if(!source || !parent)return
        const point=DS.ScreenWorkArea.beside(source,parent,width,height)
        x=point.x;y=point.y
    }
    onHeightChanged:if(visible)reposition()
    width:Math.min(380*Theme.scale,workArea.width-12)
    height:Math.min(body.implicitHeight+padding*2,workArea.height-12,560*Theme.scale)
    closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
    background:PopoverSurface {}
    function openAt(item){
        workspace.prepareReminderInbox()
        source=item;localError="";feedback=""
        workArea=DS.ScreenWorkArea.forItem(item)
        reposition()
        open()
        refreshAlerts()
    }
    function refreshAlerts(){
        const row=list.indexAt(1,list.contentY+1),first=row>=0?list.itemAtIndex(row):null
        const key=row>=0 && row<alerts.length?alerts[row].key:"",offset=first?list.contentY-first.y:0,oldY=list.contentY
        const generation=++revision
        alerts=workspace.reminderAlerts
        Qt.callLater(function(){
            if(generation!==root.revision)return
            list.forceLayout()
            const index=root.alerts.findIndex(item=>item.key===key)
            if(index>=0){list.positionViewAtIndex(index,ListView.Beginning);const item=list.itemAtIndex(index);if(item)list.contentY=item.y+offset}
            else list.contentY=oldY
            list.contentY=Math.max(list.originY,Math.min(list.originY+Math.max(0,list.contentHeight-list.height),list.contentY))
        })
    }
    onVisibleChanged:workspace.reminderInboxActive=visible
    onClosed:if(source && source.visible)source.forceActiveFocus()
    function acknowledge(key,delay){
        if(workspace.acknowledgeReminder(key,delay)){localError="";feedback=delay?qsTr("已延后10分钟，任务时间不变"):qsTr("已忽略本次提醒")}
        else localError=workspace.errorText || qsTr("未能保存，请重试。")
    }
    Connections {
        target:root.workspace
        function onReminderInboxOpening(){root.close()}
        function onReminderAlertsChanged(){root.refreshAlerts()}
        function onWidgetBrowseChanged(){if(root.visible && !root.workspace.ready)root.close()}
        function onOperationFinished(operation,success){if(root.completing && operation==="complete"){root.completing=false;if(!success)root.localError=root.workspace.errorText}}
    }
    contentItem:ColumnLayout {
        id:body;spacing:8
        RowLayout {
            Layout.fillWidth:true
            Label {text:qsTr("待处理提醒");font.pixelSize:Theme.panelTitleSize;font.weight:Font.DemiBold}
            Label {text:root.workspace.pendingReminderCount;font.pixelSize:Theme.captionSize;color:Theme.secondary}
            Item {Layout.fillWidth:true}
            IconToolButton {objectName:"reminderInboxClose";Accessible.name:qsTr("关闭提醒列表");contentItem:AppIcon {name:"close"} onClicked:root.close()}
        }
        Label {Layout.fillWidth:true;visible:!!root.localError;text:root.localError;color:Theme.danger;wrapMode:Text.Wrap}
        Item {
            Layout.fillWidth:true
            implicitHeight:Math.min(360*Theme.scale,Math.max(64,list.contentHeight))
            Layout.fillHeight:true;Layout.minimumHeight:48
            ListView {
                id:list;objectName:"reminderInboxList";anchors.fill:parent
                model:root.alerts;clip:true;cacheBuffer:0
                ScrollBar.vertical:ScrollBar {}
                delegate:Control {
                    id:row;required property var modelData
                    width:list.width-Theme.scrollGutter
                    implicitHeight:rowBody.implicitHeight+16;padding:0
                    background:Rectangle {height:1;anchors.top:parent.top;width:parent.width;color:Theme.detailDivider}
                    contentItem:RowLayout {
                        id:rowBody;spacing:4
                        IconToolButton {
                            objectName:"reminderInboxComplete-"+row.modelData.key
                            Layout.alignment:Qt.AlignTop
                            Accessible.name:qsTr("完成%1").arg(row.modelData.title)
                            enabled:!root.workspace.busy && !root.completing
                            contentItem:AppIcon {name:"circle"}
                            onClicked:{root.localError="";root.completing=root.workspace.changeState(row.modelData.taskId,row.modelData.rowVersion,"complete");if(!root.completing)root.localError=root.workspace.errorText}
                        }
                        ColumnLayout {
                            Layout.fillWidth:true;spacing:2
                            Button {id:titleButton;Layout.fillWidth:true;Layout.minimumWidth:0;flat:true;padding:0;text:row.modelData.title;contentItem:Label{text:titleButton.text;textFormat:Text.PlainText;wrapMode:Text.Wrap;font.pixelSize:Theme.listTitleSize} onClicked:root.openTask(row.modelData.taskId)}
                            Label {Layout.fillWidth:true;text:root.workspace.localReminder(row.modelData.occurrenceAt);font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap}
                            RowLayout {
                                Layout.fillWidth:true;spacing:2
                                Button {objectName:"reminderInboxSnooze-"+row.modelData.key;text:qsTr("10分钟后");flat:true;font.pixelSize:Theme.captionSize;onClicked:root.acknowledge(row.modelData.key,10)}
                                Button {text:qsTr("忽略本次");flat:true;font.pixelSize:Theme.captionSize;onClicked:root.acknowledge(row.modelData.key,0)}
                            }
                        }
                    }
                }
            }
            Label {anchors.centerIn:parent;visible:list.count===0;text:qsTr("没有待处理的提醒");color:Theme.secondary}
        }
        RowLayout {
            visible:root.feedback.length>0;Layout.fillWidth:true
            Label {text:root.feedback;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary}
            Button {objectName:"reminderInboxUndo";text:qsTr("撤销");flat:true;visible:root.workspace.canUndoReminder;onClicked:{if(root.workspace.undoReminderAcknowledgement())root.feedback="";else root.localError=root.workspace.errorText}}
        }
        Rectangle {Layout.fillWidth:true;implicitHeight:1;color:Theme.detailDivider}
        RowLayout {
            Label {Layout.fillWidth:true;text:qsTr("最近24小时 · 本机待处理");font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap}
            Button {flat:true;text:qsTr("提醒记录");font.pixelSize:Theme.captionSize;onClicked:{root.close();root.historyRequested()}}
        }
    }
}
