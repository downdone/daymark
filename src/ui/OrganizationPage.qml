pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    background: Rectangle {color: Theme.content}
    padding: 0
    property bool navigationMenuOpen: false
    property bool editing: false
    property string filterKind: "project"
    property bool archivedOnly: false
    readonly property var sectionKinds: ["project","tag"]
    readonly property var sectionNames: [qsTr("分类"),qsTr("标签")]
    readonly property string purpose: filterKind==="project" ? qsTr("按一级和二级分类整理任务；分类可选。") : qsTr("用标签跨分类查找任务，例如电话、外出。")
    readonly property var displayedItems: filterKind==="project" ? workspace.categoryTree.filter(value=>!archivedOnly || value.archived) : workspace.catalogOf("tag")
    signal createRequested(string kind,string parentId)
    signal categoryMoved(string id)
    signal optionsRequested(var item,Item anchor,real x,real y)
    property string pendingOperation: ""
    readonly property var moveRoots: workspace.categoryTree.filter(item=>item.depth===0 && item.id!==selected.id)
    readonly property bool moveBlocked: (selected.childCount || 0)>0
    function showMove(){safely(function(){moveParent.currentIndex=Math.max(0,moveRoots.findIndex(item=>item.id===selected.parentId)+1);workspace.clearError();moveDialog.open()})}
    function showKind(value) {filterKind=value==="area" ? "project" : value;editing=false;dirty=false;selected=({})}
    signal backRequested()
    function close() {backRequested()}
    function begin() {archivedOnly=false;editing=false;dirty=false;selected=({})}
    objectName: "organizationPage"
    required property TaskWorkspace workspace
    property var selected: ({})
    property bool dirty: false
    property var pendingAction: null
    readonly property var colors: ["neutral","blue","green","orange","purple"]
    title: qsTr("分类与标签")
    readonly property bool modalOpen: discard.visible || confirmDelete.visible || moveDialog.visible
    header: ColumnLayout {
        spacing: Theme.fieldGap
        SegmentedPicker {
            visible: !root.archivedOnly
            objectName: "catalogPageTabs"
            model: root.sectionNames;currentIndex: root.sectionKinds.indexOf(root.filterKind);selectionIsExternal: true
            onActivated: function(index){root.safely(function(){root.showKind(root.sectionKinds[index])})}
        }
        ActionBar {
            visible: root.displayedItems.length>0
            Layout.fillWidth: true;padding: 0;bottomPadding: Theme.fieldGap
            ActionButton {visible: root.stacked && root.editing;symbol: "list";text: qsTr("返回列表");onClicked: root.safely(function(){root.editing=false})}
            ActionButton {objectName: root.displayedItems.length ? "newOrganization" : "newOrganizationListAction";symbol: "plus";text: qsTr("新建%1").arg(root.sectionNames[root.sectionKinds.indexOf(root.filterKind)]);onClicked: root.safely(function(){root.startNew(root.filterKind)})}
        }
    }
    function safely(action) {if(dirty){pendingAction=action;discard.open()}else action()}
    function choose(item) {
        editing=true;filterKind=item.kind==="tag" ? "tag" : "project";selected=item
        itemTitle.text=item.title;description.text=item.description || "";colorChoice.currentIndex=Math.max(0,colors.indexOf(item.colorKey));archived.checked=!!item.archived;dirty=false
    }
    function save() {
        pendingOperation="save"
        if(!workspace.saveCatalog(Object.assign({},selected,{title:itemTitle.text,description:description.text,colorKey:filterKind==="tag" ? colors[colorChoice.currentIndex] : selected.colorKey || "neutral",archived:archived.checked,planningState:"ACTIVE",status:"OPEN"})))pendingOperation=""
    }
    function startNew(value) {root.createRequested(value,"")}
    Shortcut {sequence: "Escape";enabled: root.visible && !root.navigationMenuOpen && !discard.visible && !confirmDelete.visible && !moveDialog.visible && !colorChoice.popup.visible;onActivated: root.safely(function(){root.close()})}
    readonly property bool stacked: availableWidth<640*Theme.scale
    contentItem: GridLayout {
        columns: root.stacked ? 1 : 2
        columnSpacing: 20
        rowSpacing: Theme.fieldGap
        ScrollView {
            id: catalogList
            visible: root.displayedItems.length>0 && (!root.stacked || !root.editing)
            Layout.preferredWidth: 260
            Layout.fillWidth: root.stacked
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                id: catalogColumn
                width: catalogList.availableWidth-12
                Repeater {
                    model: root.displayedItems
                    ItemDelegate {
                        id: catalogEntry
                        required property var modelData
                        Layout.fillWidth: true
                        highlighted: root.selected.id===modelData.id
                        leftPadding: 10+(modelData.depth || 0)*16
                        ToolTip.visible: hovered;ToolTip.text: modelData.path || modelData.title
                        text: modelData.title + (modelData.archived ? qsTr("（已归档）") : "")
                        TapHandler {acceptedButtons: Qt.RightButton;onTapped: function(point){const x=point.position.x,y=point.position.y;root.safely(function(){root.optionsRequested(catalogEntry.modelData,catalogEntry,x,y)})}}
                        Keys.onPressed: function(event){if(event.key===Qt.Key_Menu || (event.key===Qt.Key_F10 && (event.modifiers & Qt.ShiftModifier))){root.safely(function(){root.optionsRequested(modelData,catalogEntry,catalogEntry.width/2,catalogEntry.height)});event.accepted=true}}
                        onClicked: root.safely(function(){root.choose(modelData)})
                    }
                }
            }
        }
        ScrollView {
            id: catalogForm
            objectName: "organizationForm"
            visible: root.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
            width: catalogForm.availableWidth-12
            spacing: Theme.fieldGap
            enabled: !root.workspace.busy || root.workspace.backgroundBusy
            SectionLabel {text: qsTr("编辑%1").arg(root.filterKind==="tag" ? qsTr("标签") : qsTr("分类"))}
            FormGroup {
            Label {text: qsTr("名称");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            TextField { id: itemTitle;Accessible.name: qsTr("组织名称"); Layout.fillWidth: true; objectName: "organizationTitle";placeholderText: qsTr("名称"); onTextEdited: root.dirty=true }
            SettingSwitch {id: archived;symbol: "inbox";Layout.fillWidth: true;visible: root.filterKind==="project";text: qsTr("归档分类");Accessible.description: qsTr("隐藏活动任务，可取消归档恢复");onClicked: root.dirty=true}
            ComboBox { id: colorChoice;Accessible.name: qsTr("颜色"); visible: root.filterKind==="tag"; model: [qsTr("中性"),qsTr("蓝"),qsTr("绿"),qsTr("橙"),qsTr("紫")]; onActivated: root.dirty=true }
            TextField { id: description;Accessible.name: qsTr("分类说明"); visible: root.filterKind==="project"; Layout.fillWidth: true; placeholderText: qsTr("分类说明"); onTextEdited: root.dirty=true }
            Label {visible: root.filterKind==="project";Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.selected.parentId ? qsTr("二级分类 · %1").arg(root.selected.path || root.selected.title) : qsTr("一级分类");color: Theme.secondary}
            ActionButton {objectName: "moveCategory";visible: root.selected.kind==="project";symbol: "folder";text: qsTr("移动 / 调整层级…");onClicked: root.showMove()}
            ActionButton {visible: root.selected.kind==="project" && !root.selected.parentId;symbol: "plus";text: qsTr("新建二级分类…");onClicked: root.safely(function(){root.createRequested("project",root.selected.id)})}
            }
            Label {visible: root.filterKind==="project" && archived.checked;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("活动任务将隐藏，取消归档后恢复。");color: Theme.secondary}
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.errorText; color: Theme.danger }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.gap
                WidgetAction {symbol: "up";description: qsTr("上移排序");enabled: !!root.selected.id && !root.dirty && root.workspace.canMoveCatalog(root.selected.kind,root.selected.id,-1);onClicked: {root.pendingOperation="reorder";if(!root.workspace.moveCatalog(root.selected.kind,root.selected.id,root.selected.rowVersion,-1))root.pendingOperation=""}}
                WidgetAction {symbol: "down";description: qsTr("下移排序");enabled: !!root.selected.id && !root.dirty && root.workspace.canMoveCatalog(root.selected.kind,root.selected.id,1);onClicked: {root.pendingOperation="reorder";if(!root.workspace.moveCatalog(root.selected.kind,root.selected.id,root.selected.rowVersion,1))root.pendingOperation=""}}
            }
            Item { Layout.fillHeight: true }
            }
        }
        Item {
            Layout.fillWidth: true;Layout.fillHeight: true;visible: !root.editing && (!root.stacked || root.displayedItems.length===0)
            EmptyState {
            anchors.centerIn: parent;width: Math.min(parent.width,540*Theme.scale)
            symbol: root.filterKind==="tag" ? "tag" : "folder";heading: root.displayedItems.length ? qsTr("选择一个%1").arg(root.sectionNames[root.sectionKinds.indexOf(root.filterKind)]) : qsTr("还没有%1").arg(root.sectionNames[root.sectionKinds.indexOf(root.filterKind)])
            detail: root.displayedItems.length ? "" : root.purpose
            actionObjectName: root.displayedItems.length ? "newOrganizationEmptyAction" : "newOrganization"
            actionText: root.displayedItems.length ? "" : qsTr("新建%1").arg(root.sectionNames[root.sectionKinds.indexOf(root.filterKind)]);onTriggered: root.startNew(root.filterKind)
            }
        }
    }
    footer: ActionBar {
        objectName: "organizationActions"
        visible: root.editing
        padding: 0;topPadding: Theme.fieldGap
        ActionButton {symbol: "trash";text: qsTr("删除…");flat: true;palette.buttonText: Theme.danger;enabled: !!root.selected.id && !root.workspace.busy;onClicked: confirmDelete.open()}
        Label {visible: root.dirty;text: qsTr("未保存");color: Theme.secondary}
        Button {text: qsTr("取消编辑");enabled: !root.workspace.busy;onClicked: root.safely(function(){root.editing=false})}
        Button {objectName: "saveOrganization";text: qsTr("保存");highlighted: true;enabled: !root.workspace.busy;onClicked: root.save()}
    }
    DiscardChangesDialog {id: discard;objectName: "organizationDiscard";parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);modal: true;title: qsTr("放弃未保存的整理修改？");onRejected: root.pendingAction=null;onAccepted: {root.dirty=false;root.editing=false;const action=root.pendingAction;root.pendingAction=null;if(action)action()}}
    Dialog {
        id: confirmDelete
        parent: Overlay.overlay
        width: Math.min(440,root.parent.width-40)
        height: Math.min(180*Theme.scale,root.parent.height-40)
        anchors.centerIn: parent
        title: qsTr("删除此%1？").arg(root.filterKind==="tag" ? qsTr("标签") : qsTr("分类"))
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: root.filterKind==="tag" ? qsTr("仅移除此标签及任务上的标记，任务和分类保持不变。") : qsTr("相关任务会保留并移出此分类；二级分类将提升为一级。确认继续？");wrapMode: Text.Wrap }
        onAccepted: {root.pendingOperation="delete";if(!root.workspace.deleteCatalog(root.selected.kind,root.selected.id,root.selected.rowVersion))root.pendingOperation=""}
    }
    Dialog {
        id: moveDialog;objectName: "categoryMoveDialog";parent: Overlay.overlay;anchors.centerIn: parent
        title: qsTr("移动分类");modal: true;closePolicy: Popup.NoAutoClose
        width: Math.min(480,parent.width-32);height: Math.min(implicitHeight,parent.height-32)
        contentItem: ColumnLayout {
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("当前位置：%1").arg(root.selected.path || root.selected.title)}
            Label {visible: root.moveBlocked;Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("此一级分类含有二级分类。请先移动或提升它们，再将此分类降为二级。任务归属会保留。");color: Theme.secondary}
            ComboBox {id: moveParent;objectName: "categoryMoveParent";visible: !root.moveBlocked;Layout.fillWidth: true;Accessible.name: qsTr("移至上级分类");model: [qsTr("无上级 · 提升 / 保留为一级")].concat(root.moveRoots.map(item=>item.title+(item.hidden || item.status!=="OPEN" ? qsTr("（已隐藏）") : "")))}
            Label {visible: !root.moveBlocked;Layout.fillWidth: true;wrapMode: Text.Wrap;text: moveParent.currentIndex>0 ? qsTr("移动后：%1 / %2（二级分类）").arg(root.moveRoots[moveParent.currentIndex-1].title).arg(root.selected.title || "") : qsTr("移动后：%1（一级分类）").arg(root.selected.title || "");color: Theme.accent}
            Label {visible: root.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;text: root.workspace.errorText;color: Theme.danger}
        }
        footer: ActionBar {
            Button {text: qsTr("取消");enabled: !root.workspace.busy;onClicked: moveDialog.close()}
            Button {objectName: "confirmCategoryMove";text: qsTr("移动");highlighted: true;enabled: !root.moveBlocked && !root.workspace.busy && (moveParent.currentIndex>0 ? root.moveRoots[moveParent.currentIndex-1].id : "")!==(root.selected.parentId || "");onClicked: {root.pendingOperation="move";if(!root.workspace.reparentCategory(root.selected.id,root.selected.rowVersion,moveParent.currentIndex>0 ? root.moveRoots[moveParent.currentIndex-1].id : ""))root.pendingOperation=""}}
        }
        Shortcut {sequence: "Escape";enabled: moveDialog.visible && !root.workspace.busy && !moveParent.popup.visible;onActivated: moveDialog.close()}
    }
    Connections {
        target: root.workspace
        function onOperationFinished(operation,success){
            if(operation!=="catalog")return
            if(!root.pendingOperation){if(success && root.visible && root.editing && !root.dirty){const current=root.displayedItems.find(value=>value.id===root.selected.id);if(current)root.choose(current);else {root.editing=false;root.selected=({})}}return}
            const action=root.pendingOperation;root.pendingOperation=""
            if(!success)return
            if(action==="delete"){root.dirty=false;root.editing=false;root.selected=({});return}
            const item=root.displayedItems.find(value=>value.id===root.selected.id)
            if(item){root.choose(item);if(action==="move"){moveDialog.close();root.categoryMoved(item.id)}}
        }
    }
}
