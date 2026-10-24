pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    background: Rectangle {color: Theme.sidebar;radius: 16;border.color: Theme.separator}
    objectName: "recurrenceDialog"
    required property TaskWorkspace workspace
    property var selected: ({})
    property bool dirty: false
    property var pendingAction: null
    function safely(action) {if(dirty){pendingAction=action;discard.open()}else action()}
    readonly property var frequencies: ["DAILY","WEEKLY","MONTHLY","YEARLY","AFTER_DAYS"]
    width: Math.min(740,parent.width-40)
    height: Math.min(650,parent.height-40)
    title: qsTr("重复任务规则")
    modal: true
    closePolicy: Popup.NoAutoClose
    function load(rule) {
        selected=rule
        ruleTitle.text=rule.title || ""
        notes.text=rule.notes || ""
        frequency.currentIndex=rule.frequency ? frequencies.indexOf(rule.frequency) : 0
        interval.value=rule.interval || 1
        anchor.text=rule.anchor || Qt.formatDate(new Date(),"yyyy-MM-dd")
        end.text=rule.end || ""
        monthDay.value=rule.monthDay || 1
        yearMonth.value=rule.yearMonth || 1
        for(let i=0;i<7;++i)(weekdays.itemAt(i) as CheckBox).checked=!!((rule.weekdays || 0)&(1<<i))
        active.checked=rule.active === undefined ? true : rule.active
        dirty=false
    }
    function save() {
        let mask=0
        for(let i=0;i<7;++i)if((weekdays.itemAt(i) as CheckBox).checked)mask|=1<<i
        workspace.saveRecurrence({id:selected.id || "",rowVersion:selected.rowVersion || 1,title:ruleTitle.text,notes:notes.text,
            frequency:frequencies[frequency.currentIndex],interval:interval.value,anchor:anchor.text,end:end.text,
            weekdays:mask,monthDay:monthDay.value,yearMonth:yearMonth.value,active:active.checked})
    }
    onOpened: load({})
    Shortcut {sequence: "Escape";enabled: root.visible && !discard.visible && !stopConfirm.visible && !anchor.pickerVisible && !end.pickerVisible && !frequency.popup.visible;onActivated: root.safely(function(){root.close()})}
    readonly property bool stacked: availableWidth<640*Theme.scale
    contentItem: GridLayout {
        columns: root.stacked ? 1 : 2
        columnSpacing: 20
        rowSpacing: Theme.fieldGap
        enabled: !root.workspace.busy || root.workspace.backgroundBusy
        ScrollView {
            id: ruleList
            Layout.preferredWidth: 180
            Layout.fillWidth: root.stacked
            Layout.preferredHeight: root.stacked ? Math.min(100,ruleColumn.implicitHeight) : -1
            Layout.fillHeight: !root.stacked
            clip: true
            ColumnLayout {
                id: ruleColumn
                width: ruleList.availableWidth-12
                ActionButton {symbol: "plus"; objectName: "newRecurrence";text: qsTr("新建规则"); onClicked: root.safely(function(){root.load({})}) }
                Repeater {
                    model: root.workspace.recurrenceRules
                    ItemDelegate {
                        required property var modelData
                        Layout.fillWidth: true
                        highlighted: root.selected.id===modelData.id
                        text: modelData.title
                        onClicked: root.safely(function(){root.load(modelData)})
                    }
                }
            }
        }
        ScrollView {
            id: ruleForm
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                width: ruleForm.availableWidth-12
                spacing: Theme.fieldGap
                SectionLabel {text: qsTr("重复规则")}
                Label { text: qsTr("规则产生独立任务；已完成历史和手动编辑的实例保留。"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                FormGroup {
                    heading: qsTr("任务模板")
                Label {text: qsTr("任务标题");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TextField { id: ruleTitle;Accessible.name: qsTr("重复任务标题");objectName: "recurrenceTitle"; Layout.fillWidth: true; placeholderText: qsTr("规则标题"); onTextEdited: root.dirty=true }
                TextField { id: notes;Accessible.name: qsTr("默认备注"); Layout.fillWidth: true; placeholderText: qsTr("默认备注"); onTextEdited: root.dirty=true }
                }
                FormGroup {
                    heading: qsTr("重复周期")
                ComboBox { id: frequency;Accessible.name: qsTr("重复周期类型");Layout.fillWidth: true;model: [qsTr("每N天"),qsTr("每N周指定星期"),qsTr("每N月指定日"),qsTr("每N年固定月日"),qsTr("完成后N天")]; enabled: !root.selected.id; onActivated: root.dirty=true }
                RowLayout { Label { text: qsTr("间隔N") } SpinBox { id: interval;Accessible.name: qsTr("重复间隔"); from: 1; to: 36500; editable: true; onValueModified: root.dirty=true } }
                Label { text: qsTr("起始日 / 结束日（留空不限）") }
                DateField { id: anchor;workspace: root.workspace;dateLabel: qsTr("规则起始日"); Layout.fillWidth: true; placeholderText: "yyyy-MM-dd"; onTextEdited: root.dirty=true;onDateChosen: root.dirty=true }
                DateField { id: end;workspace: root.workspace;dateLabel: qsTr("规则结束日"); Layout.fillWidth: true; placeholderText: "yyyy-MM-dd"; onTextEdited: root.dirty=true;onDateChosen: root.dirty=true }
                Flow {
                    Layout.fillWidth: true
                    visible: frequency.currentIndex===1
                    Repeater { id: weekdays; model: [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]; CheckBox { required property string modelData; text: modelData; onClicked: root.dirty=true } }
                }
                RowLayout {
                    visible: frequency.currentIndex===2 || frequency.currentIndex===3
                    Label { text: qsTr("日") }
                    SpinBox { id: monthDay;Accessible.name: qsTr("每月日期"); from: 1; to: 31; onValueModified: root.dirty=true }
                    Label { visible: frequency.currentIndex===3; text: qsTr("月") }
                    SpinBox { id: yearMonth;Accessible.name: qsTr("月份"); visible: frequency.currentIndex===3; from: 1; to: 12; onValueModified: root.dirty=true }
                }
                SettingSwitch { id: active;symbol: "reload"; text: qsTr("启用规则"); onClicked: root.dirty=true }
                Label { text: qsTr("固定规则补全最近30日至未来30日；无效月日跳过。编辑只更新明天起未手改的实例，周期类型需另建规则。"); Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                }
                FormGroup {
                    visible: !!root.selected.id
                    heading: qsTr("停止与清理")
                CheckBox { id: removeFuture;Layout.fillWidth: true;text: qsTr("停止规则时同时软删未来未完成实例") }
                ActionButton {symbol: "trash"; text: qsTr("停止并删除规则…"); palette.buttonText: Theme.danger; flat: true; enabled: !!root.selected.id; onClicked: stopConfirm.open() }
                }
                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.errorText; color: Theme.danger }
            }
        }
    }
    footer: DialogButtonBox {
        Button { text: qsTr("返回"); enabled: !root.workspace.busy; onClicked: root.safely(function(){root.close()}) }
        Button { text: qsTr("保存规则"); enabled: !root.workspace.busy; highlighted: true; onClicked: root.save() }
    }
    DiscardChangesDialog {id: discard;objectName: "recurrenceDiscard";parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(440,parent.width-32);modal: true;title: qsTr("放弃未保存的规则修改？");onRejected: root.pendingAction=null;onAccepted: {root.dirty=false;const action=root.pendingAction;root.pendingAction=null;if(action)action()}}
    Dialog { id: stopConfirm; modal: true; title: qsTr("停止产生新实例？"); standardButtons: Dialog.Ok|Dialog.Cancel; onAccepted: root.workspace.stopRecurrence(root.selected.id,root.selected.rowVersion,removeFuture.checked) }
    Connections { target: root.workspace; function onOperationFinished(operation,success){if(operation==="recurrence" && success){root.dirty=false;root.load({})}} }
}
