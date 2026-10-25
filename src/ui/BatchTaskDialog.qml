pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "batchTaskDialog"
    required property TaskWorkspace workspace
    property var selection: []
    property var tags: []
    readonly property var categories: workspace.categoryTree.filter(item=>item.kind==="project")
    signal committed()
    title: qsTr("批量修改 %1 项").arg(selection.length)
    modal: true
    width: Math.min(480*Theme.scale,parent.width-32)
    height: Math.min(Math.max(300*Theme.scale,implicitHeight),parent.height-32)
    closePolicy: Popup.NoAutoClose
    onOpened: {action.currentIndex=0;date.clear();tags=[];project.currentIndex=0}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.workspace.busy && !date.pickerVisible && !action.popup.visible && !project.popup.visible;onActivated: root.close()}
    contentItem: ScrollView {
        implicitHeight: batchFields.implicitHeight
        clip: true
        ColumnLayout {
            id: batchFields
            width: root.availableWidth-12
            spacing: Theme.fieldGap
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("所有修改一次提交；任何任务冲突或验证失败，整批保留原值。")}
            FormGroup {
                heading: qsTr("选择要修改的内容")
            ComboBox {id: action;Accessible.name: qsTr("批量操作");Layout.fillWidth: true;model: [qsTr("完成任务"),qsTr("移动到分类"),qsTr("修改开始日"),qsTr("替换标签")]}
            ComboBox {id: project;Accessible.name: qsTr("目标分类");Layout.fillWidth: true;visible: action.currentIndex===1;model: [qsTr("无分类")].concat(root.categories.map(item=>item.path))}
            DateField {id: date;workspace: root.workspace;dateLabel: qsTr("批量开始日期");Layout.fillWidth: true;visible: action.currentIndex===2;placeholderText: qsTr("yyyy-MM-dd，留空清除开始日");Accessible.name: qsTr("批量开始日期")}
            Label {visible: action.currentIndex===3;text: qsTr("替换为所选标签；不选则清除全部标签。")}
            Repeater {
                model: action.currentIndex===3 ? root.workspace.catalogOf("tag") : []
                CheckBox {
                    required property var modelData
                    text: modelData.title
                    onToggled: {const next=root.tags.slice();const i=next.indexOf(modelData.id);if(checked && i<0)next.push(modelData.id);else if(!checked && i>=0)next.splice(i,1);root.tags=next}
                }
            }
            }
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText}
        }
    }
    footer: ActionBar {
        gap: 8
        Button {text: qsTr("取消");enabled: !root.workspace.busy;onClicked: root.close()}
        Button {
            objectName: "applyBatchSelection"
            highlighted: true
            text: qsTr("确认应用到 %1 项").arg(root.selection.length)
            enabled: root.selection.length>0 && !root.workspace.busy
            onClicked: root.workspace.batchTasks(root.selection,["complete","project","start","tags"][action.currentIndex],action.currentIndex===1 ? (project.currentIndex>0 ? root.categories[project.currentIndex-1].id : "") : action.currentIndex===2 ? date.text : root.tags.join(","))
        }
    }
    Connections {target: root.workspace;function onOperationFinished(operation,success){if(operation==="batch" && success){root.close();root.committed()}}}
}
