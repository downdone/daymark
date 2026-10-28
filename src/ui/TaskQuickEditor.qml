pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Control {
    id: root
    objectName: "taskQuickEditor"
    required property TaskWorkspace workspace
    property bool dirty: false
    property bool creationMode: false
    property bool guardActive: false
    property string titleText: ""
    property string notesText: ""
    property string startOn: ""
    property string deadlineOn: ""
    property string reminderText: ""
    property string reminderRepeat: "NONE"
    property int reminderEvery: 1
    signal reminderCycleEdited(string frequency,int interval)
    property var destinations: []
    property var tags: []
    property var tagIds: []
    property string destinationId: ""
    readonly property bool multilineNotes: notesText.indexOf("\n")>=0
    readonly property var hostWindow: Window.window
    property bool timeRangeOpen: false
    property string timeSummary: ""
    readonly property bool pickerVisible: timeRangeOpen || quickStart.pickerVisible || quickEnd.pickerVisible || reminder.pickerVisible || tagPicker.visible || destinationPicker.visible
    signal timeRangeRequested(Item source)
    property bool important: false
    signal importantEdited(bool value)
    signal titleEdited(string value)
    signal notesEdited(string value)
    signal startEdited(string value)
    signal deadlineEdited(string value)
    signal reminderEdited(string value)
    signal destinationEdited(string kind,string id)
    signal tagEdited(string id,bool checked)
    signal createCatalogRequested(string kind)
    signal detailsRequested()
    signal closeRequested()
    signal saveRequested()
    signal completeRequested()
    signal focusRequested(Item item)
    property bool editing: false
    visible: editing
    readonly property Item completionItem: quickComplete
    readonly property Item completionInk: quickCompletionMark
    readonly property real defaultTitleTopInset: Math.max(0,(Math.max(32,Theme.iconSize+12)-titleMetrics.height)/2)
    property real titleTopInset: defaultTitleTopInset
    readonly property real titleInkCenter: contentItem.y+quickHeader.y+quickTextStack.y+quickTitle.y+quickTitle.topPadding+(quickTitle.height-quickTitle.topPadding-quickTitle.bottomPadding)/2
    FontMetrics {id:titleMetrics;font:quickTitle.font}
    readonly property Item titleItem: quickTitle
    function inspectorSourcePoint(target) {
        return quickTitle.mapToItem(target,Math.min(quickTitle.width,quickTitle.contentWidth),quickTitle.topPadding+(quickTitle.height-quickTitle.topPadding-quickTitle.bottomPadding)/2)
    }
    readonly property Item notesItem: quickNotes
    readonly property Item toolsItem: quickActions
    readonly property real textInset: quickComplete.width+Theme.listTextGap
    padding: 0
    topPadding: 0;bottomPadding: 4
    implicitHeight: implicitContentHeight+topPadding+bottomPadding
    function open(){editing=true;Qt.callLater(function(){focusTitle();if(root.creationMode)quickTitle.selectAll()})}
    function close(){fullNotes.keepOpen=false;hideNotesTooltip.stop();fullNotes.close();quickStart.closePicker();quickEnd.closePicker();reminder.closePicker();tagPicker.close();destinationPicker.close();editing=false}
    function focusTitle(){quickTitle.forceActiveFocus();focusRequested(quickTitle)}
    function verifyLayout(){return editing && visible && width>0 && height<=170*Theme.scale && quickTitle.font.pixelSize===Theme.listTitleSize && quickNotes.font.pixelSize===Theme.captionSize ? "PASS" : "inline geometry editing="+editing+" width="+width+" height="+height+" implicit="+implicitHeight+" scale="+Theme.scale+" fonts="+quickTitle.font.pixelSize+","+quickNotes.font.pixelSize}
    Connections {target:Theme;function onHintsBlockedChanged(){if(Theme.hintsBlocked){fullNotes.keepOpen=false;hideNotesTooltip.stop();fullNotes.close()}}}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.guardActive && !root.pickerVisible;onActivated: root.closeRequested()}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+Return" : "Ctrl+Return";enabled: root.visible && !root.guardActive && !root.pickerVisible;onActivated: root.saveRequested()}
    contentItem: ColumnLayout {
        spacing: 2
        enabled: !root.workspace.busy || root.workspace.backgroundBusy
        RowLayout {
            id:quickHeader
            Layout.fillWidth: true;spacing: Theme.listTextGap
            IconToolButton {id: quickComplete;Layout.alignment:Qt.AlignTop;enabled:!root.creationMode;hintEnabled: false;objectName: "inlineCompleteTask";Accessible.name: qsTr("完成任务：%1").arg(root.titleText);contentItem: Item {implicitWidth: Theme.iconSize;implicitHeight: Theme.iconSize;Rectangle {id:quickCompletionMark;anchors.horizontalCenter:parent.horizontalCenter;y:root.titleInkCenter-quickComplete.y-quickComplete.topPadding-height/2;width: Math.min(24,18*Theme.scale);height: width;radius: width/2;color: "transparent";border.color: Theme.secondary;border.width: 1.25}} onClicked: root.completeRequested()}
            ColumnLayout {id:quickTextStack;Layout.fillWidth:true;Layout.alignment:Qt.AlignTop;spacing:1
            TextField {
                id: quickTitle;objectName: "quickEditorTitle"
                Layout.fillWidth: true
                font.pixelSize: Theme.listTitleSize;font.weight: Font.Normal
                implicitHeight: contentHeight+topPadding+bottomPadding
                topPadding:root.titleTopInset;bottomPadding:0;verticalAlignment:TextInput.AlignTop
                leftPadding: 0;rightPadding: 0;background: Item {}
                Accessible.name: qsTr("任务标题")
                onTextEdited: root.titleEdited(text)
                onAccepted: root.saveRequested()
                onActiveFocusChanged: if(activeFocus)root.focusRequested(quickTitle)
            }
            Binding {target: quickTitle;property: "text";value: root.titleText}
        TextField {
            id: quickNotes;objectName: "quickEditorNotes"
            Layout.fillWidth: true
            font.pixelSize: Theme.captionSize;color: Theme.secondary
            implicitHeight: Math.max(20,contentHeight)
            padding: 0;verticalAlignment:TextInput.AlignTop;readOnly: root.multilineNotes
            placeholderText: qsTr("添加备注…")
            Accessible.name: root.multilineNotes ? qsTr("多行备注摘要，点击在详情编辑") : qsTr("一行备注")
            background: Item {}
            hoverEnabled: true
            onHoveredChanged: {if(hovered){fullNotes.keepOpen=true;hideNotesTooltip.stop()}else hideNotesTooltip.restart()}
            Timer {id: hideNotesTooltip;interval: 250;onTriggered: if(!quickNotes.hovered && !notesTooltipHover.hovered)fullNotes.keepOpen=false}
            ToolTip {
                id: fullNotes;objectName: "quickNotesTooltip"
                enabled: true // Explicit selectable notes-preview exception.
                property bool keepOpen: false
                visible: !Theme.hintsBlocked && !root.guardActive && !root.pickerVisible && root.visible && (root.multilineNotes || quickNotes.contentWidth>quickNotes.width-quickNotes.leftPadding-quickNotes.rightPadding+1) && (quickNotes.hovered || notesTooltipHover.hovered || keepOpen) && root.notesText.length>0
                delay: Theme.hintDelay;timeout: -1
                width: Math.min(480*Theme.scale,root.hostWindow ? root.hostWindow.width-32 : 480)
                HoverHandler {id: notesTooltipHover;onHoveredChanged: {if(hovered){fullNotes.keepOpen=true;hideNotesTooltip.stop()}else hideNotesTooltip.restart()}}
                contentItem: ScrollView {
                    implicitHeight: Math.min(320*Theme.scale,root.hostWindow ? root.hostWindow.height*0.45 : 320,notesBody.implicitHeight)
                    clip: true;contentWidth: availableWidth
                    TextArea {id: notesBody;objectName: "quickFullNotesText";text: root.notesText;textFormat: TextEdit.PlainText;readOnly: true;selectByMouse: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.text;background: Item {}}
                }
            }
            onTextEdited: root.notesEdited(text)
            onAccepted: root.saveRequested()
            onActiveFocusChanged: if(activeFocus)root.focusRequested(quickNotes)
            TapHandler {enabled: root.multilineNotes;onTapped: root.detailsRequested()}
        }
        Binding {target: quickNotes;property: "text";value: root.multilineNotes ? root.notesText.split("\n")[0]+"…" : root.notesText}
            }
            IconToolButton {id:quickDetails;objectName: "quickEditorDetails";Layout.alignment:Qt.AlignTop;contentItem:Item {implicitWidth:Theme.iconSize;implicitHeight:Theme.iconSize;AppIcon{name:"info";width:Theme.iconSize;height:width;x:(parent.width-width)/2;y:root.titleInkCenter-quickDetails.y-quickDetails.topPadding-height/2}} Accessible.name: qsTr("完整任务详情");onClicked: root.detailsRequested()}
            IconToolButton {objectName: "saveQuickEditor";Layout.alignment:Qt.AlignTop;visible:root.dirty;contentItem: AppIcon {name: "check";ink: Theme.accent} Accessible.name: qsTr("保存并收起，%1+Enter").arg(Qt.platform.os==="osx" ? "⌘" : "Ctrl");onClicked: root.saveRequested()}
        }
        Flow {
            id:quickActions;objectName:"quickEditorActions"
            Layout.fillWidth: true;Layout.leftMargin: root.textInset;spacing: 6
            ActionButton {objectName:"quickTimeRange";symbol:"clock";text:root.timeSummary || qsTr("时段");Accessible.name:qsTr("设置具体时段：%1").arg(text);onClicked:root.timeRangeRequested(this)}
            DateField {
                id: quickStart;objectName: "quickEditorStart"
                workspace: root.workspace;dateLabel: qsTr("开始：%1").arg(text || qsTr("未设置"))
                width: text ? 116*Theme.scale : height;height: Math.max(32,Theme.captionSize+14);implicitWidth: width
                font.pixelSize: Theme.captionSize
                iconOnly:!text;readOnly: true;activeFocusOnTab: false;color: iconOnly ? "transparent" : Theme.secondary;leftPadding: 8;placeholderText: ""
                background: Rectangle {radius: height/2;border.width: quickStart.activeFocus ? 1 : 0;border.color: Theme.accent;color: quickStart.text ? Theme.selected : Theme.hover}
                onDateChosen: root.startEdited(text)
                onActiveFocusChanged: if(activeFocus)root.focusRequested(quickStart)
            }
            Binding {target: quickStart;property: "text";value: root.startOn}
            DateField {
                id: quickEnd;objectName: "quickEditorDeadline"
                workspace: root.workspace;iconOnly: !text;dateLabel: qsTr("结束日期：%1").arg(text || qsTr("未设置"));symbol: "upcoming"
                width: text ? 116*Theme.scale : height;height: Math.max(32,Theme.captionSize+14);implicitWidth: width
                font.pixelSize: Theme.captionSize
                readOnly: true;activeFocusOnTab: false;color: iconOnly ? "transparent" : Theme.secondary;leftPadding: 8;placeholderText: ""
                background: Rectangle {radius: height/2;border.width: quickEnd.activeFocus ? 1 : 0;border.color: Theme.accent;color: quickEnd.text ? Theme.selected : Theme.hover}
                onDateChosen: root.deadlineEdited(text)
                onActiveFocusChanged: if(activeFocus)root.focusRequested(quickEnd)
            }
            Binding {target: quickEnd;property: "text";value: root.deadlineOn}
            IconToolButton {
                id: tagButton
                objectName: "quickTagButton"
                width: Math.max(32,Theme.iconSize+12);height: width
                background: Rectangle {radius: height/2;color: tagButton.down ? Theme.pressed : tagButton.hovered ? Theme.hover : root.tagIds.length>0 ? Theme.selected : Theme.hover;border.width: tagButton.visualFocus ? 1 : 0;border.color: Theme.accent}
                contentItem: AppIcon {name: "tag";ink: root.tagIds.length>0 ? Theme.accent : Theme.secondary}
                Accessible.name: qsTr("选择标签，已选%1个").arg(root.tagIds.length)
                onClicked: tagPicker.popup(tagButton,0,tagButton.height)
                TagMenu {id: tagPicker;objectName:"quickTagMenu";options: root.tags;selectedIds: root.tagIds;onSelected: function(id,checked){root.tagEdited(id,checked)};onCreateRequested: root.createCatalogRequested("tag")}
            }
            IconToolButton {
                id: destinationButton
                width: Math.max(32,Theme.iconSize+12);height: width
                background: Rectangle {radius: height/2;color: destinationButton.down ? Theme.pressed : destinationButton.hovered ? Theme.hover : root.destinationId ? Theme.selected : Theme.hover;border.width: destinationButton.visualFocus ? 1 : 0;border.color: Theme.accent}
                contentItem: AppIcon {name: "folder";ink: root.destinationId ? Theme.accent : Theme.secondary}
                Accessible.name: qsTr("选择分类")
                onClicked: destinationPicker.open()
                CatalogPicker {id: destinationPicker;heading: qsTr("分类");options: root.destinations;selectedIds: [root.destinationId];heightLimit: root.hostWindow ? root.hostWindow.height-32 : 440;y: destinationButton.height+4;onSelected: function(kind,id,checked){root.destinationEdited(kind,id)};onCreateRequested: function(kind){root.createCatalogRequested(kind)}}
            }
            ReminderField {
                id: reminder;objectName: "quickEditorReminder";workspace: root.workspace;iconOnly: true;commitTask: false
                width: Math.max(32,Theme.iconSize+12);height: width;implicitWidth: width
                readOnly: true;activeFocusOnTab: false;color: "transparent";leftPadding: 0;placeholderText: ""
                background: Rectangle {radius: height/2;border.width: reminder.activeFocus ? 1 : 0;border.color: Theme.accent;color: reminder.text ? Theme.selected : Theme.hover}
                onReminderChosen: {root.reminderCycleEdited(repeatFrequency,repeatInterval);root.reminderEdited(text)}
                onActiveFocusChanged: if(activeFocus)root.focusRequested(reminder)
            }
            Binding {target: reminder;property: "text";value: root.reminderText}
            Binding {target: reminder;property: "repeatFrequency";value: root.reminderRepeat}
            Binding {target: reminder;property: "repeatInterval";value: root.reminderEvery}
            IconToolButton {id:importantButton;objectName:"quickImportant";Accessible.name:checked ? qsTr("取消特别关注") : qsTr("特别关注");checkable:true;checked:root.important;background:Rectangle {radius:height/2;color:importantButton.checked ? Theme.selected : importantButton.down ? Theme.pressed : Theme.hover;border.width:importantButton.visualFocus ? 1 : 0;border.color:Theme.accent} contentItem:AppIcon{name:"flag";ink:root.important ? Theme.catalogColor("orange") : Theme.secondary} onClicked:root.importantEdited(checked)}
        }
        Label {Layout.fillWidth: true;Layout.leftMargin: root.textInset;visible: text.length>0;text: root.workspace.errorText;color: Theme.danger;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize}
    }
}
