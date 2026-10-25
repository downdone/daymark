pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    objectName: "catalogEditPopup"
    required property TaskWorkspace workspace
    property var item: ({})
    property Item trigger: null
    property bool dirty: false
    property bool saving: false
    readonly property var hostWindow: parent ? parent.Window.window : null
    readonly property var colors: ["neutral","blue","green","orange","purple"]
    readonly property var roots: [{id:"",title:qsTr("一级分类")}].concat(workspace.categoryTree.filter(value=>value.depth===0 && value.id!==item.id))
    signal saved(string id)
    function showFor(value,anchor) {
        item=value;trigger=anchor;name.text=value.title;description.text=value.description || "";colorChoice.currentIndex=Math.max(0,colors.indexOf(value.colorKey));parentChoice.currentIndex=Math.max(0,roots.findIndex(entry=>entry.id===(value.parentId || "")));dirty=false;workspace.clearError();open()
    }
    function requestClose(){if(dirty)discard.open();else close()}
    width: Math.min(400*Theme.scale,hostWindow ? hostWindow.width-32 : 800)
    height: Math.min(contentItem.implicitHeight+padding*2,hostWindow ? hostWindow.height-32 : 600)
    padding: 14;margins: 8;focus: true;modal: true;dim: false
    closePolicy: dirty ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onAboutToShow: {
        const at=trigger ? trigger.mapToItem(null,trigger.width,0) : Qt.point(24,24)
        const w=hostWindow ? hostWindow.width : 1120,h=hostWindow ? hostWindow.height : 720
        x=Math.max(16,Math.min(at.x+8,w-width-16));y=Math.max(16,Math.min(at.y,h-height-16))
    }
    onOpened: {name.forceActiveFocus();name.selectAll()}
    onClosed: {if(trigger)trigger.forceActiveFocus();trigger=null}
    Shortcut {sequence: "Escape";enabled: root.visible && !discard.visible && !parentChoice.popup.visible;onActivated: root.requestClose()}
    background: Rectangle {color: Theme.content;radius: Theme.panelRadius;border.color: Theme.separator}
    contentItem: ColumnLayout {
        spacing: 10
        Label {text: root.item.kind==="tag" ? qsTr("编辑标签") : qsTr("编辑分类");font.weight: Font.DemiBold;font.pixelSize: Theme.bodySize}
        ScrollView {
            Layout.fillWidth: true;Layout.fillHeight: true;implicitHeight: fields.implicitHeight;clip: true
            ColumnLayout {
                id: fields;width: root.availableWidth;spacing: 8;enabled: !root.workspace.busy || root.workspace.backgroundBusy
                FormRow {
                    label: qsTr("名称");labelWidth: 64*Theme.scale
                    TextField {id: name;objectName: "catalogEditName";Layout.fillWidth: true;Accessible.name: qsTr("名称");onTextEdited: root.dirty=true;onAccepted: if(save.enabled)save.clicked()}
                }
                FormRow {
                    visible: root.item.kind!=="tag";label: qsTr("上级");labelWidth: 64*Theme.scale
                    ComboBox {id: parentChoice;objectName: "catalogEditParent";Layout.fillWidth: true;enabled: !(root.item.childCount>0);model: root.roots.map(value=>value.title);Accessible.name: qsTr("上级分类");onActivated: root.dirty=true}
                }
                Label {visible: root.item.kind!=="tag" && root.item.childCount>0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("含二级分类，移动前需先整理下级。");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                FormRow {
                    visible: root.item.kind!=="tag";label: qsTr("说明");labelWidth: 64*Theme.scale
                    TextField {id: description;Layout.fillWidth: true;placeholderText: qsTr("可选");Accessible.name: qsTr("分类说明");onTextEdited: root.dirty=true}
                }
                FormRow {
                    visible: root.item.kind==="tag";label: qsTr("颜色");labelWidth: 64*Theme.scale
                    CatalogColors {id: colorChoice;onActivated: root.dirty=true}
                }
                Label {visible: root.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.workspace.errorText;color: Theme.danger;font.pixelSize: Theme.captionSize}
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Item {Layout.fillWidth: true}
            Button {text: qsTr("取消");enabled: !root.saving;onClicked: root.requestClose()}
            Button {id: save;objectName: "saveCatalogEdit";text: qsTr("保存");highlighted: true;enabled: !root.workspace.busy;onClicked: {
                root.saving=true
                const patch=Object.assign({},root.item,{title:name.text,description:description.text})
                if(root.item.kind==="tag")patch.colorKey=root.colors[colorChoice.currentIndex]
                else patch.parentId=root.roots[parentChoice.currentIndex].id
                if(!root.workspace.saveCatalog(patch))root.saving=false
            }}
        }
    }
    DiscardChangesDialog {id: discard;objectName: "catalogEditDiscard";parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(420,parent.width-32);modal: true;title: qsTr("放弃未保存的修改？");onAccepted: {root.dirty=false;root.close()}}
    Connections {
        target: root.workspace
        function onOperationFinished(operation,success){if(operation!=="catalog" || !root.saving)return;root.saving=false;if(success){root.dirty=false;root.saved(root.item.id);root.close()}}
    }
}
