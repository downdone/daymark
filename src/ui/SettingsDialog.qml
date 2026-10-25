pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    objectName: "settingsDialog"
    signal quitRequested()
    signal calendarVisibilityRequested(bool visible)
    required property TaskWorkspace workspace
    required property WidgetViewModel widgetController
    required property StartupController startupController
    property alias categoryIndex: categories.currentIndex
    title: qsTr("设置")
    width: Math.min(560*Theme.scale,parent.width-32)
    height: Math.min(480*Theme.scale,parent.height-32)
    modal: true
    standardButtons: Dialog.Close
    function setVerificationCategory(index) {
        categories.currentIndex=index
        const viewport=body.contentItem as Flickable
        if(viewport)viewport.contentY=0
    }
    function verifyCategory() {
        const selected=categories.currentIndex===0 ? appearance : categories.currentIndex===1 ? widgetOptions : general
        if(!selected.visible || selected.width<=0 || selected.height<=0)return "category content missing"
        if(categories.y+categories.height>root.availableHeight)return "category navigation clipped"
        if(categories.currentIndex===2 && (!systemReminders.visible || systemReminders.width<=0))return "system reminder preference missing"
        return "PASS"
    }
    function activateVerificationCategory(index) {return categories.activateForVerification(index)}
    contentItem: ColumnLayout {
        spacing: 10
        SegmentedPicker {
            id: categories
            objectName: "settingsCategories"
            Layout.fillWidth: false
            Layout.preferredWidth: Math.min(root.availableWidth,340*Theme.scale)
            Layout.alignment: Qt.AlignHCenter
            model: [qsTr("外观"),qsTr("桌面组件"),qsTr("通用")]
            onActivated: function(index){root.setVerificationCategory(index)}
        }
        ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                width: body.availableWidth-4
                spacing: 10
                FormGroup {
                    id: appearance
                    visible: categories.currentIndex===0
                    heading: qsTr("外观与辅助功能")

                    FormRow {
                        label: qsTr("主题")
                        SegmentedPicker {objectName: "themeModePicker";model: [qsTr("系统"),qsTr("浅色"),qsTr("深色")];selectionIsExternal: true;currentIndex: root.workspace.themeMode;onActivated: function(index){root.workspace.themeMode=index}}
                    }
                    FormRow {
                        label: qsTr("文字大小")
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {Layout.fillWidth: true;Accessible.name: qsTr("文字大小");from: 1;to: 2;stepSize: 0.25;value: root.workspace.textScale;onMoved: root.workspace.textScale=value}
                            Label {text: Math.round(root.workspace.textScale*100)+"%";font.pixelSize: Theme.captionSize;Layout.minimumWidth: 42}
                        }
                    }
                    SettingSwitch {showSymbol: false;objectName: "reduceMotionPreference";Layout.fillWidth: true;text: qsTr("减少动态效果");checked: root.workspace.preferences.reduceMotion;onClicked: root.workspace.setPreference("reduceMotion",checked)}
                    SettingSwitch {showSymbol: false;id: contrastPreference;objectName: "highContrastPreference";Layout.fillWidth: true;symbol: "eye";text: qsTr("增强对比度");checked: root.workspace.preferences.highContrast;onClicked: root.workspace.setPreference("highContrast",checked)}
                    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;visible: root.workspace.preferences.highContrast;text: qsTr("增强文字及边界对比，桌面组件使用实色背景；关闭后恢复原透明度。")}
                    SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "list";text: qsTr("紧凑任务列表");checked: root.workspace.preferences.compact;onClicked: root.workspace.setPreference("compact",checked)}
                }
                ColumnLayout {
                    id: widgetOptions
                    visible: categories.currentIndex===1
                    Layout.fillWidth: true
                    spacing: 10
                    FormGroup {
                        heading: qsTr("显示方式")
                        Flow {
                            Layout.fillWidth: true;spacing: 8
                            ActionButton {symbol: root.widgetController.visible ? "eyeOff" : "eye";text: root.widgetController.visible ? qsTr("隐藏组件") : qsTr("显示组件");onClicked: root.widgetController.setVisible(!root.widgetController.visible)}
                            ActionButton {symbol: root.widgetController.mini ? "expand" : "collapse";text: root.widgetController.mini ? qsTr("展开组件") : qsTr("切换迷你条");onClicked: root.widgetController.setMini(!root.widgetController.mini)}
                        }
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "window";text: qsTr("使用不透明背景");enabled: !root.workspace.preferences.highContrast;checked: root.workspace.preferences.solidWidget;onClicked: root.workspace.setPreference("solidWidget",checked)}
                        FormRow {
                            label: qsTr("背景透明度")
                            RowLayout {
                                Layout.fillWidth: true
                                Slider {Layout.fillWidth: true;Accessible.name: qsTr("组件背景透明度");enabled: !root.workspace.preferences.highContrast;from: 0.6;to: 1;value: root.widgetController.backgroundOpacity;onMoved: root.widgetController.setBackgroundOpacity(value)}
                                Label {text: Math.round(root.widgetController.backgroundOpacity*100)+"%";font.pixelSize: Theme.captionSize;Layout.minimumWidth: 42}
                            }
                        }
                    }
                    FormGroup {
                        heading: qsTr("窗口与交互")
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "pin";text: qsTr("保持置顶");checked: root.widgetController.alwaysOnTop;onClicked: root.widgetController.setAlwaysOnTop(checked)}
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "cursor";text: qsTr("鼠标穿透");enabled: root.widgetController.trayAvailable;checked: root.widgetController.clickThrough;onClicked: root.widgetController.setClickThrough(checked)}
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "lock";text: qsTr("锁定拖动");checked: root.widgetController.locked;onClicked: root.widgetController.setLocked(checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("穿透可从托盘或这里关闭；重启后恢复为可点击。组件显示状态会保留。")}
                        ActionButton {symbol: "reset";text: qsTr("重置组件位置");onClicked: root.widgetController.resetPosition()}
                    }
                }
                FormGroup {
                    visible: categories.currentIndex===1
                    heading: qsTr("独立桌面日历")
                    SettingSwitch {showSymbol: false;symbol: "calendar";text: qsTr("显示桌面日历");checked: root.workspace.preferences.desktopCalendarVisible;onClicked: {root.calendarVisibilityRequested(checked);checked=Qt.binding(function(){return root.workspace.preferences.desktopCalendarVisible})}}
                    FormRow {
                        label: qsTr("背景透明度")
                        RowLayout {
                            Layout.fillWidth: true
                            Slider {Layout.fillWidth: true;Accessible.name: qsTr("日历背景透明度");enabled: !root.workspace.preferences.highContrast;from: 50;to: 100;stepSize: 5;value: root.workspace.preferences.desktopCalendarOpacity;onMoved: root.workspace.setPreference("desktopCalendarOpacity",Math.round(value))}
                            Label {text: root.workspace.preferences.desktopCalendarOpacity+"%";font.pixelSize: Theme.captionSize;Layout.minimumWidth: 42}
                        }
                    }
                    SettingSwitch {showSymbol: false;symbol: "pin";text: qsTr("日历保持置顶");checked: root.workspace.preferences.desktopCalendarTopmost;onClicked: root.workspace.setPreference("desktopCalendarTopmost",checked)}
                }

                ColumnLayout {
                    id: general
                    visible: categories.currentIndex===2
                    Layout.fillWidth: true
                    spacing: 10
                    FormGroup {
                        heading: qsTr("启动与关闭")
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "window";text: qsTr("开机启动（当前用户）");enabled: root.startupController.supported;checked: root.startupController.enabled;onClicked: root.startupController.setEnabled(checked)}
                        SettingSwitch {showSymbol: false;Layout.fillWidth: true;symbol: "collapse";text: qsTr("关闭主窗口到托盘");enabled: root.widgetController.trayAvailable;checked: root.widgetController.closeToTray;onClicked: root.widgetController.setCloseToTray(checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.startupController.supported ? qsTr("移动程序后需重新开启开机启动。") : qsTr("当前平台暂不支持开机启动。")}
                        Label {visible: root.startupController.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.startupController.errorText}
                    }
                    FormGroup {
                        heading: qsTr("提醒")
                        SettingSwitch {showSymbol: false;id: systemReminders;objectName: "systemRemindersPreference";Layout.fillWidth: true;symbol: "bell";text: qsTr("系统通知");checked: root.workspace.preferences.systemReminders;onClicked: root.workspace.setPreference("systemReminders",checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("应用运行时检查提醒；系统通知取决于权限，触发记录保留在本机。")}
                    }
                    FormGroup {
                        heading: qsTr("自动备份")
                        detail: qsTr("启动时检查，保留最近7份一致性快照。")
                        ComboBox {Layout.fillWidth: true;Accessible.name: qsTr("自动备份周期");model: [qsTr("每天"),qsTr("每7天"),qsTr("每30天")];currentIndex: [1,7,30].indexOf(root.workspace.preferences.backupDays);onActivated: root.workspace.setPreference("backupDays",[1,7,30][currentIndex])}
                    }
                    ActionButton {symbol: "close";text: qsTr("退出 Daymark");onClicked: root.quitRequested()}
                    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.widgetController.trayAvailable ? qsTr("托盘可打开主窗口、恢复穿透或彻底退出。") : qsTr("系统托盘不可用；关闭主窗口将退出，穿透已禁用。")}
                }
            }
        }
        Label {visible: root.widgetController.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.widgetController.errorText}
        Label {visible: root.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText}
    }
}
