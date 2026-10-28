pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS
Dialog {
    id: gallery
    objectName: "styleGallery"
    title: qsTr("Daymark · 界面设计基准")
    modal: true
    width: Math.min(740,parent.width-40)
    height: Math.min(650,parent.height-32)
    standardButtons: Dialog.Close
    function showState(state) {if(state==="menu")sampleMenu.open();else if(state==="choice")choice.popup.open();else if(state==="focus")field.forceActiveFocus()}
    function showVisualAudit(kind) {
        Qt.callLater(function(){
            const target=kind==="icons" || kind==="clip" ? glyphMatrix : typeSamples;
            const view=galleryScroll.contentItem as Flickable;
            if(view)view.contentY=Math.max(0,Math.min(Math.max(0,view.contentHeight-view.height),target.mapToItem(galleryContent,0,0).y-(kind==="clip" ? view.height+6 : 0)));
        })
    }
    function verifyVisualSystem() {
        if(Theme.titleSize<=Theme.headingSize || Theme.headingSize<=Theme.bodySize || Theme.bodySize<=Theme.captionSize || Theme.captionSize<=Theme.footnoteSize)return "type hierarchy";
        if(Theme.controlRadius>=Theme.menuRadius || Theme.menuRadius>=Theme.groupRadius || Theme.groupRadius>=Theme.panelRadius)return "surface hierarchy";
        return verifyMenuGroups();
    }
    property bool menuAuditFirst:true
    property bool menuAuditSecond:true
    property bool menuAuditLast:true
    function verifyMenuGroups() {
        sampleMenu.open()
        const cases=[[true,true,true,2],[true,false,true,1],[false,true,true,1],[true,true,false,1],[false,false,true,0],[false,false,false,0]]
        let failure="PASS"
        for(const state of cases){
            menuAuditFirst=state[0];menuAuditSecond=state[1];menuAuditLast=state[2];sampleMenu.updateGroups()
            let rules=0
            for(let i=0;i<sampleMenu.count;++i){const item=sampleMenu.itemAt(i);if(item instanceof DS.MenuSeparator && item.visible)++rules}
            if(sampleMenu.verifyGrouping()!=="PASS" || rules!==state[3]){failure="conditional menu groups: "+state+" rules="+rules+" / "+sampleMenu.verifyGrouping();break}
        }
        menuAuditFirst=true;menuAuditSecond=true;menuAuditLast=true;sampleMenu.updateGroups()
        sampleMenu.close()
        return failure
    }
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
        id:galleryScroll
        clip: true
        ColumnLayout {
            id:galleryContent
            width: galleryScroll.availableWidth
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
            Label {text: qsTr("次级说明 · %1 DIP").arg(Theme.captionSize);font.pixelSize: Theme.captionSize;color: Theme.secondary}
            Label {text: qsTr("操作已保存");color: Theme.success}
            Label {text: qsTr("请检查输入后重试，原内容已保留。");color: Theme.danger;Layout.fillWidth: true;wrapMode: Text.Wrap}
            ColumnLayout {
                id:typeSamples;Layout.fillWidth:true;spacing:8
                SectionLabel {text:qsTr("文字角色 · 系统字体")}
                Repeater {
                    model:[{label:qsTr("页面标题"),size:Theme.pageTitleSize},{label:qsTr("普通标题"),size:Theme.titleSize},{label:qsTr("面板与任务标题"),size:Theme.headingSize},{label:qsTr("列表任务"),size:Theme.listTitleSize},{label:qsTr("正文与菜单"),size:Theme.bodySize},{label:qsTr("辅助信息"),size:Theme.captionSize},{label:qsTr("时间刻度"),size:Theme.footnoteSize}]
                    Label {required property var modelData;text:modelData.label+" · Daymark 012345";font.pixelSize:modelData.size;font.weight:modelData.size>=Theme.headingSize ? Font.DemiBold : Font.Normal;Layout.fillWidth:true;wrapMode:Text.Wrap}
                }
            }
            SectionLabel {text:qsTr("统一矢量图标")}
            Flow {
                id:glyphMatrix;Layout.fillWidth:true;spacing:8
                Repeater {
                    model:Object.keys(DS.IconPaths.paths)
                    ColumnLayout {
                        id:glyphCell
                        required property string modelData
                        width:Math.max(80,Theme.captionSize*7);height:Math.max(56,Theme.captionSize*4)
                        AppIcon {objectName:"galleryGlyph-"+glyphCell.modelData;name:glyphCell.modelData;Layout.alignment:Qt.AlignHCenter;Layout.preferredWidth:Theme.iconSize;Layout.preferredHeight:Theme.iconSize}
                        Label {text:glyphCell.modelData;font.pixelSize:Theme.footnoteSize;Layout.fillWidth:true;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideRight}
                    }
                }
            }
        }
    }
    DS.Menu {
        id: sampleMenu
        MenuItem {text: qsTr("编辑详情");visible:gallery.menuAuditFirst}
        DS.MenuSeparator {}
        MenuItem {text: qsTr("保持置顶");visible:gallery.menuAuditSecond;checkable: true;checked: true}
        DS.MenuSeparator {}
        MenuItem {text: qsTr("不可用操作");visible:gallery.menuAuditLast;enabled: false}
    }
}
