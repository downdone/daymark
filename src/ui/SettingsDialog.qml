pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ApplicationWindow {
    id: root
    objectName: "settingsDialog"
    signal quitRequested()
    signal dataRequested()
    signal helpRequested()
    signal calendarVisibilityRequested(bool visible)
    required property TaskWorkspace workspace
    required property WidgetViewModel widgetController
    required property StartupController startupController
    property int categoryIndex: -1
    readonly property var categoryTitles:[qsTr("外观"),qsTr("桌面组件"),qsTr("通用"),qsTr("关于")]
    readonly property var categorySymbols:["eye","window","settings","info"]
    readonly property var categoryDetails:[qsTr("主题、文字与辅助功能"),qsTr("任务小窗、迷你条与桌面日历"),qsTr("启动、提醒与备份"),qsTr("版本与交流反馈")]
    property real overviewY:0
    property int lastCategory:0
    title: (categoryIndex<0 ? qsTr("设置") : categoryTitles[categoryIndex])+qsTr(" · Daymark")
    visible:false
    flags:Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint
    color:Theme.grouped
    font.family:Theme.fontFamily;font.pixelSize:Theme.bodySize
    palette.window:Theme.content;palette.text:Theme.text;palette.windowText:Theme.text;palette.base:Theme.content;palette.buttonText:Theme.text;palette.highlight:Theme.accent
    readonly property real availableWidth:width-32
    readonly property real availableHeight:height-64
    width:Math.min(600*Theme.scale,transientParent ? transientParent.width-32 : 1000)
    height:Math.min(640*Theme.scale,transientParent ? transientParent.height-32 : 900)
    function open(){x=transientParent ? transientParent.x+(transientParent.width-width)/2 : x;y=transientParent ? transientParent.y+(transientParent.height-height)/2 : y;show();requestActivate()}
    function close(){hide();if(transientParent)transientParent.requestActivate()}
    function viewport(){return body.contentItem as Flickable}
    function setVerificationCategory(index) {
        const view=viewport();if(categoryIndex<0 && view)overviewY=view.contentY;
        categoryIndex=Math.max(0,Math.min(3,index));lastCategory=categoryIndex;
        Qt.callLater(function(){const v=root.viewport();if(v)v.contentY=0;settingsBack.forceActiveFocus()})
    }
    function showOverview() {
        categoryIndex=-1;
        Qt.callLater(function(){const v=root.viewport();if(v)v.contentY=Math.min(root.overviewY,Math.max(0,v.contentHeight-v.height));const row=categories.itemAt(root.lastCategory) as Button;if(row)row.forceActiveFocus()})
    }
    function verifyCategory() {
        if(categoryIndex<0)return overview.visible && overview.width>0 ? "PASS" : "settings overview missing";
        const selected=categoryIndex===0 ? appearance : categoryIndex===1 ? widgetOptions : categoryIndex===2 ? general : about;
        if(!selected.visible || selected.width<=0 || selected.height<=0)return "category content missing";
        if(!settingsBack.visible || settingsHeader.height>=root.height || body.height<80)return "category navigation clipped";
        if(categoryIndex===2 && (!systemReminders.visible || systemReminders.width<=0))return "system reminder preference missing";
        return "PASS";
    }
    function activateVerificationCategory(index) {
        showOverview();const row=categories.itemAt(index) as Button;
        if(!row)return false;row.clicked();return true;
    }
    function reveal(item) {
        const view=viewport();if(!view || !item)return;
        let owner=item;while(owner && owner!==pages)owner=owner.parent;
        if(!owner)return;
        const at=item.mapToItem(pages,0,0),extent=Math.min(item.height,view.height);
        if(at.y<view.contentY)view.contentY=Math.max(0,at.y-8);
        else if(at.y+extent>view.contentY+view.height)view.contentY=Math.min(Math.max(0,view.contentHeight-view.height),at.y+extent-view.height+8);
    }
    Connections {target:root;function onActiveFocusItemChanged(){Qt.callLater(function(){root.reveal(root.activeFocusItem)})}}
    onClosing:function(event){event.accepted=false;root.close()}
    Shortcut {sequence:"Escape";enabled:root.visible;onActivated:root.categoryIndex>=0 ? root.showOverview() : root.close()}
    footer:ToolBar {
        implicitHeight:settingsFooterRow.implicitHeight+16
        background:Rectangle{color:Theme.grouped}
        RowLayout {
            id:settingsFooterRow;anchors.fill:parent;anchors.margins:8
            Label {Layout.fillWidth:true;text:root.workspace.errorText ? qsTr("设置未保存，请重试") : qsTr("更改立即保存");font.pixelSize:Theme.captionSize;color:root.workspace.errorText ? Theme.danger : Theme.secondary}
            Button {text:qsTr("关闭");flat:true;onClicked:root.close()}
        }
    }
    ColumnLayout {
        anchors.fill:parent;anchors.margins:16;spacing:12
        RowLayout {
            id:settingsHeader;Layout.fillWidth:true;spacing:8
            WidgetAction {id:settingsBack;objectName:"settingsBack";visible:root.categoryIndex>=0;symbol:"left";description:qsTr("返回设置");onClicked:root.showOverview()}
            Label {text:root.categoryIndex<0 ? qsTr("设置") : root.categoryTitles[root.categoryIndex];Layout.fillWidth:true;font.pixelSize:root.categoryIndex<0 ? Theme.titleSize : Theme.headingSize;font.weight:Font.DemiBold;wrapMode:Text.Wrap}
        }
        ScrollView {
            id:body;objectName:"settingsScroll";Layout.fillWidth:true;Layout.fillHeight:true
            contentWidth:availableWidth;clip:true
            ColumnLayout {
                id:pages;width:body.availableWidth;spacing:Theme.blockGap
                ColumnLayout {
                    id:overview;objectName:"settingsOverview";visible:root.categoryIndex<0;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("偏好设置")
                        Repeater {
                            id:categories;model:root.categoryTitles
                            SettingsLink {
                                required property int index
                                required property string modelData
                                objectName:"settingsCategory-"+index
                                text:modelData;detail:root.categoryDetails[index];symbol:root.categorySymbols[index]
                                iconColor:index===0 ? Theme.accent : index===1 ? Theme.success : index===2 ? Theme.secondary : Theme.catalogColor("purple")
                                separator:index<3
                                onClicked:root.setVerificationCategory(index)
                            }
                        }
                    }
                    Label {text:qsTr("Daymark · 本地保存，无需账号");Layout.fillWidth:true;Layout.leftMargin:12;color:Theme.secondary;font.pixelSize:Theme.captionSize;wrapMode:Text.Wrap}
                }
                ColumnLayout {
                    id:appearance;visible:root.categoryIndex===0;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("显示")
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
                        heading:qsTr("辅助功能")
                        detail:Theme.highContrast ? qsTr("增强文字及边界对比，桌面组件使用实色背景；关闭后恢复原透明度。") : ""
                        SettingsToggle {text:qsTr("跟随系统");checked:root.workspace.preferences.followSystemAccessibility;onClicked:root.workspace.setPreference("followSystemAccessibility",checked)}
                        SettingsToggle {objectName:"reduceMotionPreference";text:qsTr("减少动态效果");checked:root.workspace.preferences.followSystemAccessibility ? root.workspace.systemAccessibility.reduceMotion : root.workspace.preferences.reduceMotion;onClicked:root.workspace.setPreference("reduceMotion",checked)}
                        SettingsToggle {separator:false;objectName:"highContrastPreference";text:qsTr("增强对比度");checked:Theme.highContrast;onClicked:root.workspace.setPreference("highContrast",checked)}
                    }
                }
                ColumnLayout {
                    id:widgetOptions;visible:root.categoryIndex===1;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("任务组件 · 显示")
                        SettingsToggle {text:qsTr("显示任务组件");checked:root.widgetController.visible;onClicked:root.widgetController.setVisible(checked)}
                        SettingsRow {
                            label:qsTr("展示形态")
                            ComboBox {Layout.fillWidth:true;Accessible.name:qsTr("任务组件展示形态");model:[qsTr("小窗"),qsTr("迷你条")];currentIndex:root.widgetController.mini ? 1 : 0;onActivated:root.widgetController.setMini(currentIndex===1)}
                        }
                        SettingsToggle {objectName:"widgetSolidBackgroundSetting";text:qsTr("使用不透明背景");enabled:!Theme.highContrast;checked:root.workspace.preferences.solidWidget;onClicked:root.workspace.setPreference("solidWidget",checked)}
                        SettingsRow {
                            objectName:"widgetOpacitySetting";label:qsTr("背景透明度");separator:false
                            enabled:!Theme.highContrast && !root.workspace.preferences.solidWidget
                            RowLayout {
                                Layout.fillWidth:true
                                Slider {Layout.fillWidth:true;Accessible.name:qsTr("组件背景透明度");enabled:!Theme.highContrast;from:0.6;to:1;value:root.widgetController.backgroundOpacity;onMoved:root.widgetController.setBackgroundOpacity(value)}
                                Label {text:Math.round(root.widgetController.backgroundOpacity*100)+"%";font.pixelSize:Theme.captionSize;Layout.minimumWidth:42}
                            }
                        }
                    }
                    SettingsGroup {
                        heading:qsTr("任务组件 · 窗口与交互")
                        detail:qsTr("鼠标穿透可从托盘或这里关闭；重启后恢复为可点击。")
                        SettingsToggle {text:qsTr("保持置顶");checked:root.widgetController.alwaysOnTop;onClicked:root.widgetController.setAlwaysOnTop(checked)}
                        SettingsToggle {objectName:"widgetDesktopPinnedSetting";text:qsTr("固定在桌面");checked:root.widgetController.desktopPinned;onClicked:root.widgetController.setDesktopPinned(checked)}
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
                }
                ColumnLayout {
                    id:general;visible:root.categoryIndex===2;Layout.fillWidth:true;spacing:Theme.blockGap
                    SettingsGroup {
                        heading:qsTr("启动与关闭")
                        detail:root.startupController.supported ? qsTr("移动程序后需重新开启开机启动。") : qsTr("当前平台暂不支持开机启动。")
                        SettingsToggle {text:qsTr("开机启动（当前用户）");enabled:root.startupController.supported;checked:root.startupController.enabled;onClicked:root.startupController.setEnabled(checked)}
                        SettingsToggle {separator:false;text:qsTr("关闭主窗口到托盘");enabled:root.widgetController.trayAvailable;checked:root.widgetController.closeToTray;onClicked:root.widgetController.setCloseToTray(checked)}
                    }
                    Label {visible:root.startupController.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.startupController.errorText}
                    SettingsGroup {
                        heading:qsTr("提醒")
                        detail:qsTr("应用运行时检查提醒；系统通知取决于权限，触发记录保留在本机。")
                        SettingsToggle {separator:false;id:systemReminders;objectName:"systemRemindersPreference";text:qsTr("系统通知");checked:root.workspace.preferences.systemReminders;onClicked:root.workspace.setPreference("systemReminders",checked)}
                    }
                    SettingsGroup {
                        heading:qsTr("自动备份")
                        detail:qsTr("启动时检查，保留最近7份一致性快照。")
                        SettingsRow {
                            label:qsTr("备份周期");separator:false
                            ComboBox {objectName:"backupDaysPicker";Layout.fillWidth:true;Accessible.name:qsTr("自动备份周期");model:[qsTr("每天"),qsTr("每7天"),qsTr("每30天")];currentIndex:[1,7,30].indexOf(root.workspace.preferences.backupDays);onActivated:root.workspace.setPreference("backupDays",[1,7,30][currentIndex])}
                        }
                    }
                    SettingsGroup {
                        heading:qsTr("资料与帮助")
                        SettingsLink {objectName:"settingsData";symbol:"backup";text:qsTr("数据与备份");onClicked:root.dataRequested()}
                        SettingsLink {separator:false;objectName:"settingsHelp";symbol:"info";text:qsTr("使用提示");onClicked:root.helpRequested()}
                    }
                    SettingsGroup {
                        detail:root.widgetController.trayAvailable ? qsTr("托盘可打开主窗口、恢复穿透或彻底退出。") : qsTr("系统托盘不可用；关闭主窗口将退出，穿透已禁用。")
                        SettingsLink {separator:false;destructive:true;symbol:"close";text:qsTr("退出 Daymark");onClicked:root.quitRequested()}
                    }
                }
                SettingsAboutPage {id:about;visible:root.categoryIndex===3;Layout.fillWidth:true}
            }
        }
        Label {visible:root.widgetController.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.widgetController.errorText}
        Label {visible:root.workspace.errorText.length>0;Layout.fillWidth:true;wrapMode:Text.Wrap;color:Theme.danger;text:root.workspace.errorText}
    }
}
