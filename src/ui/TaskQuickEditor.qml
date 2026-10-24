pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    objectName: "taskQuickEditor"
    required property TaskWorkspace workspace
    property Item anchorItem: null
    property bool dirty: false
    property bool guardActive: false
    property string titleText: ""
    property string notesText: ""
    property string startOn: ""
    property string deadlineOn: ""
    signal titleEdited(string value)
    signal notesEdited(string value)
    signal startEdited(string value)
    signal deadlineEdited(string value)
    signal detailsRequested()
    signal closeRequested()
    signal saveRequested()
    width: Math.min(560*Theme.scale,parent.width-32)
    height: Math.min(contentItem.implicitHeight+padding*2,parent.height-32)
    x: anchorItem ? Math.max(16,Math.min(parent.width-width-16,anchorItem.mapToItem(parent,0,0).x)) : Math.max(16,(parent.width-width)/2)
    y: anchorItem ? Math.max(16,Math.min(parent.height-height-16,anchorItem.mapToItem(parent,0,0).y)) : Math.max(16,Math.min(96,(parent.height-height)/2))
    padding: 16;margins: 16;focus: true;modal: false
    closePolicy: Popup.NoAutoClose
    background: Rectangle {color: Theme.content;radius: 16;border.color: Theme.separator}
    function verifyLayout() {
        const at=footer.mapToItem(root.contentItem,0,0)
        return body.height>=40 && at.y+footer.height<=root.availableHeight+1 && root.width<=root.parent.width-16 ? "PASS" : "quick editor clipped: body="+body.height+", footer="+at.y+"/"+footer.height+", available="+root.availableHeight+", width="+root.width+"/"+root.parent.width
    }
    property bool manualScroll: false
    function reveal(item,cursor) {
        if(manualScroll || !item)return
        const y=item.mapToItem(body.contentItem,0,cursor ? quickNotes.cursorRectangle.y : 0).y
        const bottom=y+(cursor ? quickNotes.cursorRectangle.height : Math.min(item.height,body.height/2))+8
        if(y<body.contentY)body.contentY=y
        else if(bottom>body.contentY+body.height)body.contentY=bottom-body.height
    }
    function focusTitle(){quickTitle.forceActiveFocus()}
    onOpened: {manualScroll=false;body.contentY=0;focusTitle()}
    onClosed: {quickStart.closePicker();quickEnd.closePicker()}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.guardActive && !quickStart.pickerVisible && !quickEnd.pickerVisible;onActivated: root.closeRequested()}
    contentItem: ColumnLayout {
        spacing: 8
        RowLayout {
            id: header
            Layout.fillWidth: true
            Label {text: qsTr("快速编辑");font.weight: Font.DemiBold;Layout.fillWidth: true}
            Label {visible: root.dirty;text: qsTr("未保存");color: Theme.secondary;font.pixelSize: Theme.captionSize}
            IconToolButton {contentItem: AppIcon {name: "close"} Accessible.name: qsTr("关闭快速编辑");enabled: !root.workspace.busy;onClicked: root.closeRequested()}
        }
        Flickable {
            id: body
            DS.ScrollPhysics {target: body}
            objectName: "quickEditorScroll"
            Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: 40
            implicitHeight: fields.implicitHeight
            onMovementStarted: root.manualScroll=true
            WheelHandler {target: null;blocking: false;onWheel: function(event){root.manualScroll=true;event.accepted=false}}
            contentWidth: width;contentHeight: fields.implicitHeight
            clip: true;boundsBehavior: Flickable.StopAtBounds;flickableDirection: Flickable.VerticalFlick
            ScrollBar.vertical: ScrollBar {policy: ScrollBar.AsNeeded}
            ColumnLayout {
                id: fields
                width: body.width-12;spacing: 8
                enabled: !root.workspace.busy || root.workspace.backgroundBusy
                TextField {id: quickTitle;objectName: "quickEditorTitle";Layout.fillWidth: true;font.pixelSize: Theme.taskTitleSize;font.weight: Font.DemiBold;Accessible.name: qsTr("任务标题");onTextEdited: {root.manualScroll=false;root.titleEdited(text)} onActiveFocusChanged: if(activeFocus){root.manualScroll=false;root.reveal(quickTitle,false)}}
                Binding {target: quickTitle;property: "text";value: root.titleText}
                TextArea {id: quickNotes;objectName: "quickEditorNotes";Layout.fillWidth: true;implicitHeight: Math.max(40,contentHeight+topPadding+bottomPadding);wrapMode: TextEdit.Wrap;placeholderText: qsTr("添加备注…");Accessible.name: qsTr("备注");onTextChanged: if(activeFocus){root.manualScroll=false;root.notesEdited(text)} onCursorPositionChanged: {root.manualScroll=false;Qt.callLater(function(){if(quickNotes.activeFocus)root.reveal(quickNotes,true)})} onActiveFocusChanged: if(activeFocus){root.manualScroll=false;root.reveal(quickNotes,true)}}
                Binding {target: quickNotes;property: "text";value: root.notesText}
                GridLayout {
                    Layout.fillWidth: true;columns: fields.width>=440*Theme.scale ? 2 : 1;columnSpacing: 12
                    ColumnLayout {
                        Layout.fillWidth: true;Layout.preferredWidth: 1
                        Label {text: qsTr("计划开始");color: Theme.secondary;font.pixelSize: Theme.captionSize}
                        DateField {id: quickStart;onActiveFocusChanged: if(activeFocus){root.manualScroll=false;root.reveal(quickStart,false)} objectName: "quickEditorStart";workspace: root.workspace;dateLabel: qsTr("计划开始");Layout.fillWidth: true;onTextEdited: root.startEdited(text);onDateChosen: root.startEdited(text)}
                        Binding {target: quickStart;property: "text";value: root.startOn}
                    }
                    ColumnLayout {
                        Layout.fillWidth: true;Layout.preferredWidth: 1
                        Label {text: qsTr("截止日期");color: Theme.secondary;font.pixelSize: Theme.captionSize}
                        DateField {id: quickEnd;onActiveFocusChanged: if(activeFocus){root.manualScroll=false;root.reveal(quickEnd,false)} objectName: "quickEditorDeadline";workspace: root.workspace;dateLabel: qsTr("截止日期");Layout.fillWidth: true;onTextEdited: root.deadlineEdited(text);onDateChosen: root.deadlineEdited(text)}
                        Binding {target: quickEnd;property: "text";value: root.deadlineOn}
                    }
                }
                Label {Layout.fillWidth: true;visible: text.length>0;text: root.workspace.errorText;color: Theme.danger;wrapMode: Text.Wrap}
            }
        }
        Flow {
            id: footer
            Layout.fillWidth: true;spacing: 8
            ActionButton {objectName: "quickEditorDetails";symbol: "expand";text: qsTr("完整详情");enabled: !root.workspace.busy;onClicked: root.detailsRequested()}
            Button {text: qsTr("关闭");enabled: !root.workspace.busy;onClicked: root.closeRequested()}
            Button {objectName: "saveQuickEditor";text: qsTr("保存");highlighted: true;enabled: !root.workspace.busy;onClicked: root.saveRequested()}
        }
    }
}
