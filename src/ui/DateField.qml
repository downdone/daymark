pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

TextField {
    id: field
    required property TaskWorkspace workspace
    property string dateLabel: qsTr("日期")
    property bool allowClear: true
    property bool inlineMode: false
    property bool inlineExpanded: false
    readonly property bool pickerVisible: shortcuts.visible || (inlineMode ? inlineExpanded : picker.visible)
    property var month: ({title: "", days: []})
    readonly property var hostWindow: Window.window
    property point anchorPosition: Qt.point(0,0)
    property string placement: "below"
    signal dateChosen()
    signal calendarFocusRequested()
    selectByMouse: true
    placeholderText: qsTr("未设置")
    Accessible.name: dateLabel
    rightPadding: calendarButton.width+8
    function openPicker() {
        shortcuts.close()
        month=workspace.datePicker(text,0)
        if(inlineMode){inlineExpanded=true;forceActiveFocus();return}
        anchorPosition=mapToItem(null,0,0)
        const popupWidth=picker.width
        if(anchorPosition.x>=popupWidth+8)placement="left"
        else if(hostWindow && hostWindow.width-anchorPosition.x-width>=popupWidth+8)placement="right"
        else placement=hostWindow && hostWindow.height-anchorPosition.y-height<anchorPosition.y ? "above" : "below"
        picker.open()
        forceActiveFocus()
    }
    function chooseDate(value) {text=value;dateChosen();closePicker();forceActiveFocus()}
    function openShortcuts(){if(inlineMode)inlineExpanded=false;picker.close();shortcuts.open()}
    function closePicker() {picker.close();shortcuts.close();inlineExpanded=false}
    onVisibleChanged: if(!visible)closePicker()
    Keys.onEscapePressed: function(event) {if(pickerVisible)closePicker();else event.accepted=false}
    Keys.onReturnPressed: function(event) {if(pickerVisible)closePicker();else event.accepted=false}
    Keys.onPressed: function(event) {
        if(event.key===Qt.Key_Down && (event.modifiers & Qt.AltModifier)) {openPicker();event.accepted=true}
        else if(event.key===Qt.Key_Down && pickerVisible){if(inlineMode)calendarFocusRequested();else calendar.focusSelectedDay();event.accepted=true}
    }
    TapHandler {onTapped: field.openPicker()}
    IconToolButton {
        id: calendarButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(32,Theme.bodySize+16)
        height: field.height-4
        Accessible.name: qsTr("选择%1").arg(field.dateLabel)
        contentItem: AppIcon {name: "calendar"}
        onClicked: shortcuts.visible ? shortcuts.close() : field.openShortcuts()
        Menu {
            id: shortcuts
            objectName: "dateShortcuts"
            x: parent.width-width;y: parent.height
            MenuItem {visible: field.allowClear;text: qsTr("无日期");checkable: true;checked: !field.text;onTriggered: field.chooseDate("")}
            MenuSeparator {visible: field.allowClear}
            MenuItem {objectName: "dateShortcutToday";text: qsTr("今天");checkable: true;checked: field.text===field.workspace.dateOffset(0);onTriggered: field.chooseDate(field.workspace.dateOffset(0))}
            MenuItem {objectName: "dateShortcutTomorrow";text: qsTr("明天");checkable: true;checked: field.text===field.workspace.dateOffset(1);onTriggered: field.chooseDate(field.workspace.dateOffset(1))}
            MenuItem {text: qsTr("一周后");checkable: true;checked: field.text===field.workspace.dateOffset(7);onTriggered: field.chooseDate(field.workspace.dateOffset(7))}
            MenuSeparator {}
            MenuItem {text: qsTr("自定义日期…");onTriggered: field.openPicker()}
        }
    }
    Popup {
        id: picker
        objectName: "dateFieldPopup"
        x: field.placement==="left" ? -width-8 : field.placement==="right" ? field.width+8 : 0
        y: field.placement==="above" ? -height-4 : field.placement==="below" ? field.height+4 : Math.max(8-field.anchorPosition.y,Math.min(0,(field.hostWindow ? field.hostWindow.height : 720)-field.anchorPosition.y-height-8))
        width: Math.min(Math.max(320,320*Theme.scale),field.hostWindow ? field.hostWindow.width-32 : 640)
        height: Math.min(contentItem.implicitHeight+padding*2,field.hostWindow ? (field.placement==="above" ? field.anchorPosition.y-12 : field.placement==="below" ? field.hostWindow.height-field.anchorPosition.y-field.height-12 : field.hostWindow.height-16) : 600)
        padding: 12
        margins: 8
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {color: Theme.content;border.color: Theme.separator;radius: 16}
        contentItem: ScrollView {
            clip: true
            implicitHeight: calendar.implicitHeight
            CalendarPicker {
                id: calendar
                width: picker.availableWidth
                workspace: field.workspace
                month: field.month
                selectedDate: field.text
                allowClear: field.allowClear
                onDateSelected: function(value){field.chooseDate(value)}
                onDismissRequested: {field.closePicker();field.forceActiveFocus()}
                onMoveMonth: function(offset){field.month=field.workspace.datePicker(field.month.anchor,offset)}
            }
        }
    }
}
