pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: gallery
    objectName: "styleGallery"
    title: qsTr("Daymark · 界面设计基准")
    modal: true
    width: Math.min(740,parent.width-40)
    height: Math.min(650,parent.height-32)
    standardButtons: Dialog.Close
    function showState(state) {if(state==="menu")sampleMenu.open();else if(state==="choice")choice.popup.open();else if(state==="focus")field.forceActiveFocus()}
    function verifyActions() {
        if(iconOnly.Accessible.name!==qsTr("搜索任务") || iconOnly.symbol!=="search" || createAction.symbol!=="plus" || createAction.text.length===0 || createAction.contentItem.implicitWidth<=0)return "action metadata"
        return "PASS"
    }
    function dismissPopups(){sampleMenu.close();choice.popup.close()}
    function verifyControls() {
        const options=choice.popup.contentItem as ListView
        const option=options.itemAtIndex(2) as MenuItem
        if(!option)return "missing option"
        option.clicked()
        if(choice.currentIndex!==2 || choice.popup.visible)return "selection="+choice.currentIndex+", popup="+choice.popup.visible
        checkedOption.toggle()
        if(checkedOption.checked)return "checkbox"
        checkedOption.toggle()
        countInput.increase()
        if(countInput.value!==8)return "spinbox="+countInput.value
        countInput.decrease()
        field.forceActiveFocus()
        return "PASS"
    }
    onClosed: dismissPopups()
    contentItem: ScrollView {
        clip: true
        ColumnLayout {
            width: gallery.availableWidth-12
            spacing: Theme.fieldGap
            SectionLabel {text: qsTr("操作层级")}
            Flow {
                Layout.fillWidth: true;spacing: Theme.gap
                Button {text: qsTr("普通操作")}
                Button {text: qsTr("主要操作");highlighted: true}
                Button {text: qsTr("轻量操作");flat: true}
                Button {text: qsTr("删除");palette.buttonText: Theme.danger}
                Button {text: qsTr("不可用");enabled: false}
                IconToolButton {
                    Accessible.name: qsTr("更多")
                    contentItem: AppIcon {name: "more"}
                    onClicked: sampleMenu.open()
                }
            }
            Flow {
                Layout.fillWidth: true;spacing: Theme.gap
                ActionButton {id: createAction;symbol: "plus";text: qsTr("新建事件");highlighted: true}
                ActionButton {symbol: "edit";text: qsTr("编辑整个系列…")}
                ActionButton {symbol: "export";text: qsTr("导出JSON…")}
                WidgetAction {id: iconOnly;symbol: "search";description: qsTr("搜索任务")}
                WidgetAction {symbol: "filter";description: qsTr("日历显示选项");checked: true}
                WidgetAction {symbol: "reload";description: qsTr("重新载入");enabled: false}
            }
            SectionLabel {text: qsTr("输入与选择")}
            TextField {id: field;Layout.fillWidth: true;placeholderText: qsTr("统一输入框、焦点轮廓和占位文字")}
            ComboBox {id: choice;Layout.fillWidth: true;model: [qsTr("选择一个选项"),qsTr("今天"),qsTr("明天"),qsTr("以后再说")]}
            Flow {
                Layout.fillWidth: true;spacing: Theme.gap
                CheckBox {text: qsTr("未勾选")}
                CheckBox {id: checkedOption;text: qsTr("已勾选");checked: true}
                CheckBox {text: qsTr("禁用");enabled: false}
            }
            RowLayout {
                Layout.fillWidth: true
                Slider {Layout.fillWidth: true;value: 0.6}
                SpinBox {id: countInput;from: 1;to: 30;value: 7;editable: true}
            }
            SectionLabel {text: qsTr("信息与反馈")}
            Label {text: qsTr("正文承载主要信息，次级文字说明上下文。");Layout.fillWidth: true;wrapMode: Text.Wrap}
            Label {text: qsTr("次级说明 · 12px");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            Label {text: qsTr("操作已保存");color: Theme.success}
            Label {text: qsTr("请检查输入后重试，原内容已保留。");color: Theme.danger;Layout.fillWidth: true;wrapMode: Text.Wrap}
        }
    }
    Menu {
        id: sampleMenu
        MenuItem {text: qsTr("编辑详情")}
        MenuItem {text: qsTr("保持置顶");checkable: true;checked: true}
        MenuSeparator {}
        MenuItem {text: qsTr("不可用操作");enabled: false}
    }
}
