pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs

Dialog {
    id: root
    objectName: "dataToolsDialog"
    required property TaskWorkspace workspace
    width: Math.min(520*Theme.scale,parent.width-32)
    height: Math.min(480*Theme.scale,parent.height-32)
    title: qsTr("数据导出、导入与恢复")
    modal: true
    closePolicy: Popup.NoAutoClose
    Shortcut {sequence: "Escape";enabled: root.visible && !root.workspace.busy && !replaceFirst.visible && !replaceSecond.visible;onActivated: root.close()}
    contentItem: ScrollView {
        id:toolsScroll
        clip: true
    ColumnLayout {
        width: toolsScroll.availableWidth
        spacing: Theme.fieldGap
        enabled: !root.workspace.busy
        FormGroup {
            heading: qsTr("导出与备份")
            detail: qsTr("JSON保留全部内容，CSV供表格阅读；快照用于完整恢复。")
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            ActionButton {symbol: "export"; text: qsTr("导出JSON…"); onClicked: exportJson.open() }
            ActionButton {symbol: "export"; text: qsTr("导出CSV…"); onClicked: exportCsv.open() }
            ActionButton {symbol: "backup"; text: qsTr("创建快照"); onClicked: root.workspace.createBackup() }
        }
        }
        FormGroup {
            heading: qsTr("导入与恢复")
            detail: root.workspace.syncState.mode==="ACCOUNT" ? qsTr("合并导入会同步至当前账号；此设备的外观设置保持不变。") : qsTr("先预览，再选择仅添加或替换。")
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            ActionButton {symbol: "import"; text: qsTr("预览JSON导入…"); onClicked: importJson.open() }
            ActionButton {symbol: "backup"; text: qsTr("预览快照恢复…"); onClicked: restoreSnapshot.open() }
        }
        Label { text: root.workspace.importSummary; visible: text.length>0; Layout.fillWidth: true; wrapMode: Text.Wrap }
        Flow {
            Layout.fillWidth: true;spacing: Theme.gap
            visible: root.workspace.importSummary.length>0
            Button { highlighted: true;text: qsTr("仅添加，跳过相同ID"); onClicked: root.workspace.applyImport(false) }
            Button { visible:root.workspace.syncState.mode!=="ACCOUNT";text: qsTr("替换当前数据…"); palette.buttonText: Theme.danger; flat: true; onClicked: replaceFirst.open() }
        }
        }
        Label { text: root.workspace.errorText || root.workspace.infoText; Layout.fillWidth: true; wrapMode: Text.Wrap; color: root.workspace.errorText ? Theme.danger : Theme.success }
        RowLayout {Layout.fillWidth:true;Label {Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary;text:root.workspace.backupInfo.lastSuccess ? qsTr("上次备份：%1").arg(root.workspace.backupInfo.lastSuccess) : qsTr("尚无成功备份")} Button{text:qsTr("打开备份位置");onClicked:Qt.openUrlExternally(root.workspace.backupInfo.folderUrl)}}
        Label { text: qsTr("启动时按每 %1 天检查自动备份，保留最近7份。替换/恢复前另建快照；损坏导入不修改数据。").arg(root.workspace.preferences.backupDays); Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
    }
    }
    footer: DialogButtonBox { Button { text: qsTr("关闭"); enabled: !root.workspace.busy; onClicked: root.close() } }
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
        onOpened:{const confirm=standardButton(Dialog.Ok);if(confirm)confirm.text=qsTr("继续核对")}
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
        onOpened:{const confirm=standardButton(Dialog.Ok);if(confirm)confirm.text=qsTr("备份并替换")}
        onAccepted: root.workspace.applyImport(true)
    }
}
