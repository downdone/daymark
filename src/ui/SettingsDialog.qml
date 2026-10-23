pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Dialog {
    id: root
    objectName: "settingsDialog"
    signal calendarVisibilityRequested(bool visible)
    required property TaskWorkspace workspace
    required property WidgetViewModel widgetController
    required property StartupController startupController
    property alias categoryIndex: categories.currentIndex
    title: qsTr("设置")
    width: Math.min(640*Theme.scale,parent.width-32)
    height: Math.min(560*Theme.scale,parent.height-32)
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
        spacing: 12
        SegmentedPicker {
            id: categories
            objectName: "settingsCategories"
            model: [qsTr("外观"),qsTr("桌面组件"),qsTr("通用")]
            onActivated: function(index){root.setVerificationCategory(index)}
        }
        Label {text: qsTr("更改会立即保存在本机");color: Theme.secondary;font.pixelSize: Theme.captionSize;Layout.fillWidth: true}
        ScrollView {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            ColumnLayout {
                width: body.availableWidth-8
                spacing: 16
                FormGroup {
                    id: appearance
                    visible: categories.currentIndex===0
                    heading: qsTr("阅读与显示")
                    detail: qsTr("使用系统字体，文字放大时布局会随之调整。")
                    Label {text: qsTr("主题");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    ComboBox {Layout.fillWidth: true;Accessible.name: qsTr("主题");model: [qsTr("跟随系统"),qsTr("浅色"),qsTr("深色")];currentIndex: root.workspace.themeMode;onActivated: root.workspace.themeMode=currentIndex}
                    Label {text: qsTr("文字大小 · %1%").arg(Math.round(root.workspace.textScale*100))}
                    Slider {Layout.fillWidth: true;Accessible.name: qsTr("文字大小");from: 1;to: 2;stepSize: 0.25;value: root.workspace.textScale;onMoved: root.workspace.textScale=value}
                    CheckBox {Layout.fillWidth: true;text: qsTr("紧凑任务列表");checked: root.workspace.preferences.compact;onClicked: root.workspace.setPreference("compact",checked)}
                }
                ColumnLayout {
                    id: widgetOptions
                    visible: categories.currentIndex===1
                    Layout.fillWidth: true
                    spacing: 16
                    FormGroup {
                        heading: qsTr("显示方式")
                        Flow {
                            Layout.fillWidth: true;spacing: 8
                            Button {text: root.widgetController.visible ? qsTr("隐藏组件") : qsTr("显示组件");onClicked: root.widgetController.setVisible(!root.widgetController.visible)}
                            Button {text: root.widgetController.mini ? qsTr("展开组件") : qsTr("切换迷你条");onClicked: root.widgetController.setMini(!root.widgetController.mini)}
                        }
                        CheckBox {Layout.fillWidth: true;text: qsTr("使用不透明背景");checked: root.workspace.preferences.solidWidget;onClicked: root.workspace.setPreference("solidWidget",checked)}
                        Label {text: qsTr("背景透明度 · %1%").arg(Math.round(root.widgetController.backgroundOpacity*100))}
                        Slider {Layout.fillWidth: true;Accessible.name: qsTr("组件背景透明度");from: 0.6;to: 1;value: root.widgetController.backgroundOpacity;onMoved: root.widgetController.setBackgroundOpacity(value)}
                    }
                    FormGroup {
                        heading: qsTr("窗口与交互")
                        CheckBox {Layout.fillWidth: true;text: qsTr("保持置顶");checked: root.widgetController.alwaysOnTop;onClicked: root.widgetController.setAlwaysOnTop(checked)}
                        CheckBox {Layout.fillWidth: true;text: qsTr("鼠标穿透");enabled: root.widgetController.trayAvailable;checked: root.widgetController.clickThrough;onClicked: root.widgetController.setClickThrough(checked)}
                        CheckBox {Layout.fillWidth: true;text: qsTr("锁定拖动");checked: root.widgetController.locked;onClicked: root.widgetController.setLocked(checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("穿透可从托盘或这里关闭；重启后恢复为可点击。组件显示状态会保留。")}
                        Button {text: qsTr("重置组件位置");onClicked: root.widgetController.resetPosition()}
                    }
                }
                FormGroup {
                    visible: categories.currentIndex===1
                    heading: qsTr("独立桌面日历")
                    CheckBox {text: qsTr("显示日历（可与任务小窗并存）");checked: root.workspace.preferences.desktopCalendarVisible;onClicked: {root.calendarVisibilityRequested(checked);checked=Qt.binding(function(){return root.workspace.preferences.desktopCalendarVisible})}}
                    Label {text: qsTr("背景透明度 · %1%").arg(root.workspace.preferences.desktopCalendarOpacity)}
                    Slider {Layout.fillWidth: true;Accessible.name: qsTr("日历背景透明度");from: 50;to: 100;stepSize: 5;value: root.workspace.preferences.desktopCalendarOpacity;onMoved: root.workspace.setPreference("desktopCalendarOpacity",Math.round(value))}
                    CheckBox {text: qsTr("日历保持置顶");checked: root.workspace.preferences.desktopCalendarTopmost;onClicked: root.workspace.setPreference("desktopCalendarTopmost",checked)}
                }

                ColumnLayout {
                    id: general
                    visible: categories.currentIndex===2
                    Layout.fillWidth: true
                    spacing: 16
                    FormGroup {
                        heading: qsTr("启动与关闭")
                        CheckBox {Layout.fillWidth: true;text: qsTr("开机启动（当前用户）");enabled: root.startupController.supported;checked: root.startupController.enabled;onClicked: root.startupController.setEnabled(checked)}
                        CheckBox {Layout.fillWidth: true;text: qsTr("关闭主窗口到托盘");enabled: root.widgetController.trayAvailable;checked: root.widgetController.closeToTray;onClicked: root.widgetController.setCloseToTray(checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("开机启动使用当前程序路径，移动程序后需重新开启。")}
                        Label {visible: root.startupController.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.startupController.errorText}
                    }
                    FormGroup {
                        heading: qsTr("任务提醒")
                        CheckBox {id: systemReminders;objectName: "systemRemindersPreference";Layout.fillWidth: true;text: qsTr("提交系统提醒");checked: root.workspace.preferences.systemReminders;onClicked: root.workspace.setPreference("systemReminders",checked)}
                        Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("应用内触发记录始终保留；系统通知的展示取决于权限。")}
                    }
                    FormGroup {
                        heading: qsTr("自动备份")
                        detail: qsTr("启动时检查，保留最近7份一致性快照。")
                        ComboBox {Layout.fillWidth: true;Accessible.name: qsTr("自动备份周期");model: [qsTr("每天"),qsTr("每7天"),qsTr("每30天")];currentIndex: [1,7,30].indexOf(root.workspace.preferences.backupDays);onActivated: root.workspace.setPreference("backupDays",[1,7,30][currentIndex])}
                    }
                    Label {Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: root.widgetController.trayAvailable ? qsTr("托盘可打开主窗口、恢复穿透或彻底退出。") : qsTr("系统托盘不可用；关闭主窗口将退出，穿透已禁用。")}
                }
            }
        }
        Label {visible: root.widgetController.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.widgetController.errorText}
        Label {visible: root.workspace.errorText.length>0;Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: root.workspace.errorText}
    }
}
