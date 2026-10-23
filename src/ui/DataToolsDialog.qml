pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs

Dialog {
    id: root
    required property TaskWorkspace workspace
    width: Math.min(550,parent.width-40)
    height: Math.min(600,parent.height-32)
    title: qsTr("数据导出、导入与恢复")
    modal: true
    closePolicy: Popup.NoAutoClose
    contentItem: ScrollView {
        clip: true
    ColumnLayout {
        width: root.availableWidth-12
        spacing: Theme.fieldGap
        enabled: !root.workspace.busy
        Label { text: qsTr("数据只保存在本机。JSON保留任务、事件、关联、组织、清单和重复规则；CSV仅导出任务供表格阅读。"); Layout.fillWidth: true; wrapMode: Text.Wrap }
        SectionLabel {text: qsTr("导出与备份")}
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            Button { text: qsTr("导出JSON…"); onClicked: exportJson.open() }
            Button { text: qsTr("导出CSV…"); onClicked: exportCsv.open() }
            Button { text: qsTr("创建快照"); onClicked: root.workspace.createBackup() }
        }
        SectionLabel {text: qsTr("导入与恢复")}
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            Button { text: qsTr("预览JSON导入…"); onClicked: importJson.open() }
            Button { text: qsTr("预览快照恢复…"); onClicked: restoreSnapshot.open() }
        }
        Label { text: root.workspace.importSummary; visible: text.length>0; Layout.fillWidth: true; wrapMode: Text.Wrap }
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            visible: root.workspace.importSummary.length>0
            Button { text: qsTr("仅添加，跳过相同ID"); onClicked: root.workspace.applyImport(false) }
            Button { text: qsTr("替换当前数据…"); palette.buttonText: Theme.danger; flat: true; onClicked: replaceFirst.open() }
        }
        Label { text: root.workspace.errorText || root.workspace.infoText; Layout.fillWidth: true; wrapMode: Text.Wrap; color: root.workspace.errorText ? Theme.danger : Theme.success }
        Label { text: qsTr("默认每日首次启动自动保留快照（最近7份）。替换/恢复前另建完整快照；损坏导入不修改数据。"); Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
    }
    }
    footer: DialogButtonBox { Button { text: qsTr("返回"); enabled: !root.workspace.busy; onClicked: root.close() } }
    NativeDialogs.FileDialog { id: exportJson; title: qsTr("导出Daymark JSON"); fileMode: NativeDialogs.FileDialog.SaveFile; nameFilters: ["Daymark JSON (*.json)"]; defaultSuffix: "json"; onAccepted: root.workspace.exportData(selectedFile.toString(),false) }
    NativeDialogs.FileDialog { id: exportCsv; title: qsTr("导出CSV"); fileMode: NativeDialogs.FileDialog.SaveFile; nameFilters: ["CSV (*.csv)"]; defaultSuffix: "csv"; onAccepted: root.workspace.exportData(selectedFile.toString(),true) }
    NativeDialogs.FileDialog { id: importJson; title: qsTr("选择Daymark JSON"); fileMode: NativeDialogs.FileDialog.OpenFile; nameFilters: ["Daymark JSON (*.json)"]; onAccepted: root.workspace.previewImport(selectedFile.toString(),false) }
    NativeDialogs.FileDialog { id: restoreSnapshot; title: qsTr("选择Daymark一致性快照"); fileMode: NativeDialogs.FileDialog.OpenFile; nameFilters: ["Daymark snapshot (*.sqlite)"]; onAccepted: root.workspace.previewImport(selectedFile.toString(),true) }
    Dialog {
        id: replaceFirst
        parent: Overlay.overlay
        width: Math.min(440,root.parent.width-40)
        height: Math.min(180*Theme.scale,root.parent.height-40)
        modal: true
        title: qsTr("替换会移除当前全部数据")
        standardButtons: Dialog.Ok|Dialog.Cancel
        contentItem: Label { text: qsTr("当前数据会被预览文件替代。先创建备份，再事务替换。继续？");wrapMode: Text.Wrap }
        onAccepted: replaceSecond.open()
    }
    Dialog {
        id: replaceSecond
        parent: Overlay.overlay
        width: Math.min(440,root.parent.width-40)
        height: Math.min(180*Theme.scale,root.parent.height-40)
        modal: true
        title: qsTr("最后确认替换")
        standardButtons: Dialog.Ok|Dialog.Cancel
        contentItem: Label { text: qsTr("确认使用已验证的预览替换本机数据？备份失败则不执行。");wrapMode: Text.Wrap }
        onAccepted: root.workspace.applyImport(true)
    }
}
