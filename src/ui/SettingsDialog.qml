pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import DaymarkStyle as DS
ApplicationWindow {
    id: root
    component SettingsComboBox: ComboBox {
        id: picker
        background: DS.FieldSurface {
            quietOutline: true
            focused: picker.visualFocus || picker.popup.visible
            hovered: picker.hovered || picker.down
            enabledField: picker.enabled
        }
    }
    objectName: "settingsDialog"
    signal accountRequested()
    signal dataRequested()
    signal helpRequested()
    signal calendarVisibilityRequested(bool visible)
    required property TaskWorkspace workspace
    required property WindowChromeController windowChrome
    WindowChrome {
        id: settingsChrome; objectName: "settingsChrome"
        window: root; controller: root.windowChrome; dark: Theme.dark
        titleBar: settingsTop
        buttonStrip: settingsCaption.item ? (settingsCaption.item as DaymarkTitleBar).buttonStrip : null

    }


    header: Item {
        id: settingsTop; objectName: "settingsIntegratedHeader"
        height: Math.max(56,settingsHeadingMetrics.height+4+settingsDescriptionMetrics.reservedHeight+24)
        FontMetrics {id: settingsHeadingMetrics; font: settingsCategoryTitle.font}
        Item {
            id: settingsDescriptionMetrics
            visible: false
            width: settingsHeader.width
            readonly property real reservedHeight: Math.max(appearanceMeasure.implicitHeight,widgetMeasure.implicitHeight,generalMeasure.implicitHeight,aboutMeasure.implicitHeight,tasksMeasure.implicitHeight,dataMeasure.implicitHeight,accountMeasure.implicitHeight)
            Text {id: appearanceMeasure; width: parent.width; text: root.categoryDetails[0];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: widgetMeasure; width: parent.width; text: root.categoryDetails[1];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: generalMeasure; width: parent.width; text: root.categoryDetails[2];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: aboutMeasure; width: parent.width; text: root.categoryDetails[3];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: tasksMeasure; width: parent.width; text: root.categoryDetails[4];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: dataMeasure; width: parent.width; text: root.categoryDetails[5];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
            Text {id: accountMeasure; width: parent.width; text: root.categoryDetails[6];font.family: Theme.fontFamily;font.pixelSize: Theme.captionSize;wrapMode: Text.Wrap;textFormat: Text.PlainText}
        }
        Rectangle {anchors.fill: parent; color: Theme.grouped}
        Rectangle {width: root.sidebarWidth; height: parent.height; color: Theme.sidebar
            Rectangle {anchors.right: parent.right; width: 1; height: parent.height; color: Theme.separator}
        }
        Label {x: 20; y: 12; text: qsTr("设置"); font.pixelSize: Theme.windowTitleSize; font.weight: Font.DemiBold}
        ColumnLayout {
            id:settingsHeader; x: root.sidebarWidth+16; y: 12; width: Math.max(0,root.width-x-16-54); spacing:4
            Label {id: settingsCategoryTitle;objectName:"settingsCategoryTitle";text:root.categoryTitles[root.categoryIndex];Layout.fillWidth:true;font.pixelSize:Theme.windowTitleSize;font.weight:Font.DemiBold;wrapMode:Text.Wrap}
            Label {text:root.categoryDetails[root.categoryIndex];Layout.fillWidth:true;font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap}
        }
        Loader {
            id: settingsCaption; objectName: "settingsCaption"
            anchors.top: parent.top; anchors.right: parent.right
            active: settingsChrome.customEnabled && root.visible && root.visibility !== Window.FullScreen
            visible: active; width: active && item ? (item as Item).implicitWidth : 0
            height: active ? Math.max(36,settingsHeadingMetrics.height+24) : 0
            sourceComponent: DaymarkTitleBar {chrome: settingsChrome; targetWindow: root; surface: Theme.grouped}
        }
    }

    property var updateController: null
    required property WidgetViewModel widgetController
    required property StartupController startupController
    // Stable category IDs are independent from sidebar row order.
    property int categoryIndex: 2
    readonly property var categoryOrder:[2,6,4,1,0,5,3]
    readonly property var categoryTitles:[qsTr("外观"),qsTr("桌面组件"),qsTr("常规"),qsTr("关于与帮助"),qsTr("任务与提醒"),qsTr("数据与隐私"),qsTr("账号与同步")]
    readonly property var categorySymbols:["eye","window","settings","info","bell","backup","account"]
    readonly property var categoryDetails:[qsTr("主题、文字与辅助功能"),qsTr("任务组件与桌面日历"),qsTr("启动、关闭与系统托盘"),qsTr("版本、更新与使用帮助"),qsTr("快速添加与到期提醒"),qsTr("备份、使用统计与本机日志"),qsTr("账号、设备资料与同步状态")]
    property var categoryScroll:[0,0,0,0,0,0,0]
    property bool restoringScroll:false
    readonly property real sidebarWidth:Math.min(220,176*Theme.scale)
    readonly property real availableWidth:width-sidebarWidth-32
    title:qsTr("设置 · Daymark")
    visible:false
    flags:Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint
    color:Theme.grouped
    font.family:Theme.fontFamily;font.pixelSize:Theme.bodySize
    palette.window:Theme.content;palette.text:Theme.text;palette.windowText:Theme.text;palette.base:Theme.content;palette.buttonText:Theme.text;palette.highlight:Theme.accent
    minimumWidth:640;minimumHeight:420
    width:Math.min(860*Theme.scale,Math.max(minimumWidth,transientParent ? transientParent.width-32 : Screen.width-48),Math.max(minimumWidth,Screen.width-48))
    height:Math.min(680*Theme.scale,Math.max(minimumHeight,transientParent ? transientParent.height-32 : Screen.height-48),Math.max(minimumHeight,Screen.height-48))
    function open(){x=Math.max(Screen.virtualX,Math.min(transientParent ? transientParent.x+(transientParent.width-width)/2 : x,Screen.virtualX+Screen.width-width));y=Math.max(Screen.virtualY,Math.min(transientParent ? transientParent.y+(transientParent.height-height)/2 : y,Screen.virtualY+Screen.height-height));restoringScroll=true;setVisible(true);requestActivate()}
    function close(){const view=viewport(),saved=categoryScroll.slice();if(view)saved[categoryIndex]=view.contentY;categoryScroll=saved;hide();if(transientParent)transientParent.requestActivate()}
    function viewport(){return body.contentItem as Flickable}
    function focusCategory(){const index=categoryOrder.indexOf(categoryIndex);categories.positionViewAtIndex(index,ListView.Contain);const row=categories.itemAtIndex(index) as Button;if(row)row.forceActiveFocus()}
    function setVerificationCategory(index) { selectCategory(index) }
    function selectCategory(index) {
        const next=Number(index);if(categoryOrder.indexOf(next)<0 || next===categoryIndex)return
        const view=viewport(),saved=categoryScroll.slice();if(view)saved[categoryIndex]=view.contentY;categoryScroll=saved
        restoringScroll=true;categoryIndex=next
    }
    function showOverview(){focusCategory()}
    function activateVerificationCategory(index) {
        const rowIndex=categoryOrder.indexOf(index);categories.positionViewAtIndex(rowIndex,ListView.Contain);const row=categories.itemAtIndex(rowIndex) as Button;if(!row)return false;row.clicked();return true
    }
    function navigateCategory(index){setVerificationCategory(index);Qt.callLater(focusCategory)}
    function verifyCategory() {
        const selected=[appearance.item,widgetOptions.item,general.item,about.item,tasks.item,dataPrivacy.item,account.item][categoryIndex] as Item
        if(categoryIndex<0 || !selected || !selected.visible || selected.width<=0 || selected.height<=0)return "category content missing"
        if(sidebar.width<148 || settingsHeader.height>=root.height || body.height<80 || contentPane.x<sidebar.width-1)return "split settings navigation clipped"
        if(categoryIndex===4 && !(tasks.item as TaskPageContent).systemReminderVisible)return "system reminder preference missing"
        for(let i=0;i<categoryOrder.length;++i){const row=categories.itemAtIndex(i) as Button;if(!row || row.checked!==(categoryOrder[i]===categoryIndex))return "settings sidebar selection mismatch"}
        return "PASS"
    }
    function reveal(item) {
        const view=viewport();if(!view || !item)return
        let owner=item;while(owner && owner!==pages)owner=owner.parent;if(!owner)return
        restoringScroll=false
        const at=item.mapToItem(pages,0,0),extent=Math.min(item.height,view.height)
        if(at.y<view.contentY)view.contentY=Math.max(0,at.y-8)
        else if(at.y+extent>view.contentY+view.height)view.contentY=Math.min(Math.max(0,view.contentHeight-view.height),at.y+extent-view.height+8)
    }
    Timer {id:focusReveal;interval:0;onTriggered:root.reveal(root.activeFocusItem)}
    Connections {target:root;function onActiveFocusItemChanged(){focusReveal.restart()}}
    Connections {target:root.viewport();function onContentHeightChanged(){if(!root.restoringScroll)focusReveal.restart()}}
    Binding {
        target:root.viewport();property:"contentY";when:root.restoringScroll && !!root.viewport();restoreMode:Binding.RestoreNone
        value:{const view=root.viewport();return view ? Math.min(root.categoryScroll[root.categoryIndex],Math.max(0,view.contentHeight-view.height)) : 0}
    }
    Connections {target:root.viewport();function onMovementStarted(){root.restoringScroll=false}}
    onClosing:function(event){event.accepted=false;root.close()}
    Shortcut {sequence:"Escape";enabled:root.visible;onActivated:root.close()}
    RowLayout {
        anchors.fill:parent;spacing:0
        Rectangle {
            id:sidebar;objectName:"settingsSidebar";Layout.preferredWidth:root.sidebarWidth;Layout.fillHeight:true;color:Theme.sidebar
            Rectangle {anchors.right:parent.right;width:1;height:parent.height;color:Theme.separator}
            ColumnLayout {
                anchors.fill:parent;anchors.margins:12;spacing:12
                ListView {
                    id:categories;objectName:"settingsCategories";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
                    model:root.categoryOrder;currentIndex:root.categoryOrder.indexOf(root.categoryIndex);boundsBehavior:Flickable.StopAtBounds
                    delegate:Button {
                        id:categoryRow
                        required property int index
                        required property int modelData
                        objectName:"settingsCategory-"+modelData;width:categories.width
                        text:root.categoryTitles[modelData];checkable:true;autoExclusive:true;checked:root.categoryIndex===modelData;activeFocusOnTab:checked
                        implicitHeight:Math.max(36,categoryLabel.implicitHeight+14);padding:8
                        Accessible.name:text;Accessible.description:root.categoryDetails[modelData]
                        onClicked:root.selectCategory(modelData)
                        Keys.onPressed:function(event){
                            if(event.key===Qt.Key_Up){root.navigateCategory(root.categoryOrder[Math.max(0,index-1)]);event.accepted=true}
                            else if(event.key===Qt.Key_Down){root.navigateCategory(root.categoryOrder[Math.min(root.categoryOrder.length-1,index+1)]);event.accepted=true}
                            else if(event.key===Qt.Key_Home){root.navigateCategory(root.categoryOrder[0]);event.accepted=true}
                            else if(event.key===Qt.Key_End){root.navigateCategory(root.categoryOrder[root.categoryOrder.length-1]);event.accepted=true}
                        }
                        contentItem:RowLayout {
                            spacing:8
                            Rectangle {Layout.preferredWidth:24;Layout.preferredHeight:24;radius:Theme.controlRadius;color:Theme.highContrast ? Theme.hover : Qt.rgba(categoryIcon.ink.r,categoryIcon.ink.g,categoryIcon.ink.b,Theme.dark ? 0.2 : 0.1)
                                AppIcon {id:categoryIcon;anchors.centerIn:parent;width:16;height:16;name:root.categorySymbols[categoryRow.modelData];ink:categoryRow.modelData===0 ? Theme.accent : categoryRow.modelData===1 ? Theme.success : categoryRow.modelData===2 ? Theme.secondary : Theme.catalogColor("purple")}
                            }
                            Label {id:categoryLabel;text:categoryRow.text;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.bodySize;font.weight:categoryRow.checked ? Font.DemiBold : Font.Normal}
                        }
                        background:Rectangle {radius:Theme.rowRadius;color:categoryRow.down ? Theme.pressed : categoryRow.checked ? Theme.navigationSelection : categoryRow.hovered ? Theme.hover : "transparent";border.width:categoryRow.visualFocus ? 1 : 0;border.color:Theme.accent}
                    }
                }
            }
        }
        ColumnLayout {
            id:contentPane;objectName:"settingsContentPane";Layout.fillWidth:true;Layout.minimumWidth:0;Layout.fillHeight:true;Layout.margins:16;spacing:12

            ScrollView {
                id:body;objectName:"settingsScroll";Layout.fillWidth:true;Layout.minimumWidth:0;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
                ColumnLayout {
                    id:pages;width:body.availableWidth;spacing:Theme.blockGap
                Loader {id:appearance;visible:active;active:root.visible && root.categoryIndex===0;Layout.fillWidth:true;sourceComponent:AppearancePageContent {}}
                Loader {id:widgetOptions;visible:active;active:root.visible && root.categoryIndex===1;Layout.fillWidth:true;sourceComponent:ColumnLayout {
                    id:widgetOptionsPage;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("组件与形态")
                        SettingsToggle {text:qsTr("显示任务组件");checked:root.widgetController.visible;onClicked:root.widgetController.setVisible(checked)}
                        SettingsToggle {objectName:"widgetPanelEnabled";text:qsTr("启用小窗");checked:((root.widgetController.enabledForms & 1)!==0);onClicked:root.widgetController.setFormEnabled(0,checked)}
                        SettingsToggle {objectName:"widgetMiniEnabled";text:qsTr("启用 Mini 条");checked:((root.widgetController.enabledForms & 2)!==0);onClicked:root.widgetController.setFormEnabled(1,checked)}
                        SettingsToggle {objectName:"widgetBallEnabled";text:qsTr("启用悬浮球");checked:((root.widgetController.enabledForms & 4)!==0);onClicked:root.widgetController.setFormEnabled(2,checked)}
                        SettingsRow {
                            label:qsTr("常驻形态")
                            SettingsComboBox {
                                objectName:"widgetFormSelection"
                                readonly property var forms: {const mask=root.widgetController.enabledForms;return [0,1,2].filter(function(form){return (mask & (1<<form))!==0})}
                                Layout.fillWidth:true;enabled:forms.length>0
                                Accessible.name:qsTr("任务组件常驻形态")
                                model:forms.map(function(form){return [qsTr("小窗"),qsTr("Mini 条"),qsTr("悬浮球")][form]})
                                currentIndex:forms.indexOf(root.widgetController.selectedForm)
                                onActivated:root.widgetController.selectForm(forms[currentIndex])
                            }
                        }
                        Label {visible:root.widgetController.formCount===0;text:qsTr("未启用任何形态，组件已隐藏");Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.secondary}
                        SettingsLink {visible:root.widgetController.formCount===0;symbol:"reset";text:qsTr("恢复默认形态");onClicked:{root.widgetController.setFormEnabled(0,true);root.widgetController.setFormEnabled(1,true);root.widgetController.selectForm(0)}}
                    }
                    SettingsGroup {
                        heading:qsTr("背景与收展")
                        SettingsToggle {objectName:"widgetSolidBackgroundSetting";text:qsTr("使用不透明背景");enabled:!Theme.highContrast;checked:root.workspace.preferences.solidWidget;onClicked:root.workspace.setPreference("solidWidget",checked)}
                        SettingsRow {
                            objectName:"widgetOpacitySetting";label:qsTr("背景透明度");separator:true
                            enabled:!Theme.highContrast && !root.workspace.preferences.solidWidget
                            RowLayout {
                                Layout.fillWidth:true
                                Slider {Layout.fillWidth:true;Accessible.name:qsTr("组件背景透明度");enabled:!Theme.highContrast;from:0.6;to:1;value:root.widgetController.backgroundOpacity;onMoved:root.widgetController.setBackgroundOpacity(value)}
                                Label {text:Math.round(root.widgetController.backgroundOpacity*100)+"%";font.pixelSize:Theme.captionSize;Layout.minimumWidth:42}
                            }
                        }
                        SettingsToggle {objectName:"miniAutoCollapseSetting";separator:false;text:qsTr("球与 Mini 条自动收展");enabled:((root.widgetController.enabledForms & 2)!==0) && ((root.widgetController.enabledForms & 4)!==0);checked:root.widgetController.miniAutoCollapse;onClicked:root.widgetController.setAutoCompact(checked)}
                    }
                    SettingsGroup {
                        heading:qsTr("分类检索")
                        detail:qsTr("小窗标题下的“分类”入口始终可用；今天入口不受此范围影响。")
                        SettingsRow {
                            label:qsTr("分类任务范围")
                            SettingsComboBox {objectName:"widgetCategoryRangeSetting";Layout.fillWidth:true;Accessible.name:qsTr("小窗分类任务范围");model:[qsTr("全部未完成"),qsTr("仅今天")];currentIndex:root.workspace.widgetCategoryToday?1:0;enabled:root.workspace.ready && !root.workspace.busy;onActivated:{root.workspace.requestWidgetCategoryToday(currentIndex===1);currentIndex=Qt.binding(function(){return root.workspace.widgetCategoryToday?1:0})}}
                        }
                        SettingsToggle {objectName:"widgetShowShortcutsSetting";separator:false;text:qsTr("显示常用分类");checked:root.workspace.widgetShowShortcuts;onClicked:root.workspace.setWidgetShowShortcuts(checked)}
                      }
                      SettingsGroup {
                        heading:qsTr("层级与交互")
                        detail:qsTr("鼠标穿透可从托盘或这里关闭；重启后恢复为可点击。")
                        SettingsRow {
                            label:qsTr("窗口层级")
                            SettingsComboBox {objectName:"widgetLayerSelection";Layout.fillWidth:true;Accessible.name:qsTr("任务组件窗口层级");model:[qsTr("普通"),qsTr("始终置顶"),qsTr("桌面层")];currentIndex:root.widgetController.layer;onActivated:root.widgetController.setLayer(currentIndex)}
                        }
                        SettingsToggle {text:qsTr("鼠标穿透");enabled:root.widgetController.trayAvailable;checked:root.widgetController.clickThrough;onClicked:root.widgetController.setClickThrough(checked)}
                        SettingsToggle {text:qsTr("锁定拖动");checked:root.widgetController.locked;onClicked:root.widgetController.setLocked(checked)}
                        SettingsLink {separator:false;disclosure:false;symbol:"reset";text:qsTr("重置组件位置");onClicked:root.widgetController.resetPosition()}
                    }
                    SettingsGroup {
                        heading:qsTr("独立桌面日历")
                        SettingsToggle {text:qsTr("显示桌面日历");checked:root.workspace.preferences.desktopCalendarVisible;onClicked:{root.calendarVisibilityRequested(checked);checked=Qt.binding(function(){return root.workspace.preferences.desktopCalendarVisible})}}
                        SettingsRow {
                            label:qsTr("背景透明度")
                            RowLayout {
                                Layout.fillWidth:true
                                Slider {Layout.fillWidth:true;Accessible.name:qsTr("日历背景透明度");enabled:!Theme.highContrast;from:50;to:100;stepSize:5;value:root.workspace.preferences.desktopCalendarOpacity;onMoved:root.workspace.setPreference("desktopCalendarOpacity",Math.round(value))}
                                Label {text:root.workspace.preferences.desktopCalendarOpacity+"%";font.pixelSize:Theme.captionSize;Layout.minimumWidth:42}
                            }
                        }
                        SettingsToggle {text:qsTr("日历保持置顶");checked:root.workspace.preferences.desktopCalendarTopmost;onClicked:root.workspace.setPreference("desktopCalendarTopmost",checked)}
                        SettingsToggle {separator:false;objectName:"calendarDesktopPinnedSetting";text:qsTr("固定在桌面");checked:root.workspace.preferences.desktopCalendarPinned;onClicked:root.workspace.setPreference("desktopCalendarPinned",checked)}
                    }
                }}
                Loader {id:general;visible:active;active:root.visible && root.categoryIndex===2;Layout.fillWidth:true;sourceComponent:GeneralPageContent {}}
                Loader {id:tasks;visible:active;active:root.visible && root.categoryIndex===4;Layout.fillWidth:true;sourceComponent:TaskPageContent {}}
                Loader {id:dataPrivacy;visible:active;active:root.visible && root.categoryIndex===5;Layout.fillWidth:true;sourceComponent:DataPrivacyPageContent {}}
                Loader {id:account;visible:active;active:root.visible && root.categoryIndex===6;Layout.fillWidth:true;sourceComponent:AccountPageContent {}}
                Loader {id:about;visible:active;active:root.visible && root.categoryIndex===3;Layout.fillWidth:true;sourceComponent:SettingsAboutPage {id:aboutPage;updateController:root.updateController;Layout.fillWidth:true;onHelpRequested:root.helpRequested()}}
                }
            }
            Label {visible:root.widgetController.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.widgetController.errorText}
            Label {visible:root.workspace.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.workspace.errorText}
            Label {visible:root.windowChrome.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.windowChrome.errorText}
            RowLayout {
                Layout.fillWidth:true
                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:(root.workspace.errorText || root.windowChrome.errorText) ? qsTr("设置未保存，请重试") : qsTr("更改立即保存");font.pixelSize:Theme.captionSize;color:(root.workspace.errorText || root.windowChrome.errorText) ? Theme.danger : Theme.secondary}
                Button {objectName:"settingsClose";text:qsTr("关闭");flat:true;onClicked:root.close()}
            }
        }
    }
    component AppearancePageContent:ColumnLayout {
                    id:appearancePage;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("主题与显示")

                        SettingsRow {
                            label:qsTr("主题")
                            SegmentedPicker {objectName:"themeModePicker";model:[qsTr("系统"),qsTr("浅色"),qsTr("深色")];selectionIsExternal:true;currentIndex:root.workspace.themeMode;onActivated:function(index){root.workspace.themeMode=index}}
                        }
                        SettingsRow {
                            label:qsTr("文字大小")
                            RowLayout {
                                Layout.fillWidth:true
                                Slider {Layout.fillWidth:true;Accessible.name:qsTr("文字大小");from:1;to:2;stepSize:0.25;value:root.workspace.textScale;onMoved:root.workspace.textScale=value}
                                Label {text:Math.round(root.workspace.textScale*100)+"%";font.pixelSize:Theme.captionSize;Layout.minimumWidth:42}
                            }
                        }
                        SettingsToggle {separator:false;text:qsTr("紧凑界面");Accessible.description:qsTr("统一导航、列表、表单、菜单和桌面组件的间距，不改变文字大小");checked:root.workspace.preferences.compact;onClicked:root.workspace.setPreference("compact",checked)}
                    }
                    SettingsGroup {
                        heading:qsTr("窗口")
                        visible:root.windowChrome.supported
                        SettingsRow {
                            visible: root.windowChrome.supported
                            separator:false;label: qsTr("标题栏")
                            SegmentedPicker {
                                objectName: "titleBarModePicker"
                                model: [qsTr("Daymark"), qsTr("系统原生")]
                                selectionIsExternal: true
                                currentIndex: root.windowChrome.mode === 1 ? 0 : 1
                                onActivated: function(index) {root.windowChrome.requestMode(index === 0 ? 1 : 0)}
                            }
                        }
                    }
                    SettingsGroup {
                        heading:qsTr("辅助功能")
                        detail:Theme.highContrast ? qsTr("增强文字及边界对比，桌面组件使用实色背景；关闭后恢复原透明度。") : ""
                        SettingsToggle {text:qsTr("跟随系统");checked:root.workspace.preferences.followSystemAccessibility;onClicked:root.workspace.setPreference("followSystemAccessibility",checked)}
                        SettingsToggle {objectName:"reduceMotionPreference";text:qsTr("减少动态效果");checked:root.workspace.preferences.followSystemAccessibility ? root.workspace.systemAccessibility.reduceMotion : root.workspace.preferences.reduceMotion;onClicked:root.workspace.setPreference("reduceMotion",checked)}
                        SettingsToggle {separator:false;objectName:"highContrastPreference";text:qsTr("增强对比度");checked:Theme.highContrast;onClicked:root.workspace.setPreference("highContrast",checked)}
                    }
                }
    component GeneralPageContent:ColumnLayout {
        Layout.fillWidth:true;spacing:Theme.blockGap
        SettingsGroup {
                        heading:qsTr("启动与关闭")
                        detail:root.startupController.supported ? qsTr("移动程序后需重新开启开机启动。") : qsTr("当前平台暂不支持开机启动。")
                        SettingsToggle {text:qsTr("开机启动（当前用户）");enabled:root.startupController.supported;checked:root.startupController.enabled;onClicked:root.startupController.setEnabled(checked)}
                        SettingsToggle {separator:false;text:qsTr("关闭主窗口到托盘");enabled:root.widgetController.trayAvailable;checked:root.widgetController.closeToTray;onClicked:root.widgetController.setCloseToTray(checked)}
                    }
        Label {visible:root.startupController.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.startupController.errorText}

    }
    component AccountPageContent:ColumnLayout {
        id:accountPage
        readonly property var accountState:root.workspace.syncState
        readonly property bool connected:!!accountState.authenticated
        Layout.fillWidth:true;spacing:Theme.blockGap
        SettingsGroup {
            heading:qsTr("账号")
            detail:accountPage.connected ? qsTr("任务先保存在此设备，再同步到当前账号服务。") : qsTr("未登录时仅保存在此设备，登录后可在设备间同步。")
            SettingsRow {label:qsTr("当前账号"); Label {objectName:"settingsAccountIdentity";Layout.fillWidth:true;wrapMode:Text.Wrap;font.weight:Font.DemiBold;text:accountPage.connected ? (accountPage.accountState.login_label || qsTr("已登录")) : qsTr("尚未登录")}}
            SettingsLink {objectName:"settingsManageAccount";separator:false;symbol:"account";text:accountPage.connected ? qsTr("管理账号与设备资料…") : qsTr("登录或创建账号…");onClicked:root.accountRequested()}
        }
        SettingsGroup {
            heading:qsTr("同步")
            SettingsRow {label:qsTr("状态"); Label {objectName:"settingsSyncStatus";Layout.fillWidth:true;wrapMode:Text.Wrap;text:accountPage.accountState.status || qsTr("仅存储在此设备");color:accountPage.accountState.safe_error_code ? Theme.danger : Theme.secondary;Accessible.name:text}}
            Label {visible:(accountPage.accountState.pending_count || 0)>0;Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12;wrapMode:Text.Wrap;text:qsTr("%1 组修改保留在此设备，等待同步。").arg(accountPage.accountState.pending_count || 0);color:Theme.secondary}
            SettingsLink {objectName:"settingsSyncNow";separator:false;symbol:"reset";text:accountPage.accountState.busy ? qsTr("正在同步…") : qsTr("立即同步");enabled:accountPage.connected && !accountPage.accountState.busy;onClicked:root.workspace.syncNow()}
        }
    }
    component TaskPageContent:ColumnLayout {
        id:tasksPage;readonly property bool systemReminderVisible:systemReminders.visible && systemReminders.width>0
        Layout.fillWidth:true;spacing:Theme.blockGap
        SettingsGroup {
                        heading:qsTr("快速添加")
                        detail:qsTr("识别标题中的明确日期和时刻，保存时添加安排或提醒；原始标题保留。")
                        SettingsToggle {objectName:"recognizeTaskTimePreference";text:qsTr("识别日期和时间");checked:root.workspace.preferences.recognizeTaskTime;onClicked:root.workspace.setPreference("recognizeTaskTime",checked)}
                        SettingsToggle {objectName:"recognitionDateReminderPreference";text:qsTr("只有日期时也提醒");enabled:root.workspace.preferences.recognizeTaskTime;checked:root.workspace.preferences.recognitionDateReminder;onClicked:root.workspace.setPreference("recognitionDateReminder",checked)}
                        SettingsRow {
                            label:qsTr("默认提醒时刻");separator:false
                            enabled:root.workspace.preferences.recognizeTaskTime && root.workspace.preferences.recognitionDateReminder
                            TimeField {objectName:"recognitionReminderTimePreference";Layout.fillWidth:true;text:root.workspace.preferences.recognitionReminderTime;timeLabel:qsTr("日期任务默认提醒时刻");onTimeChosen:root.workspace.setPreference("recognitionReminderTime",text);onEditingFinished:if(!root.workspace.setPreference("recognitionReminderTime",text))text=root.workspace.preferences.recognitionReminderTime}
                        }
                    }
        SettingsGroup {
                        heading:qsTr("提醒")
                        detail:qsTr("应用运行时显示到期弹窗，可完成或延后；系统通知另受系统权限控制。")
                        SettingsToggle {objectName:"reminderPopupsPreference";text:qsTr("到期弹窗");checked:root.workspace.preferences.reminderPopups;onClicked:root.workspace.setPreference("reminderPopups",checked)}
                        SettingsToggle {separator:false;id:systemReminders;objectName:"systemRemindersPreference";text:qsTr("系统通知");checked:root.workspace.preferences.systemReminders;onClicked:root.workspace.setPreference("systemReminders",checked)}
                    }
    }
    component DataPrivacyPageContent:ColumnLayout {
        id:dataPrivacyPage;property string diagnosticError:""
        Layout.fillWidth:true;spacing:Theme.blockGap
        SettingsGroup {
                        heading:qsTr("备份与迁移")
                        detail:qsTr("启动时检查，保留最近7份一致性快照。")
                        SettingsRow {
                            label:qsTr("备份周期")
                            SettingsComboBox {objectName:"backupDaysPicker";Layout.fillWidth:true;Accessible.name:qsTr("自动备份周期");model:[qsTr("每天"),qsTr("每7天"),qsTr("每30天")];currentIndex:[1,7,30].indexOf(root.workspace.preferences.backupDays);onActivated:root.workspace.setPreference("backupDays",[1,7,30][currentIndex])}
                        }
                        SettingsLink {separator:false;objectName:"settingsData";symbol:"backup";text:qsTr("导入、导出与备份");onClicked:root.dataRequested()}
                    }
        SettingsGroup {
                        objectName:"telemetryPrivacyGroup"
                        heading:qsTr("使用统计")
                        Label {
                            objectName:"telemetryPrivacyDescription"
                            Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12;Layout.topMargin:10;Layout.bottomMargin:8
                            text:qsTr("自愿分享应用版本、使用的界面和性能区间，不含任务、分类、标签或输入内容。仅发往当前账号服务，并关联账号和设备以支持撤回。原始记录保留7天，汇总保留90天；关闭后停止发送并请求清理。")
                            font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap;textFormat:Text.PlainText
                        }
                        SettingsToggle {
                            objectName:"telemetryConsentPreference"
                            text:qsTr("分享使用情况与性能统计")
                            enabled:root.workspace.syncState.authenticated
                            checked:root.workspace.telemetryState.enabled
                            onClicked:root.workspace.setTelemetryEnabled(checked)
                        }
                        Label {objectName:"telemetryConsentStatus";Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12;Layout.bottomMargin:10;text:root.workspace.telemetryState.status;font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap;textFormat:Text.PlainText}
                        SettingsLink {objectName:"telemetryConsentRetry";visible:(root.workspace.telemetryState.error_key || "").length>0;enabled:!root.workspace.telemetryState.busy;separator:false;text:qsTr("重试连接");symbol:"reset";onClicked:root.workspace.retryTelemetry()}
                    }
        SettingsGroup {
            heading:qsTr("本机日志")
            detail:qsTr("日志只保存在此设备，自动轮换，不会自动上传；与自愿分享的使用统计独立。")
            SettingsLink {objectName:"settingsOpenLogs";separator:false;disclosure:false;symbol:"folder";text:qsTr("打开日志目录");onClicked:dataPrivacyPage.diagnosticError=root.workspace.openDiagnosticLogs()?"":qsTr("无法打开日志目录，请检查本地目录权限。")}
            Label {visible:dataPrivacyPage.diagnosticError.length>0;Layout.fillWidth:true;Layout.leftMargin:12;Layout.rightMargin:12;wrapMode:Text.Wrap;text:dataPrivacyPage.diagnosticError;color:Theme.danger}
        }
    }
}
