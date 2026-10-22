pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property TaskWorkspace workspace
    property var selected: ({})
    property bool dirty: false
    property var pendingAction: null
    readonly property var colors: ["neutral","blue","green","orange","purple"]
    readonly property var kinds: ["area", "project", "heading", "tag"]
    readonly property var names: [qsTr("领域"), qsTr("项目"), qsTr("分组"), qsTr("标签")]
    property var parents: []
    title: qsTr("组织管理")
    width: Math.min(780, parent.width - 40)
    height: Math.min(580, parent.height - 40)
    modal: true
    closePolicy: Popup.NoAutoClose
    function safely(action) {if(dirty){pendingAction=action;discard.open()}else action()}
    function resetForm() {
        selected = ({})
        itemTitle.clear()
        planning.currentIndex = 0
        status.currentIndex = 0
        parents = workspace.catalogOf(kind.currentIndex === 1 ? "area" : "project")
        parentChoice.currentIndex = 0
        description.clear();projectStart.clear();projectDeadline.clear();colorChoice.currentIndex=0
        dirty=false
    }
    function choose(item) {
        selected = item
        kind.currentIndex = kinds.indexOf(item.kind)
        itemTitle.text = item.title
        planning.currentIndex = item.planningState === "SOMEDAY" ? 1 : 0
        status.currentIndex = ["OPEN", "COMPLETED", "CANCELLED"].indexOf(item.status)
        parents = workspace.catalogOf(item.kind === "project" ? "area" : "project")
        parentChoice.currentIndex = 0
        for (let i = 0; i < parents.length; ++i) if (parents[i].id === item.parentId) parentChoice.currentIndex = i + 1
        description.text=item.description;projectStart.text=item.startOn;projectDeadline.text=item.deadlineOn;colorChoice.currentIndex=colors.indexOf(item.colorKey)
        dirty=false
    }
    function save() {
        workspace.saveCatalog({id: selected.id || "", kind: kinds[kind.currentIndex], title: itemTitle.text,
            parentId: parentChoice.currentIndex > 0 ? parents[parentChoice.currentIndex - 1].id : "",
            planningState: planning.currentIndex === 1 ? "SOMEDAY" : "ACTIVE",
            status: ["OPEN", "COMPLETED", "CANCELLED"][status.currentIndex], rowVersion: selected.rowVersion || 1,
            description:description.text,startOn:projectStart.text,deadlineOn:projectDeadline.text,colorKey:colors[colorChoice.currentIndex]})
    }
    onOpened: resetForm()
    contentItem: RowLayout {
        spacing: 20
        ScrollView {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            ColumnLayout {
                width: 250
                Repeater {
                    model: root.workspace.catalog
                    Button {
                        required property var modelData
                        Layout.fillWidth: true
                        text: root.names[root.kinds.indexOf(modelData.kind)] + " · " + modelData.title + (modelData.status !== "OPEN" ? qsTr("（历史）") : "")
                        onClicked: root.safely(function(){root.choose(modelData)})
                    }
                }
                Label { visible: root.workspace.catalog.length === 0; text: qsTr("先创建一个领域或项目"); wrapMode: Text.Wrap }
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
            width: root.availableWidth-300
            enabled: !root.workspace.busy
            Label { text: root.selected.id ? qsTr("编辑现有项") : qsTr("新建组织项"); font.bold: true }
            ComboBox { id: kind; model: root.names; enabled: !root.selected.id; onActivated: root.resetForm() }
            TextField { id: itemTitle; Layout.fillWidth: true; placeholderText: qsTr("名称"); onTextEdited: root.dirty=true }
            ComboBox { id: colorChoice; visible: kind.currentIndex===0 || kind.currentIndex===3; model: [qsTr("中性"),qsTr("蓝"),qsTr("绿"),qsTr("橙"),qsTr("紫")]; onActivated: root.dirty=true }
            TextField { id: description; visible: kind.currentIndex===1; Layout.fillWidth: true; placeholderText: qsTr("项目描述"); onTextEdited: root.dirty=true }
            TextField { id: projectStart; visible: kind.currentIndex===1; Layout.fillWidth: true; placeholderText: qsTr("项目开始 yyyy-MM-dd，可空"); onTextEdited: root.dirty=true }
            TextField { id: projectDeadline; visible: kind.currentIndex===1; Layout.fillWidth: true; placeholderText: qsTr("项目截止 yyyy-MM-dd，可空"); onTextEdited: root.dirty=true }
            Label { text: kind.currentIndex === 1 ? qsTr("所属领域（可选）") : qsTr("所属项目（分组必选）"); visible: kind.currentIndex === 1 || kind.currentIndex === 2 }
            ComboBox {
                id: parentChoice
                Layout.fillWidth: true
                visible: kind.currentIndex === 1 || kind.currentIndex === 2
                model: [qsTr("无归属")].concat(root.parents.map(function(item) { return item.title }))
                onActivated: root.dirty=true
            }
            RowLayout {
                visible: kind.currentIndex === 1
                ComboBox { id: planning; model: [qsTr("已激活"), qsTr("以后再说")]; onActivated: root.dirty=true }
                ComboBox { id: status; model: [qsTr("进行中"), qsTr("已完成"), qsTr("已取消")]; onActivated: root.dirty=true }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("完成项目会隐藏其未完成任务；恢复项目后重新显示。删除领域或项目保留任务并解除归属。"); color: Theme.secondary }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.errorText; color: Theme.danger }
            RowLayout {
                Button { text: qsTr("保存"); highlighted: true; onClicked: { if (root.selected.id && kind.currentIndex === 1 && status.currentIndex > 0) confirmComplete.open(); else root.save() } }
                Button { text: qsTr("新建"); onClicked: root.safely(function(){root.resetForm()}) }
                Button { text: qsTr("删除…"); enabled: !!root.selected.id; onClicked: confirmDelete.open() }
            }
            Item { Layout.fillHeight: true }
            }
        }
    }
    footer: DialogButtonBox {Button {text: qsTr("返回");enabled: !root.workspace.busy;onClicked: root.safely(function(){root.close()})}}
    Dialog {id: discard;modal: true;title: qsTr("放弃未保存的组织修改？");standardButtons: Dialog.Ok|Dialog.Cancel;onAccepted: {root.dirty=false;const action=root.pendingAction;root.pendingAction=null;if(action)action()}}
    Dialog {
        id: confirmDelete
        parent: Overlay.overlay
        width: Math.min(440,root.parent.width-40)
        height: Math.min(180*Theme.scale,root.parent.height-40)
        anchors.centerIn: parent
        title: qsTr("删除组织项？")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: qsTr("相关任务会保留，项目或领域归属会解除。确认继续？");wrapMode: Text.Wrap }
        onAccepted: root.workspace.deleteCatalog(root.selected.kind, root.selected.id, root.selected.rowVersion)
    }
    Dialog {
        id: confirmComplete
        parent: Overlay.overlay
        width: Math.min(440,root.parent.width-40)
        height: Math.min(180*Theme.scale,root.parent.height-40)
        anchors.centerIn: parent
        title: qsTr("结束项目？")
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label { text: qsTr("不会自动完成任务；开放任务从活动视图隐藏，可恢复项目后继续。");wrapMode: Text.Wrap }
        onAccepted: root.save()
    }
    Connections { target: root.workspace; function onOperationFinished(operation, success) { if (operation === "catalog" && success) root.resetForm() } }
}
