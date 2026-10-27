pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    objectName: "catalogCreateDialog"
    required property TaskWorkspace workspace
    property string objectPrefix: ""
    property string kind: "project"
    property bool pending: false
    property bool forManagement: false
    readonly property var roots: workspace.categoryTree.filter(item=>item.depth===0)
    property bool selectAfterCreate: true
    signal created(string kind,string id)
    title: kind==="project" ? qsTr("新建分类") : kind==="area" ? qsTr("新建分类") : qsTr("新建标签")
    modal: true
    width: Math.min(460*Theme.scale,parent.width-32)
    height: Math.min(implicitHeight,parent.height-32)
    closePolicy: Popup.NoAutoClose
    function showFor(value){forManagement=false;openFor(value,true)}
    function showForLibrary(value){forManagement=false;openFor(value,false)}
    function showChild(parentId){forManagement=false;openFor("project",false);parentChoice.currentIndex=Math.max(0,roots.findIndex(item=>item.id===parentId)+1)}
    function showForManagement(value,parentId){showChild(parentId);kind=value;forManagement=true}
    function openFor(value,forTask){selectAfterCreate=forTask;kind=value;name.clear();colorChoice.currentIndex=0;pending=false;parentChoice.currentIndex=0;workspace.clearError();open();name.forceActiveFocus()}
    function create(){pending=workspace.saveCatalog({kind:kind,title:name.text,colorKey:kind==="tag" ? colorChoice.keys[colorChoice.currentIndex] : "neutral",parentId:kind==="project" && parentChoice.currentIndex>0 ? roots[parentChoice.currentIndex-1].id : ""});}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.workspace.busy;onActivated: root.close()}
    contentItem: ScrollView {
        implicitHeight: fields.implicitHeight;clip: true
        ColumnLayout {id: fields;width: root.availableWidth-12;spacing: 10
        RowLayout {
            Layout.fillWidth: true;spacing: 10
            Rectangle {implicitWidth: 36;implicitHeight: 36;radius: Theme.menuRadius;color: Theme.primaryFill
                AppIcon {anchors.centerIn: parent;name: root.kind==="tag" ? "tag" : "folder";ink: root.kind==="tag" ? Theme.catalogColor(colorChoice.keys[colorChoice.currentIndex]) : Theme.accent}}
            Label {text: root.kind==="tag" ? qsTr("用标签横向查找任务") : qsTr("整理任务的归属");font.pixelSize: Theme.captionSize;color: Theme.secondary;Layout.fillWidth: true;wrapMode: Text.Wrap}
        }
        FormRow {
            label: qsTr("名称");labelWidth: 72*Theme.scale
            TextField {id: name;objectName: root.objectPrefix+"newCatalogName";Layout.fillWidth: true;Accessible.name: qsTr("名称");placeholderText: root.kind==="tag" ? qsTr("标签名称") : qsTr("分类名称");enabled: !root.workspace.busy;onAccepted: if(text.trim().length>0)root.create()}
        }
        FormRow {
            visible: root.kind==="project";label: qsTr("上级");labelWidth: 72*Theme.scale
            ComboBox {id: parentChoice;objectName: root.objectPrefix+"newCategoryParent";Layout.fillWidth: true;Accessible.name: qsTr("上级分类");model: [qsTr("无 · 一级分类")].concat(root.roots.map(item=>item.title+(item.hidden || item.status!=="OPEN" ? qsTr("（已归档）") : "")))}
        }
        FormRow {
            visible: root.kind==="tag";label: qsTr("颜色");labelWidth: 72*Theme.scale
            CatalogColors {id: colorChoice;objectName: root.objectPrefix+"newCatalogColors"}
        }
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;visible: root.selectAfterCreate;text: qsTr("新建后选入任务草稿，任务修改仍需保存。")}
        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText;visible: text.length>0}
    }
    }
    footer: ActionBar {
        Button {text: qsTr("取消");enabled: !root.workspace.busy;onClicked: root.close()}
        Button {objectName: root.objectPrefix+"createCatalogHere";text: root.selectAfterCreate ? qsTr("新建并选择") : qsTr("新建");highlighted: true;enabled: !root.workspace.busy && name.text.trim().length>0;onClicked: root.create()}
    }
    Connections {
        target: root.workspace
        function onCatalogSaved(id,kind){if(root.visible && root.pending && root.kind===kind){root.pending=false;root.created(kind,id);root.close()}}
        function onOperationFinished(operation,success){if(root.visible && operation==="catalog" && !success)root.pending=false}
    }
}
