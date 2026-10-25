pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

TextField {
    id: field
    required property TaskWorkspace workspace
    property string symbol: "calendar"
    property string dateLabel: qsTr("日期")
    property bool iconOnly: false
    property bool allowClear: true
    property bool inlineMode: false
    property bool inlineExpanded: false
    readonly property bool pickerVisible: inlineMode ? inlineExpanded : picker.visible
    property var month: ({title: "", days: []})
    readonly property var hostWindow: Window.window
    property point anchorPosition: Qt.point(0,0)
    property string placement: "below"
    signal inlinePickerRequested()
    signal dateChosen()
    signal calendarFocusRequested()
    selectByMouse: true
    placeholderText: qsTr("未设置")
    Accessible.name: dateLabel
    rightPadding: calendarButton.width+8
    function openPicker() {
        month=workspace.datePicker(text,0)
        if(inlineMode){inlineExpanded=true;inlinePickerRequested();forceActiveFocus();return}
        anchorPosition=mapToItem(null,0,0)
        const popupWidth=picker.width
        const desiredHeight=Math.min(calendar.implicitHeight+picker.padding*2,(hostWindow ? hostWindow.height : 720)-24)
        if(hostWindow && hostWindow.height-anchorPosition.y-height-12>=desiredHeight)placement="below"
        else if(anchorPosition.y-12>=desiredHeight)placement="above"
        else if(anchorPosition.x>=popupWidth+8)placement="left"
        else if(hostWindow && hostWindow.width-anchorPosition.x-width>=popupWidth+8)placement="right"
        else placement=hostWindow && hostWindow.height-anchorPosition.y-height<anchorPosition.y ? "above" : "below"
        picker.open()
        forceActiveFocus()
    }
    function chooseDate(value) {text=value;dateChosen();closePicker();forceActiveFocus()}
    function openShortcuts(){openPicker()}
    function closePicker() {picker.close();inlineExpanded=false}
    onVisibleChanged: if(!visible && !inlineExpanded)closePicker()
    Keys.onEscapePressed: function(event) {if(pickerVisible)closePicker();else event.accepted=false}
    Keys.onReturnPressed: function(event) {if(pickerVisible)closePicker();else event.accepted=false}
    Keys.onPressed: function(event) {
        if(event.key===Qt.Key_Down && (event.modifiers & Qt.AltModifier)) {openPicker();event.accepted=true}
        else if(event.key===Qt.Key_Down && pickerVisible){if(inlineMode)calendarFocusRequested();else calendar.focusSelectedDay();event.accepted=true}
    }
    TapHandler {enabled:field.readOnly || field.iconOnly;onTapped: field.openPicker()}
    IconToolButton {
        id: calendarButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: field.iconOnly ? field.width : Math.max(32,Theme.bodySize+16)
        height: field.iconOnly ? field.height : field.height-4
        Accessible.name: qsTr("选择%1").arg(field.dateLabel)
        contentItem: AppIcon {name: field.symbol}
        onClicked: field.pickerVisible ? field.closePicker() : field.openPicker()
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
        background: Rectangle {color: Theme.content;border.color: Theme.separator;radius: Theme.panelRadius}
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
