pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "batchTaskDialog"
    required property TaskWorkspace workspace
    property var selection: []
    property int initialAction: 0
    readonly property var actions: ["complete","project","start","tags","reopen","delete","restore"]
    readonly property bool deleting: action.currentIndex===5
    readonly property bool restoring: action.currentIndex===6
    readonly property int affectedCount:workspace.batchImpact(selection,actions[action.currentIndex])
    property var tags: []
    readonly property var categories: workspace.categoryTree.filter(item=>item.kind==="project")
    signal committed()
    title: restoring ? qsTr("恢复所选任务") : deleting ? qsTr("删除所选任务") : qsTr("批量修改 %1 项").arg(selection.length)
    modal: true
    width: Math.min(480*Theme.scale,parent.width-32)
    height: Math.min(deleting || restoring ? implicitHeight : Math.max(300*Theme.scale,implicitHeight),parent.height-32)
    closePolicy: Popup.NoAutoClose
    onOpened: {action.currentIndex=initialAction;date.clear();tags=[];project.currentIndex=0}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.workspace.busy && !date.pickerVisible && !action.popup.visible && !project.popup.visible;onActivated: root.close()}
    contentItem: ScrollView {
        implicitHeight: batchFields.implicitHeight
        clip: true
        ColumnLayout {
            id: batchFields
            width: root.availableWidth-12
            spacing: Theme.fieldGap
            Label {objectName:"batchImpactSummary";Layout.fillWidth: true;wrapMode: Text.Wrap;text:root.restoring ? qsTr("恢复所选任务及同次删除的关联子任务，共 %1 项。单独删除的其他任务保留；任何冲突或失败，整批保持原状。").arg(root.affectedCount) : root.deleting ? qsTr("将 %1 项选中任务及关联子任务，共 %2 项移到最近删除。可撤销本次删除；其他任务保留。").arg(root.selection.length).arg(root.affectedCount) : qsTr("已选 %1 项，连同关联子任务实际影响 %2 项。一次提交；任何冲突或验证失败，整批保留原值。").arg(root.selection.length).arg(root.affectedCount)}
            FormGroup {
                visible:!root.deleting && !root.restoring
                heading: qsTr("选择要修改的内容")
            ComboBox {id: action;Accessible.name: qsTr("批量操作");Layout.fillWidth: true;model: [qsTr("完成任务"),qsTr("移动到分类"),qsTr("修改开始日"),qsTr("替换标签"),qsTr("重新打开任务"),qsTr("删除任务"),qsTr("恢复任务")]}
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
        ActionButton {
            objectName: "applyBatchSelection"
            symbol:root.restoring ? "undo" : root.deleting ? "trash" : "check"
            highlighted: !root.deleting
            palette.buttonText:root.deleting ? Theme.danger : Theme.text
            text:root.restoring ? qsTr("恢复 %1 项").arg(root.affectedCount) : root.deleting ? qsTr("删除 %1 项").arg(root.affectedCount) : qsTr("应用到 %1 项").arg(root.affectedCount)
            enabled: root.selection.length>0 && !root.workspace.busy
            onClicked: root.workspace.batchTasks(root.selection,root.actions[action.currentIndex],action.currentIndex===1 ? (project.currentIndex>0 ? root.categories[project.currentIndex-1].id : "") : action.currentIndex===2 ? date.text : root.tags.join(","))
        }
    }
    Connections {target: root.workspace;function onOperationFinished(operation,success){if(root.visible && operation==="batch" && success){root.close();root.committed()}}}
}
