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
    signal syncLoginRequested(string origin,string login,string password)
    signal syncRegisterRequested(string origin,string login,string password)
    property bool syncRegisterMode:false
    onSyncFormOpenChanged:if(syncFormOpen){syncOrigin.text=root.workspace.syncState.server_origin || "";syncLogin.text=root.workspace.syncState.login_label || ""}
    signal syncProfileRequested(string id)
    signal syncResolveRequested(string id,string decision)
    signal syncRebuildRequested()
    property bool syncFormOpen:false
    required property TaskWorkspace workspace
    required property WidgetViewModel widgetController
    required property StartupController startupController
    property int categoryIndex: 0
    readonly property var categoryTitles:[qsTr("外观"),qsTr("桌面组件"),qsTr("通用"),qsTr("关于")]
    readonly property var categorySymbols:["eye","window","settings","info"]
    readonly property var categoryDetails:[qsTr("主题、文字与辅助功能"),qsTr("任务小窗、迷你条与桌面日历"),qsTr("启动、提醒与备份"),qsTr("版本与交流反馈")]
    property var categoryScroll:[0,0,0,0]
    property bool restoringScroll:false
    readonly property real sidebarWidth:Math.min(220,176*Theme.scale)
    readonly property real availableWidth:width-sidebarWidth-32
    readonly property real availableHeight:height-64
    title:qsTr("设置 · Daymark")
    visible:false
    flags:Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint
    color:Theme.grouped
    font.family:Theme.fontFamily;font.pixelSize:Theme.bodySize
    palette.window:Theme.content;palette.text:Theme.text;palette.windowText:Theme.text;palette.base:Theme.content;palette.buttonText:Theme.text;palette.highlight:Theme.accent
    minimumWidth:640;minimumHeight:420
    width:Math.min(860*Theme.scale,Math.max(minimumWidth,transientParent ? transientParent.width-32 : Screen.width-48),Math.max(minimumWidth,Screen.width-48))
    height:Math.min(680*Theme.scale,Math.max(minimumHeight,transientParent ? transientParent.height-32 : Screen.height-48),Math.max(minimumHeight,Screen.height-48))
    function open(){x=Math.max(Screen.virtualX,Math.min(transientParent ? transientParent.x+(transientParent.width-width)/2 : x,Screen.virtualX+Screen.width-width));y=Math.max(Screen.virtualY,Math.min(transientParent ? transientParent.y+(transientParent.height-height)/2 : y,Screen.virtualY+Screen.height-height));setVisible(true);requestActivate()}
    function close(){hide();if(transientParent)transientParent.requestActivate()}
    function viewport(){return body.contentItem as Flickable}
    function focusCategory(){const row=categories.itemAtIndex(categoryIndex) as Button;if(row)row.forceActiveFocus()}
    function setVerificationCategory(index) {
        const next=Math.max(0,Math.min(3,index));if(next===categoryIndex)return
        const view=viewport(),saved=categoryScroll.slice();if(view)saved[categoryIndex]=view.contentY;categoryScroll=saved
        restoringScroll=true;categoryIndex=next
    }
    function showOverview(){focusCategory()}
    function activateVerificationCategory(index) {
        const row=categories.itemAtIndex(index) as Button;if(!row)return false;row.clicked();return true
    }
    function navigateCategory(index){setVerificationCategory(index);Qt.callLater(focusCategory)}
    function verifyCategory() {
        const selected=categoryIndex===0 ? appearance : categoryIndex===1 ? widgetOptions : categoryIndex===2 ? general : about
        if(categoryIndex<0 || !selected.visible || selected.width<=0 || selected.height<=0)return "category content missing"
        if(sidebar.width<148 || settingsHeader.height>=root.height || body.height<80 || contentPane.x<sidebar.width-1)return "split settings navigation clipped"
        if(categoryIndex===2 && (!systemReminders.visible || systemReminders.width<=0))return "system reminder preference missing"
        for(let i=0;i<4;++i){const row=categories.itemAtIndex(i) as Button;if(!row || row.checked!==(i===categoryIndex))return "settings sidebar selection mismatch"}
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
    Connections {target:root;function onActiveFocusItemChanged(){Qt.callLater(function(){root.reveal(root.activeFocusItem)})}}
    Binding {
        target:root.viewport();property:"contentY";when:root.restoringScroll && !!root.viewport();restoreMode:Binding.RestoreNone
        value:{const view=root.viewport();return view ? Math.min(root.categoryScroll[root.categoryIndex],Math.max(0,view.contentHeight-view.height)) : 0}
    }
    Connections {target:root.viewport();function onMovementStarted(){root.restoringScroll=false}}
    onClosing:function(event){event.accepted=false;root.close()}
    onVisibleChanged:if(!visible){syncPassword.clear();syncConfirmPassword.clear();syncRegisterMode=false}
    onSyncRegisterModeChanged:{syncPassword.clear();syncConfirmPassword.clear()}
    Connections {target:root.workspace;function onSyncStateChanged(){if(root.workspace.syncState.authenticated){root.syncFormOpen=false;root.syncRegisterMode=false}}}
    Shortcut {sequence:"Escape";enabled:root.visible;onActivated:root.close()}
    RowLayout {
        anchors.fill:parent;spacing:0
        Rectangle {
            id:sidebar;objectName:"settingsSidebar";Layout.preferredWidth:root.sidebarWidth;Layout.fillHeight:true;color:Theme.sidebar
            Rectangle {anchors.right:parent.right;width:1;height:parent.height;color:Theme.separator}
            ColumnLayout {
                anchors.fill:parent;anchors.margins:12;spacing:12
                Label {text:qsTr("设置");font.pixelSize:Theme.headingSize;font.weight:Font.DemiBold;Layout.leftMargin:8;Layout.topMargin:6;Layout.fillWidth:true}
                ListView {
                    id:categories;objectName:"settingsCategories";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:4
                    model:root.categoryTitles;currentIndex:root.categoryIndex;boundsBehavior:Flickable.StopAtBounds
                    delegate:Button {
                        id:categoryRow
                        required property int index
                        required property string modelData
                        objectName:"settingsCategory-"+index;width:categories.width
                        text:modelData;checkable:true;autoExclusive:true;checked:root.categoryIndex===index;activeFocusOnTab:checked
                        implicitHeight:Math.max(36,categoryLabel.implicitHeight+14);padding:8
                        Accessible.name:modelData;Accessible.description:root.categoryDetails[index]
                        onClicked:root.setVerificationCategory(index)
                        Keys.onPressed:function(event){
                            if(event.key===Qt.Key_Up){root.navigateCategory(Math.max(0,root.categoryIndex-1));event.accepted=true}
                            else if(event.key===Qt.Key_Down){root.navigateCategory(Math.min(3,root.categoryIndex+1));event.accepted=true}
                            else if(event.key===Qt.Key_Home){root.navigateCategory(0);event.accepted=true}
                            else if(event.key===Qt.Key_End){root.navigateCategory(3);event.accepted=true}
                        }
                        contentItem:RowLayout {
                            spacing:8
                            Rectangle {Layout.preferredWidth:24;Layout.preferredHeight:24;radius:Theme.controlRadius;color:Theme.highContrast ? Theme.hover : Qt.rgba(categoryIcon.ink.r,categoryIcon.ink.g,categoryIcon.ink.b,Theme.dark ? 0.2 : 0.1)
                                AppIcon {id:categoryIcon;anchors.centerIn:parent;width:16;height:16;name:root.categorySymbols[categoryRow.index];ink:categoryRow.index===0 ? Theme.accent : categoryRow.index===1 ? Theme.success : categoryRow.index===2 ? Theme.secondary : Theme.catalogColor("purple")}
                            }
                            Label {id:categoryLabel;text:categoryRow.modelData;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:Theme.bodySize;font.weight:categoryRow.checked ? Font.DemiBold : Font.Normal}
                        }
                        background:Rectangle {radius:Theme.rowRadius;color:categoryRow.down ? Theme.pressed : categoryRow.checked ? Theme.selected : categoryRow.hovered ? Theme.hover : "transparent";border.width:categoryRow.visualFocus ? 1 : 0;border.color:Theme.accent}
                    }
                }
                Label {text:qsTr("本地保存 · 无需账号");Layout.fillWidth:true;Layout.leftMargin:8;wrapMode:Text.Wrap;font.pixelSize:Theme.captionSize;color:Theme.secondary}
            }
        }
        ColumnLayout {
            id:contentPane;objectName:"settingsContentPane";Layout.fillWidth:true;Layout.fillHeight:true;Layout.margins:16;spacing:12
            ColumnLayout {
                id:settingsHeader;Layout.fillWidth:true;spacing:4
                Label {objectName:"settingsCategoryTitle";text:root.categoryTitles[root.categoryIndex];Layout.fillWidth:true;font.pixelSize:Theme.headingSize;font.weight:Font.DemiBold;wrapMode:Text.Wrap}
                Label {text:root.categoryDetails[root.categoryIndex];Layout.fillWidth:true;font.pixelSize:Theme.captionSize;color:Theme.secondary;wrapMode:Text.Wrap}
            }
            ScrollView {
                id:body;objectName:"settingsScroll";Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
                ColumnLayout {
                    id:pages;width:body.availableWidth;spacing:Theme.blockGap
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
                        heading:qsTr("账号与同步")
                        detail:root.workspace.syncState.status || qsTr("仅存储在此设备")
                        ColumnLayout {
                            Layout.fillWidth:true;Layout.margins:12;spacing:12
                            Label {visible:!!root.workspace.syncState.login_label;Layout.fillWidth:true;wrapMode:Text.Wrap;text:(root.workspace.syncState.login_label || "")+"\n"+(root.workspace.syncState.server_origin || "");font.pixelSize:Theme.bodySize}
                            ColumnLayout {
                                visible:root.syncFormOpen && !root.workspace.syncState.authenticated;Layout.fillWidth:true;spacing:8
                                TextField {id:syncOrigin;objectName:"syncServerOrigin";Layout.fillWidth:true;placeholderText:qsTr("服务器地址 · https://sync.example.com");Accessible.name:qsTr("同步服务器地址");selectByMouse:true;enabled:!root.workspace.syncState.busy}
                                TextField {id:syncLogin;objectName:"syncLogin";Layout.fillWidth:true;placeholderText:qsTr("账号");Accessible.name:qsTr("账号");selectByMouse:true;enabled:!root.workspace.syncState.busy}
                                TextField {id:syncPassword;objectName:"syncPassword";Layout.fillWidth:true;placeholderText:qsTr("密码");Accessible.name:qsTr("密码");echoMode:TextInput.Password;enabled:!root.workspace.syncState.busy;onAccepted:syncSubmit.clicked()}
                                TextField {id:syncConfirmPassword;objectName:"syncConfirmPassword";visible:root.syncRegisterMode;Layout.fillWidth:true;placeholderText:qsTr("再次输入密码");Accessible.name:qsTr("确认密码");echoMode:TextInput.Password;enabled:!root.workspace.syncState.busy;onAccepted:syncSubmit.clicked()}
                                Label {visible:root.syncRegisterMode;Layout.fillWidth:true;wrapMode:Text.Wrap;text:syncConfirmPassword.text.length>0 && syncConfirmPassword.text!==syncPassword.text ? qsTr("两次密码不一致。") : qsTr("账号使用3～254位英文字母、数字或 . _ @ + -；密码至少15个字符。");font.pixelSize:Theme.captionSize;color:Theme.secondary}
                                Button {id:syncSubmit;objectName:"syncSubmit";text:root.workspace.syncState.busy ? qsTr("正在连接…") : root.syncRegisterMode ? qsTr("注册并同步") : qsTr("登录并同步");enabled:!root.workspace.syncState.busy && syncOrigin.text.trim().length>0 && syncLogin.text.trim().length>0 && syncPassword.text.length>0 && (!root.syncRegisterMode || (Array.from(syncPassword.text).length>=15 && syncConfirmPassword.text===syncPassword.text));onClicked:{if(!enabled)return;if(root.syncRegisterMode)root.syncRegisterRequested(syncOrigin.text,syncLogin.text,syncPassword.text);else root.syncLoginRequested(syncOrigin.text,syncLogin.text,syncPassword.text);syncPassword.clear();syncConfirmPassword.clear()}}
                                Button {objectName:"syncRegisterMode";text:root.syncRegisterMode ? qsTr("已有账号？登录") : qsTr("没有账号？注册");enabled:!root.workspace.syncState.busy;onClicked:{root.syncRegisterMode=!root.syncRegisterMode;syncLogin.forceActiveFocus()}}
                                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("登录后将合并此设备与服务器的资料。");font.pixelSize:Theme.captionSize;color:Theme.secondary}
                            }
                            Button {visible:!root.syncFormOpen && !root.workspace.syncState.authenticated;text:root.workspace.syncState.mode==="ACCOUNT" ? qsTr("重新登录") : qsTr("登录账号");enabled:!root.workspace.syncState.busy;onClicked:{root.syncFormOpen=true;syncOrigin.forceActiveFocus()}}
                            RowLayout {
                                visible:!!root.workspace.syncState.authenticated;Layout.fillWidth:true
                                Button {objectName:"syncNow";text:root.workspace.syncState.busy ? qsTr("正在同步…") : qsTr("立即同步");enabled:!root.workspace.syncState.busy;onClicked:root.workspace.syncNow()}
                                Button {objectName:"syncLogout";text:qsTr("退出账号");onClicked:root.workspace.syncLogout()}
                            }
                            Button {objectName:"syncRebuild";visible:root.workspace.syncState.phase==="RECONCILE_REQUIRED" && !!root.workspace.syncState.authenticated;text:qsTr("备份并重新合并");enabled:!root.workspace.syncState.busy;onClicked:root.syncRebuildRequested()}
                            Label {visible:(root.workspace.syncState.pending_count || 0)>0;Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("%1 组修改保留在此设备，等待同步。").arg(root.workspace.syncState.pending_count || 0);font.pixelSize:Theme.captionSize;color:Theme.secondary}
                            Repeater {
                                model:root.workspace.syncState.conflicts || []
                                delegate:ColumnLayout {
                                    id:syncConflict
                                    required property var modelData
                                    Layout.fillWidth:true;spacing:8
                                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("修改冲突：%1").arg(syncConflict.modelData.local_title || syncConflict.modelData.remote_title || qsTr("相关资料"));font.pixelSize:Theme.bodySize}
                                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("此选择处理 %1 项相关修改，两个版本保留在恢复记录中。").arg(syncConflict.modelData.related_count || 1);font.pixelSize:Theme.captionSize;color:Theme.secondary}
                                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("此设备\n%1").arg(syncConflict.modelData.local_summary || "");font.pixelSize:Theme.captionSize}
                                    Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:qsTr("服务器\n%1").arg(syncConflict.modelData.remote_summary || "");font.pixelSize:Theme.captionSize}
                                    Flow {
                                        Layout.fillWidth:true;spacing:8
                                        Button {text:qsTr("保留此设备");enabled:!root.workspace.syncState.busy;onClicked:root.syncResolveRequested(syncConflict.modelData.id,"local")}
                                        Button {text:qsTr("使用服务器版本");enabled:!root.workspace.syncState.busy && syncConflict.modelData.remote_available!==false;onClicked:root.syncResolveRequested(syncConflict.modelData.id,"remote")}
                                    }
                                }
                            }
                            ComboBox {id:syncProfiles;objectName:"syncProfiles";visible:count>1;Layout.fillWidth:true;Accessible.name:qsTr("本地账号资料");model:root.workspace.syncState.profiles || [];textRole:"label";onActivated:root.syncProfileRequested(model[currentIndex].id)}
                        }
                    }
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
            RowLayout {
                Layout.fillWidth:true
                Label {Layout.fillWidth:true;wrapMode:Text.Wrap;text:root.workspace.errorText ? qsTr("设置未保存，请重试") : qsTr("更改立即保存");font.pixelSize:Theme.captionSize;color:root.workspace.errorText ? Theme.danger : Theme.secondary}
                Button {objectName:"settingsClose";text:qsTr("关闭");flat:true;onClicked:root.close()}
            }
        }
    }
}
