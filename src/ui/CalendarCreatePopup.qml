pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS

// A not-yet-persisted draft. Existing task field components own input semantics;
// TaskService commits every attribute in a single transaction.
Popup {
    id: root
    objectName: "calendarCreatePopup"
    required property TaskWorkspace workspace
    property string date: ""
    property int minute: -1
    property int duration: 60
    property bool globalScope: false
    property point anchor: Qt.point(-1, -1)
    property bool creating: false
    property point manualPosition: Qt.point(-1,-1)
    property bool validDraft: false
    property string reviewedTitle: ""
    readonly property var inputTime: workspace.previewTaskInput(title.text)
    readonly property bool dateConflict: title.text.length>0 && !title.inputMethodComposing &&
        inputTime.state==="ready" && inputTime.date!==startField.text && reviewedTitle!==title.text
    readonly property var hostWindow: contentItem ? contentItem.Window.window : null
    readonly property var startField: temporal.startField
    readonly property var endField: temporal.endField
    readonly property var timing: temporal.timingFields
    readonly property string notes: notesField.text
    readonly property string startOn: startField.text
    readonly property string endOn: endField.text
    readonly property var timelineSelection: timing.hasSpecificTime ? workspace.timeRangeDraft({startDate:startField.text,endDate:endField.text,startTime:timing.startTimeValue,endTime:timing.endTimeValue,zone:timing.timeZoneValue}) : ({valid:false})
    signal submitted(string title)
    function detailsPatch() {
        return Object.assign(assignment.detailsPatch(),temporal.detailsPatch(workspace.systemTimeZone()));
    }
    function finish() {
        if(creating || draftCatalog.visible || timing.rangePickerVisible)return false;
        if(!title.text.trim()){close();return false;}
        if(dateConflict){title.forceActiveFocus();return false;}
        submitted(title.text);
        return creating;
    }
    function discard() {if(!creating)close();}
    function reveal(item) {
        if(!visible || picker.active || !item)return;
        let owner=item;while(owner && owner!==form)owner=owner.parent;
        if(!owner)return;
        const cursor=item.cursorRectangle;
        const point=item.mapToItem(form,0,item.height>scroller.height && cursor ? cursor.y : 0);
        const extent=item.height>scroller.height && cursor ? cursor.height : item.height;
        if(point.y<scroller.contentY)scroller.contentY=Math.max(0,point.y-4);
        else if(point.y+extent>scroller.contentY+scroller.height)scroller.contentY=Math.min(Math.max(0,form.height-scroller.height),point.y+extent-scroller.height+4);
    }
    Connections {
        target:root.hostWindow
        function onActiveFocusItemChanged(){Qt.callLater(function(){if(root.hostWindow)root.reveal(root.hostWindow.activeFocusItem)})}
    }
    x: manualPosition.x>=0 ? Math.max(16,Math.min(manualPosition.x,parent.width-width-16)) : anchor.x >= 0 ? Math.max(16,Math.min(anchor.x+width+30<parent.width ? anchor.x+14 : anchor.x-width-14,parent.width-width-16)) : Math.max(16, (parent.width - width) / 2)
    y: manualPosition.y>=0 ? Math.max(16,Math.min(manualPosition.y,parent.height-height-16)) : anchor.y >= 0 ? Math.max(16, Math.min(anchor.y-28, parent.height-height-16)) : Math.max(16, (parent.height-height)/3)
    width: Math.min(408 * Theme.scale, parent.width - 32)
    height: Math.min(parent.height-32,Math.max(240,form.implicitHeight+createHeader.implicitHeight+topPadding+bottomPadding+18+(createError.visible ? createError.implicitHeight+8 : 0)))
    padding: 16
    focus: true
    modal: false
    dim: true
    closePolicy: Popup.NoAutoClose
    Overlay.modeless: Rectangle {
        color: "transparent"
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton | Qt.RightButton;onClicked:root.finish()}
    }
    Shortcut {
        sequence: "Escape";enabled:root.visible && !root.creating && !assignment.tagPickerVisible && !draftCatalog.visible && (!temporal.pickerOpen || picker.active)
        onActivated:picker.active ? picker.close() : root.finish()
    }
    onOpened: {
        manualPosition=Qt.point(-1,-1);
        const item=workspace.calendarDraft(date,minute,duration,globalScope);
        validDraft=item.startOn!==undefined;
        title.clear();reviewedTitle="";notesField.clear();startField.text=item.startOn || date;endField.text=item.deadlineOn || "";
        temporal.reminderField.loadTask({});temporal.datesEdited=false;
        assignment.loadTask(Object.assign({tagIds:[],taskId:""},item));timing.loadTask(item);
        // A selected hour is an explicit draft schedule, including if the user
        // adds a title without touching the prefilled endpoint controls.
        timing.timeDirty=!!item.schedule;
        temporal.startEditing();picker.close();scroller.contentY=0;title.forceActiveFocus();
    }
    onClosed: {picker.close();temporal.finish(false);temporal.closePickers();}
    CatalogCreateDialog {
        id: draftCatalog;objectName: "calendarCreateCatalogDialog";objectPrefix: "calendarDraft"
        parent: Overlay.overlay;anchors.centerIn: parent;workspace: root.workspace
        onCreated: function(kind,id) {assignment.applyCreated(kind,id)}
    }
    background: PopoverSurface {attached:root.manualPosition.x<0 && root.anchor.x>=0;sourcePoint:Qt.point(root.anchor.x-root.x,root.anchor.y-root.y)}
    contentItem: ColumnLayout {
        objectName:"popoverFieldGroup"
        spacing: 6
        RowLayout {
            id:createHeader
            Layout.fillWidth:true
            Label {
                objectName: "calendarCreateDragHandle"
                text:qsTr("新任务");color:Theme.secondary;font.pixelSize:Theme.captionSize;font.weight:Font.Normal;Layout.fillWidth:true;Layout.preferredHeight:Theme.controlHeight;verticalAlignment:Text.AlignVCenter
                MouseArea {
                    anchors.fill:parent;cursorShape:Qt.SizeAllCursor;preventStealing:true
                    property point origin
                    property point popupOrigin
                    onPressed:function(mouse){origin=mapToItem(root.parent,mouse.x,mouse.y);popupOrigin=Qt.point(root.x,root.y)}
                    onPositionChanged:function(mouse){if(pressed){const at=mapToItem(root.parent,mouse.x,mouse.y);root.manualPosition=Qt.point(popupOrigin.x+at.x-origin.x,popupOrigin.y+at.y-origin.y)}}
                }
            }
            Button {text:qsTr("取消");flat:true;enabled:!root.creating;onClicked:root.discard()}
            Button {objectName:"calendarCreateSubmit";text:root.creating ? qsTr("正在添加…") : qsTr("添加");highlighted:true;enabled:root.validDraft && !!title.text.trim() && !root.creating && !root.dateConflict;onClicked:root.finish()}
        }
        Label {id:createError;visible:!!root.workspace.errorText;Layout.fillWidth:true;wrapMode:Text.Wrap;text:root.workspace.errorText;color:Theme.danger;font.pixelSize:Theme.captionSize}
        TaskInspectorPicker {id:picker;objectPrefix:"calendarCreate";workspace:root.workspace;visible:active;Layout.fillWidth:true;Layout.fillHeight:true}
        Item {id:createViewport;visible:!picker.active;Layout.fillWidth:true;Layout.fillHeight:true
        Flickable {
            id:scroller
            DS.ScrollPhysics {target:scroller}
            objectName:"calendarCreateScroll"
            anchors.fill:parent;anchors.rightMargin:Theme.scrollGutter
            contentWidth:width;contentHeight:form.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar{parent:createViewport;x:createViewport.width-width;y:scroller.y;height:scroller.height}
            ColumnLayout {
                id:form;width:scroller.width;spacing:4;enabled:!root.creating
                TextField {
                    id:title;objectName:"calendarCreateTitle";Layout.fillWidth:true
                    placeholderText:qsTr("任务标题");Accessible.name:qsTr("新任务标题");font.pixelSize:Theme.taskTitleSize;font.weight:Font.DemiBold
                    leftPadding:0;rightPadding:0
                    background:Rectangle{color:"transparent";radius:Theme.controlRadius;border.width:title.activeFocus && Theme.highContrast ? 1 : 0;border.color:Theme.accent}
                    onTextEdited:root.reviewedTitle=""
                    onAccepted:if(!inputMethodComposing && text.trim() && root.validDraft)root.finish()
                }
                ColumnLayout {
                    visible:root.dateConflict
                    Layout.fillWidth:true
                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("标题中的日期是 %1，当前安排为 %2。请选择使用哪一天。").arg(root.inputTime.date).arg(root.startOn)}
                    Flow {
                        Layout.fillWidth:true;spacing:4
                        Button {
                            objectName:"calendarKeepDate"
                            text:qsTr("保留 %1").arg(root.startOn)
                            onClicked:root.reviewedTitle=title.text
                        }
                        Button {
                            objectName:"calendarUseTitleDate"
                            text:qsTr("使用日期 %1").arg(root.inputTime.date)
                            onClicked:{
                                const selected=root.inputTime.date
                                root.startField.text=selected
                                root.endField.text=selected
                                temporal.datesEdited=true
                                root.reviewedTitle=title.text
                            }
                        }
                    }
                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("时段和提醒以以下安排为准。");font.pixelSize:Theme.captionSize;color:Theme.secondary}
                }
                TextArea {
                    id:notesField;objectName:"calendarCreateNotes";Layout.fillWidth:true
                    placeholderText:qsTr("添加备注…");Accessible.name:qsTr("新任务备注");wrapMode:TextEdit.Wrap
                    implicitHeight:Math.max(24,contentHeight+topPadding+bottomPadding)
                    leftPadding:0;rightPadding:0
                    onCursorRectangleChanged:if(activeFocus)Qt.callLater(function(){root.reveal(notesField)})
                    background:Rectangle{color:"transparent"}
                }
                DetailSection {
                    plain:true;heading:""
                    TaskTemporalFields {
                        id:temporal;workspace:root.workspace;objectPrefix:"calendarCreate";showActions:false;inlinePickers:true
                        onDatesChanged:root.reviewedTitle=title.text
                        startName:"calendarCreateStart";endName:"calendarCreateEnd";
                        onPickerRequested:function(field,kind){picker.openFor(field,kind)}
                    }
                }
                TaskDetailsFields {id:assignment;objectPrefix:"calendarCreate";objectName:"calendarCreateAssignment";workspace:root.workspace;Layout.fillWidth:true;inspectorMode:true;creationMode:true;onCreateCatalogRequested:function(kind){draftCatalog.showFor(kind)}}
            }
        }
    }
    }
}
