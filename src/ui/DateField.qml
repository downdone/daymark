pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

TextField {
    id: field
    required property TaskWorkspace workspace
    property string dateLabel: qsTr("日期")
    readonly property bool pickerVisible: picker.visible
    property var month: ({title: "", days: []})
    readonly property var hostWindow: Window.window
    property point anchorPosition: Qt.point(0,0)
    property string placement: "below"
    signal dateChosen()
    selectByMouse: true
    placeholderText: qsTr("未设置")
    Accessible.name: dateLabel
    rightPadding: calendarButton.width+8
    function openPicker() {
        anchorPosition=mapToItem(null,0,0)
        const popupWidth=picker.width
        if(anchorPosition.x>=popupWidth+8)placement="left"
        else if(hostWindow && hostWindow.width-anchorPosition.x-width>=popupWidth+8)placement="right"
        else placement=hostWindow && hostWindow.height-anchorPosition.y-height<anchorPosition.y ? "above" : "below"
        month=workspace.datePicker(text,0)
        picker.open()
        forceActiveFocus()
    }
    function chooseDate(value) {text=value;dateChosen();picker.close();forceActiveFocus()}
    function closePicker() {picker.close()}
    onVisibleChanged: if(!visible)picker.close()
    Keys.onEscapePressed: function(event) {if(picker.visible)picker.close();else event.accepted=false}
    Keys.onReturnPressed: function(event) {if(picker.visible)picker.close();else event.accepted=false}
    Keys.onPressed: function(event) {
        if(event.key===Qt.Key_Down && (event.modifiers & Qt.AltModifier)) {openPicker();event.accepted=true}
        else if(event.key===Qt.Key_Down && picker.visible) {const day=dayButtons.itemAt(0);if(day)day.forceActiveFocus();event.accepted=true}
    }
    TapHandler {onTapped: field.openPicker()}
    ToolButton {
        id: calendarButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Math.max(32,Theme.bodySize+16)
        height: field.height-4
        Accessible.name: qsTr("选择%1").arg(field.dateLabel)
        contentItem: AppIcon {name: "calendar"}
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
        background: Rectangle {color: Theme.content;border.color: Theme.separator;radius: 8}
        contentItem: ScrollView {
            clip: true
            implicitHeight: calendar.implicitHeight
            ColumnLayout {
                id: calendar
                width: picker.availableWidth
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    ToolButton {contentItem: AppIcon {name: "left"} Accessible.name: qsTr("上月");onClicked: field.month=field.workspace.datePicker(field.month.anchor,-1)}
                    Label {Layout.fillWidth: true;text: field.month.title;horizontalAlignment: Text.AlignHCenter}
                    ToolButton {contentItem: AppIcon {name: "right"} Accessible.name: qsTr("下月");onClicked: field.month=field.workspace.datePicker(field.month.anchor,1)}
                }
                GridLayout {
                    Layout.fillWidth: true
                    columns: 7
                    uniformCellWidths: true
                    columnSpacing: 2
                    rowSpacing: 2
                    Repeater {
                        model: field.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
                        Label {required property string modelData;text: modelData;font.pixelSize: Theme.captionSize;color: Theme.secondary;Layout.fillWidth: true;horizontalAlignment: Text.AlignHCenter}
                    }
                    Repeater {
                        id: dayButtons
                        model: field.month.days
                        Button {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            Layout.minimumWidth: 28
                            implicitWidth: 32
                            padding: 4
                            horizontalPadding: 0
                            implicitHeight: Math.max(32,contentItem.implicitHeight+8)
                            text: modelData.day
                            flat: field.text!==modelData.date
                            highlighted: field.text===modelData.date
                            font.weight: modelData.today ? Font.DemiBold : Font.Normal
                            palette.buttonText: modelData.today ? Theme.accent : Theme.text
                            Accessible.name: modelData.date+(modelData.today ? qsTr("，今天") : "")+(highlighted ? qsTr("，已选择") : "")
                            onClicked: field.chooseDate(modelData.date)
                            function focusOffset(offset) {const next=dayButtons.itemAt(Math.max(0,Math.min(dayButtons.count-1,index+offset)));if(next)next.forceActiveFocus()}
                            Keys.onLeftPressed: focusOffset(-1)
                            Keys.onRightPressed: focusOffset(1)
                            Keys.onUpPressed: focusOffset(-7)
                            Keys.onDownPressed: focusOffset(7)
                        }
                    }
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: 4
                    Button {text: qsTr("今天");flat: true;onClicked: field.chooseDate(field.workspace.dateOffset(0))}
                    Button {text: qsTr("明天");flat: true;onClicked: field.chooseDate(field.workspace.dateOffset(1))}
                    Button {text: qsTr("清除");flat: true;onClicked: field.chooseDate("")}
                }
                Label {Layout.fillWidth: true;text: qsTr("可直接输入 yyyy-MM-dd，Enter 收起日历");font.pixelSize: Theme.captionSize;color: Theme.secondary;wrapMode: Text.Wrap}
            }
        }
    }
}
