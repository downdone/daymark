pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "catalogCreateDialog"
    required property TaskWorkspace workspace
    property string kind: "project"
    property bool pending: false
    signal created(string kind,string id)
    title: kind==="project" ? qsTr("新建项目") : kind==="area" ? qsTr("新建领域") : qsTr("新建标签")
    modal: true
    width: Math.min(460,parent.width-32)
    height: Math.min(implicitHeight,parent.height-32)
    closePolicy: Popup.NoAutoClose
    function showFor(value){kind=value;name.clear();pending=false;workspace.clearError();open();name.forceActiveFocus()}
    function create(){pending=workspace.saveCatalog({kind:kind,title:name.text});}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.workspace.busy;onActivated: root.close()}
    contentItem: ScrollView {
        implicitHeight: fields.implicitHeight;clip: true
        ColumnLayout {id: fields;width: root.availableWidth-12;spacing: 10
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.kind==="project" ? qsTr("把相关任务放在同一个目标下，例如“装修书房”。") : root.kind==="area" ? qsTr("管理长期职责，例如“工作”或“生活”。") : qsTr("给跨项目的任务加标记，例如“电话”或“外出”。")}
        TextField {id: name;objectName: "newCatalogName";Layout.fillWidth: true;Accessible.name: qsTr("名称");placeholderText: qsTr("输入名称");enabled: !root.workspace.busy;onAccepted: root.create()}
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("新建后保留在项目与标签库，并选入当前任务草稿；任务修改仍需保存。")}
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText;visible: text.length>0}
    }
    }
    footer: ActionBar {
        Button {text: qsTr("取消");enabled: !root.workspace.busy;onClicked: root.close()}
        Button {objectName: "createCatalogHere";text: qsTr("新建并选择");highlighted: true;enabled: !root.workspace.busy && name.text.trim().length>0;onClicked: root.create()}
    }
    Connections {
        target: root.workspace
        function onCatalogSaved(id,kind){if(root.visible && root.pending && root.kind===kind){root.pending=false;root.created(kind,id);root.close()}}
        function onOperationFinished(operation,success){if(root.visible && operation==="catalog" && !success)root.pending=false}
    }
}
