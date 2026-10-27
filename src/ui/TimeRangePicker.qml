pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    required property TaskWorkspace workspace
    property string objectPrefix: ""
    objectName: objectPrefix + "timeRangePicker"
    property Item source: null
    property string taskId: ""
    property string zone: ""
    property bool initializing: false
    property string previewKey: ""
    property var backgroundData: ({days:[],blocks:[],clusters:[],minutes:1440})
    property var selection: ({valid:false})
    readonly property var hostWindow: parent ? parent.Window.window : null
    signal selected(var range)
    parent: Overlay.overlay
    width: Math.min(460 * Theme.scale, parent ? parent.width-24 : 460)
    height: Math.min(620 * Theme.scale, parent ? parent.height-24 : 620)
    padding: Theme.panelPadding
    margins: 8;focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: PopoverSurface {
        attached:!!root.source
        sourcePoint:{if(!root.source || !root.parent)return Qt.point(-1,-1);const at=root.source.mapToItem(root.parent,root.source.width/2,root.source.height/2);return Qt.point(at.x-root.x,at.y-root.y)}
    }
    function openFor(owner, seed) {
        source=owner;taskId=seed.taskId || "";zone=seed.zone || workspace.systemTimeZone();
        initializing=true;
        startDate.text=seed.startDate || workspace.dateOffset(0);
        endDate.text=seed.endDate || startDate.text;
        startTime.text=seed.startTime || "09:00";endTime.text=seed.endTime || "10:00";
        initializing=false;previewKey="";updateSelection();
        timeline.initialMinute=selection.valid ? selection.minute : 480;
        timeline.initialPositionPending=true;
        const flick=fieldsScroll.contentItem as Flickable;if(flick)flick.contentY=0;
        open();Qt.callLater(timeline.positionInitially);
    }
    function refreshBackground(force) {
        const key=[startDate.text,zone,taskId].join("/");
        if(!force && key===previewKey)return;
        previewKey=key;backgroundData=workspace.timeRangePreview(startDate.text,zone,taskId);
    }
    function updateSelection() {
        if(initializing)return;
        selection=workspace.timeRangeDraft({startDate:startDate.text,endDate:endDate.text,startTime:startTime.text,endTime:endTime.text,zone:zone});
        refreshBackground(false);
    }
    function selectMinutes(date, first, last, finish) {
        const next=workspace.timeRangeDraft({startDate:date,startMinute:first,endMinute:last,zone:zone});
        selection=next;
        if(!next.valid)return;
        initializing=true;startDate.text=next.startDate;endDate.text=next.endDate;startTime.text=next.startTime;endTime.text=next.endTime;initializing=false;
        refreshBackground(false);
        if(finish)apply();
    }
    function apply() {updateSelection();if(selection.valid){selected(selection);close()}}
    function revealField(item) {
        if(!visible || !item)return;
        let owner=item;while(owner && owner!==fields)owner=owner.parent;
        if(!owner)return;
        const flick=fieldsScroll.contentItem as Flickable;if(!flick)return;
        const y=item.mapToItem(fields,0,0).y;
        if(y<flick.contentY)flick.contentY=Math.max(0,y-4);
        else if(y+item.height>flick.contentY+flick.height)flick.contentY=Math.min(Math.max(0,fields.height-flick.height),y+item.height-flick.height+4);
    }
    onAboutToShow: {
        const at=source ? source.mapToItem(parent,0,0) : Qt.point(parent.width/2,parent.height/2);
        x=Math.max(8,Math.min(at.x,parent.width-width-8));
        y=source && parent.height-at.y-source.height>=height+8 ? at.y+source.height+4 : at.y>=height+8 ? at.y-height-4 : Math.max(8,(parent.height-height)/2);
    }
    onClosed: {
        startDate.closePicker();endDate.closePicker();timeline.cancelPreview();
        if(source && source.visible)source.forceActiveFocus();
    }
    Shortcut {sequence: "Escape";enabled:root.visible && !startDate.pickerVisible && !endDate.pickerVisible && !timeline.dragging;onActivated:root.close()}
    Shortcut {sequence: "Return";enabled:root.visible && !startDate.pickerVisible && !endDate.pickerVisible && !timeline.dragging;onActivated:root.apply()}
    Connections {target:root.workspace;function onStateChanged(){if(root.visible)root.refreshBackground(true)}}
    Connections {target:root.hostWindow;function onActiveFocusItemChanged(){Qt.callLater(function(){if(root.hostWindow)root.revealField(root.hostWindow.activeFocusItem)})}}
    contentItem: ColumnLayout {
        objectName:"popoverFieldGroup"
        spacing: Theme.fieldGap
        RowLayout {
            Layout.fillWidth:true
            Label {text:qsTr("选择时段");font.weight:Font.DemiBold;Layout.fillWidth:true}
            WidgetAction {symbol:"close";description:qsTr("取消选择时段");onClicked:root.close()}
        }
        ScrollView {
            id:fieldsScroll;Layout.fillWidth:true
            Layout.preferredHeight: Math.min(fields.implicitHeight,root.availableHeight*0.38)
            contentWidth:availableWidth;clip:true
            ColumnLayout {
                id:fields;width:fieldsScroll.availableWidth;spacing:6
                FormRow {
                    label:qsTr("开始");labelWidth:48*Theme.scale;minimumValueWidth:260*Theme.scale
                    GridLayout {
                        Layout.fillWidth:true;columns:width>=260*Theme.scale ? 2 : 1;columnSpacing:6
                        DateField {id:startDate;objectName:root.objectPrefix+"rangeStartDate";workspace:root.workspace;Layout.fillWidth:true;dateLabel:qsTr("时段开始日期");onTextEdited:root.updateSelection();onDateChosen:root.updateSelection()}
                        TextField {id:startTime;objectName:root.objectPrefix+"rangeStartTime";Layout.fillWidth:true;Layout.preferredWidth:96*Theme.scale;placeholderText:"HH:mm";Accessible.name:qsTr("时段开始时间，小时和分钟");onTextEdited:root.updateSelection();onAccepted:root.apply()}
                    }
                }
                FormRow {
                    label:qsTr("结束");labelWidth:48*Theme.scale;minimumValueWidth:260*Theme.scale
                    GridLayout {
                        Layout.fillWidth:true;columns:width>=260*Theme.scale ? 2 : 1;columnSpacing:6
                        DateField {id:endDate;objectName:root.objectPrefix+"rangeEndDate";workspace:root.workspace;Layout.fillWidth:true;dateLabel:qsTr("时段结束日期");onTextEdited:root.updateSelection();onDateChosen:root.updateSelection()}
                        TextField {id:endTime;objectName:root.objectPrefix+"rangeEndTime";Layout.fillWidth:true;Layout.preferredWidth:96*Theme.scale;placeholderText:"HH:mm";Accessible.name:qsTr("时段结束时间，小时和分钟");onTextEdited:root.updateSelection();onAccepted:root.apply()}
                    }
                }
            }
        }
        Label {Layout.fillWidth:true;text:qsTr("拖选后直接使用，或输入精确时间。");wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary}
        CalendarTimeline {
            id:timeline;objectPrefix:root.objectPrefix+"range";workspace:root.workspace
            Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumHeight:100
            rangeSelectionMode:true;timeline:root.backgroundData
            creationSelection:root.selection.valid ? {date:root.selection.startDate,minute:root.selection.minute,duration:Math.min(root.selection.duration,(root.backgroundData.minutes || 1440)-root.selection.minute)} : ({})
            creationLabel:root.selection.valid ? root.selection.startTime+" — "+root.selection.endTime : ""
            onSlotSelected:function(date,minute,at){root.selectMinutes(date,minute,Math.min(minute+60,root.backgroundData.minutes || 1440),false)}
            onCreateRequested:function(date,minute,x,y,endMinute){root.selectMinutes(date,minute,endMinute,true)}
        }
        Label {Layout.fillWidth:true;visible:!root.selection.valid;text:root.selection.error || "";wrapMode:Text.Wrap;color:Theme.danger;font.pixelSize:Theme.captionSize}
        RowLayout {
            Layout.fillWidth:true
            Label {Layout.fillWidth:true;text:root.zone;font.pixelSize:Theme.captionSize;color:Theme.secondary;elide:Text.ElideRight}
            Button {text:qsTr("取消");onClicked:root.close()}
            Button {objectName:root.objectPrefix+"applyTimeRange";text:qsTr("完成");highlighted:true;enabled:root.selection.valid;onClicked:root.apply()}
        }
    }
}
