pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    required property TaskWorkspace workspace
    property var month: ({title: "",days: []})
    property string field: "start"
    property string selectedDate: ""
    signal dateSelected(string date,string field)
    title: field === "start" ? qsTr("选择计划开始日") : qsTr("选择截止日")
    width: Math.min(500,parent.width-32)
    height: Math.min(530,parent.height-32)
    modal: true
    standardButtons: Dialog.Cancel
    function showFor(date,which) {field=which;selectedDate=date;month=workspace.datePicker(date,0);open()}
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: root.availableWidth-12
            RowLayout {
                IconToolButton {contentItem: AppIcon {name: "left"} Accessible.name: qsTr("上月"); onClicked: root.month=root.workspace.datePicker(root.month.anchor,-1) }
                Label { text: root.month.title; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                IconToolButton {contentItem: AppIcon {name: "right"} Accessible.name: qsTr("下月"); onClicked: root.month=root.workspace.datePicker(root.month.anchor,1) }
            }
            GridLayout {
                columns: 7
                Layout.fillWidth: true
                Repeater {
                    model: root.workspace.sundayFirst ? [qsTr("日"),qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六")] : [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]
                    Label { required property string modelData; text: modelData; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
                }
                Repeater {
                    model: root.month.days
                    Button {
                        required property var modelData
                        text: modelData.day
                        Layout.fillWidth: true
                        Layout.minimumWidth: 24
                        implicitWidth: 32
                        implicitHeight: Math.max(32,contentItem.implicitHeight+8)
                        highlighted: modelData.date===root.selectedDate
                        font.weight: modelData.today ? Font.DemiBold : Font.Normal
                        palette.buttonText: modelData.today ? Theme.accent : Theme.text
                        flat: !modelData.inMonth
                        Accessible.name: modelData.date+(modelData.today ? qsTr("，今天") : "")+(highlighted ? qsTr("，已选择") : "")
                        onClicked: {root.dateSelected(modelData.date,root.field);root.close()}
                    }
                }
            }
            RowLayout {
                Button { text: qsTr("今天"); onClicked: {root.dateSelected(root.workspace.dateOffset(0),root.field);root.close()} }
                Button { text: qsTr("明天"); onClicked: {root.dateSelected(root.workspace.dateOffset(1),root.field);root.close()} }
                Button { text: qsTr("清除日期"); onClicked: {root.dateSelected("",root.field);root.close()} }
            }
        }
    }
}
