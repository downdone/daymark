pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
    readonly property var hostWindow: contentItem ? contentItem.Window.window : null
    readonly property string notes: notesField.text
    readonly property string startOn: startField.text
    readonly property string endOn: endField.text
    signal submitted(string title)
    function detailsPatch() {
        const patch=Object.assign(assignment.detailsPatch(),timing.detailsPatch());
        patch.remindAtLocal=reminder.text;
        patch.reminderRepeat=reminder.repeatFrequency;
        patch.reminderEvery=reminder.repeatInterval;
        patch.reminderZone=workspace.systemTimeZone();
        return patch;
    }
    function finish() {
        if(creating)return false;
        if(!title.text.trim()){close();return false;}
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
    x: manualPosition.x>=0 ? Math.max(8,Math.min(manualPosition.x,parent.width-width-8)) : anchor.x >= 0 ? Math.max(8, Math.min(anchor.x, parent.width - width - 8)) : Math.max(12, (parent.width - width) / 2)
    y: manualPosition.y>=0 ? Math.max(8,Math.min(manualPosition.y,parent.height-height-8)) : anchor.y >= 0 ? Math.max(8, Math.min(anchor.y, parent.height - height - 8)) : Math.max(12, (parent.height - height) / 3)
    width: Math.min(400 * Theme.scale, parent.width - 24)
    height: Math.min(parent.height-24,Math.max(320,Math.min(540*Theme.scale,form.implicitHeight+96)))
    padding: Theme.panelPadding
    focus: true
    modal: false
    dim: true
    closePolicy: Popup.NoAutoClose
    Overlay.modeless: Rectangle {
        color: "transparent"
        MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton | Qt.RightButton;onClicked:root.finish()}
    }
    Shortcut {
        sequence: "Escape";enabled:root.visible && !root.creating
        onActivated:picker.active ? picker.close() : root.finish()
    }
    onOpened: {
        manualPosition=Qt.point(-1,-1);
        const item=workspace.calendarDraft(date,minute,duration,globalScope);
        validDraft=item.startOn!==undefined;
        title.clear();notesField.clear();startField.text=item.startOn || date;endField.text=item.deadlineOn || "";
        reminder.clear();reminder.repeatFrequency="NONE";reminder.repeatInterval=1;
        assignment.loadTask(Object.assign({tagIds:[],taskId:""},item));timing.loadTask(item);
        // A selected hour is an explicit draft schedule, including if the user
        // adds a title without touching the prefilled endpoint controls.
        timing.timeDirty=!!item.schedule;
        picker.close();scroller.contentY=0;title.forceActiveFocus();
    }
    onClosed: {picker.close();timing.closePickers();startField.closePicker();endField.closePicker();reminder.closePicker();}
    background: Rectangle {radius:Theme.panelRadius;color:Theme.grouped;border.color:Theme.separator}
    contentItem: ColumnLayout {
        spacing: 6
        RowLayout {
            Layout.fillWidth:true
            Label {
                objectName: "calendarCreateDragHandle"
                text:qsTr("新任务");font.pixelSize:Theme.bodySize;font.weight:Font.DemiBold;Layout.fillWidth:true;Layout.preferredHeight:Theme.controlHeight;verticalAlignment:Text.AlignVCenter
                MouseArea {
                    anchors.fill:parent;cursorShape:Qt.SizeAllCursor;preventStealing:true
                    property point origin
                    property point popupOrigin
                    onPressed:function(mouse){origin=mapToItem(root.parent,mouse.x,mouse.y);popupOrigin=Qt.point(root.x,root.y)}
                    onPositionChanged:function(mouse){if(pressed){const at=mapToItem(root.parent,mouse.x,mouse.y);root.manualPosition=Qt.point(popupOrigin.x+at.x-origin.x,popupOrigin.y+at.y-origin.y)}}
                }
            }
            Button {text:qsTr("取消");enabled:!root.creating;onClicked:root.discard()}
            Button {objectName:"calendarCreateSubmit";text:root.creating ? qsTr("正在添加…") : qsTr("添加");highlighted:true;enabled:root.validDraft && !!title.text.trim() && !root.creating;onClicked:{root.submitted(title.text)}}
        }
        Label {visible:!!root.workspace.errorText;Layout.fillWidth:true;wrapMode:Text.Wrap;text:root.workspace.errorText;color:Theme.danger;font.pixelSize:Theme.captionSize}
        TaskInspectorPicker {id:picker;objectPrefix:"calendarCreate";workspace:root.workspace;visible:active;Layout.fillWidth:true;Layout.fillHeight:true}
        Flickable {
            id:scroller
            objectName:"calendarCreateScroll"
            visible:!picker.active;Layout.fillWidth:true;Layout.fillHeight:true
            contentWidth:width;contentHeight:form.implicitHeight;clip:true;boundsBehavior:Flickable.StopAtBounds
            ScrollBar.vertical:ScrollBar{}
            ColumnLayout {
                id:form;width:scroller.width;spacing:Theme.groupGap;enabled:!root.creating
                TextField {
                    id:title;objectName:"calendarCreateTitle";Layout.fillWidth:true
                    placeholderText:qsTr("任务标题");Accessible.name:qsTr("新任务标题");font.pixelSize:Theme.taskTitleSize;font.weight:Font.DemiBold
                    background:Rectangle{color:"transparent"}
                    onAccepted:if(text.trim() && root.validDraft){root.submitted(text)}
                }
                TextArea {
                    id:notesField;objectName:"calendarCreateNotes";Layout.fillWidth:true
                    placeholderText:qsTr("添加备注…");Accessible.name:qsTr("新任务备注");wrapMode:TextEdit.Wrap
                    implicitHeight:Math.max(24,contentHeight+topPadding+bottomPadding)
                    onCursorRectangleChanged:if(activeFocus)Qt.callLater(function(){root.reveal(notesField)})
                    background:Rectangle{color:"transparent"}
                }
                DetailSection {
                    heading:qsTr("日期与时间")
                    ColumnLayout {
                        Layout.fillWidth:true;spacing:Theme.fieldGap
                        FormRow {
                            label:qsTr("开始");minimumValueWidth:(timing.hasSpecificTime ? 280 : 180)*Theme.scale
                            GridLayout {
                                id:startEndpoint;Layout.fillWidth:true;columns:width>=280*Theme.scale ? 2 : 1;columnSpacing:8;rowSpacing:4
                                DateField {id:startField;objectName:"calendarCreateStart";workspace:root.workspace;Layout.fillWidth:true;inlineMode:true;onInlinePickerRequested:picker.openFor(startField,"date");onTextEdited:timing.changed(true);onDateChosen:timing.changed(true)}
                            }
                        }
                        FormRow {
                            label:qsTr("结束");minimumValueWidth:(timing.hasSpecificTime ? 280 : 180)*Theme.scale
                            GridLayout {
                                id:endEndpoint;Layout.fillWidth:true;columns:width>=280*Theme.scale ? 2 : 1;columnSpacing:8;rowSpacing:4
                                DateField {id:endField;objectName:"calendarCreateEnd";workspace:root.workspace;Layout.fillWidth:true;inlineMode:true;onInlinePickerRequested:picker.openFor(endField,"date");onTextEdited:timing.changed(true);onDateChosen:timing.changed(true)}
                            }
                        }
                    }
                    TaskTimingFields {
                        id:timing;compactPresentation:true;objectPrefix:"calendarCreate";objectName:"calendarCreateTiming";workspace:root.workspace;Layout.fillWidth:true
                        inlinePickers:true;startEndpointHost:startEndpoint;endEndpointHost:endEndpoint
                        startDateValue:startField.text;endDateValue:endField.text
                        onPickerRequested:function(field,kind){picker.openFor(field,kind)}
                        onDateDefaultsRequested:{if(!startField.text)startField.text=root.date;if(!endField.text)endField.text=startField.text}
                    }
                    FormRow {label:qsTr("提醒")
                    ReminderField {id:reminder;objectPrefix:"calendarCreate";objectName:"calendarCreateReminder";workspace:root.workspace;Layout.fillWidth:true;inlineMode:true;placeholderText:qsTr("添加提醒…");onInlinePickerRequested:picker.openFor(reminder,"reminder")}
                    }
                }
                TaskDetailsFields {id:assignment;objectPrefix:"calendarCreate";objectName:"calendarCreateAssignment";workspace:root.workspace;Layout.fillWidth:true;inspectorMode:true;creationMode:true}
            }
        }
    }
}
