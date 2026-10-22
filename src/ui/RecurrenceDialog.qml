pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    required property TaskWorkspace workspace
    property var selected: ({})
    property bool dirty: false
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
    contentItem: RowLayout {
        enabled: !root.workspace.busy
        ScrollView {
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                width: 170
                Button { text: qsTr("新建规则"); onClicked: {if(root.dirty)discard.open();else root.load({})} }
                Repeater {
                    model: root.workspace.recurrenceRules
                    Button {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.title
                        onClicked: {if(root.dirty)discard.open();else root.load(modelData)}
                    }
                }
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ColumnLayout {
                width: root.availableWidth-210
                Label { text: qsTr("规则产生独立任务；已完成历史和手动编辑的实例保留。"); Layout.fillWidth: true; wrapMode: Text.Wrap }
                TextField { id: ruleTitle; Layout.fillWidth: true; placeholderText: qsTr("规则标题"); onTextEdited: root.dirty=true }
                TextField { id: notes; Layout.fillWidth: true; placeholderText: qsTr("默认备注"); onTextEdited: root.dirty=true }
                ComboBox { id: frequency; model: [qsTr("每N天"),qsTr("每N周指定星期"),qsTr("每N月指定日"),qsTr("每N年固定月日"),qsTr("完成后N天")]; enabled: !root.selected.id; onActivated: root.dirty=true }
                RowLayout { Label { text: qsTr("间隔N") } SpinBox { id: interval; from: 1; to: 36500; editable: true; onValueModified: root.dirty=true } }
                Label { text: qsTr("起始日 / 结束日（留空不限）") }
                TextField { id: anchor; Layout.fillWidth: true; placeholderText: "yyyy-MM-dd"; onTextEdited: root.dirty=true }
                TextField { id: end; Layout.fillWidth: true; placeholderText: "yyyy-MM-dd"; onTextEdited: root.dirty=true }
                Flow {
                    Layout.fillWidth: true
                    visible: frequency.currentIndex===1
                    Repeater { id: weekdays; model: [qsTr("一"),qsTr("二"),qsTr("三"),qsTr("四"),qsTr("五"),qsTr("六"),qsTr("日")]; CheckBox { required property string modelData; text: modelData; onClicked: root.dirty=true } }
                }
                RowLayout {
                    visible: frequency.currentIndex===2 || frequency.currentIndex===3
                    Label { text: qsTr("日") }
                    SpinBox { id: monthDay; from: 1; to: 31; onValueModified: root.dirty=true }
                    Label { visible: frequency.currentIndex===3; text: qsTr("月") }
                    SpinBox { id: yearMonth; visible: frequency.currentIndex===3; from: 1; to: 12; onValueModified: root.dirty=true }
                }
                CheckBox { id: active; text: qsTr("启用规则"); onClicked: root.dirty=true }
                Label { text: qsTr("固定规则补全最近30日至未来30日；无效月日跳过。编辑只更新明天起未手改的实例，周期类型需另建规则。"); Layout.fillWidth: true; wrapMode: Text.Wrap; font.pixelSize: 11*Theme.scale; color: Theme.secondary }
                CheckBox { id: removeFuture; text: qsTr("停止规则时同时软删未来未完成实例") }
                Button { text: qsTr("停止并删除规则…"); enabled: !!root.selected.id; onClicked: stopConfirm.open() }
                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.errorText; color: Theme.danger }
            }
        }
    }
    footer: DialogButtonBox {
        Button { text: qsTr("返回"); enabled: !root.workspace.busy; onClicked: {if(root.dirty)discard.open();else root.close()} }
        Button { text: qsTr("保存规则"); enabled: !root.workspace.busy; highlighted: true; onClicked: root.save() }
    }
    Dialog { id: discard; modal: true; title: qsTr("放弃未保存的修改并返回？"); standardButtons: Dialog.Ok|Dialog.Cancel; onAccepted: {root.dirty=false;root.close()} }
    Dialog { id: stopConfirm; modal: true; title: qsTr("停止产生新实例？"); standardButtons: Dialog.Ok|Dialog.Cancel; onAccepted: root.workspace.stopRecurrence(root.selected.id,root.selected.rowVersion,removeFuture.checked) }
    Connections { target: root.workspace; function onOperationFinished(operation,success){if(operation==="recurrence" && success){root.dirty=false;root.load({})}} }
}
