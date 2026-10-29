pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    objectName: "accountWindow"
    required property TaskWorkspace workspace
    signal syncLoginRequested(string origin, string login, string password)
    signal syncRegisterRequested(string origin, string login, string password)
    signal syncProfileRequested(string id)
    signal syncResolveRequested(string id, string decision)
    signal syncRebuildRequested()
    signal dismissed()
    property bool syncRegisterMode: false
    property bool serverExpanded: true
    property bool showPassword: false
    property bool prefilled: false
    property string prefillProfileId: ""
    readonly property bool connected: !!workspace.syncState.authenticated
    readonly property bool connecting: !!workspace.syncState.busy
    readonly property bool validRegistration: Array.from(syncPassword.text).length >= 15 && syncConfirmPassword.text === syncPassword.text
    readonly property bool canSubmit: !connecting && syncOrigin.text.trim().length > 0 && syncLogin.text.trim().length > 0 && syncPassword.text.length > 0 && (!syncRegisterMode || validRegistration)
    title: qsTr("账号 · Daymark")
    visible: false
    modality: Qt.WindowModal
    flags: Qt.Dialog | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.CustomizeWindowHint
    minimumWidth: 420
    minimumHeight: 440
    width: Math.min(560, Screen.width - 32)
    height: Math.min(680, Screen.height - 64)
    color: Theme.content
    font.family: Theme.fontFamily
    font.pixelSize: Theme.bodySize
    palette.window: Theme.content
    palette.base: Theme.content
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    function viewport() { return body.contentItem as Flickable }
    function clearSecrets() { syncPassword.clear(); syncConfirmPassword.clear(); showPassword = false }
    function open() {
        if (visible) { requestActivate(); return }
        syncRegisterMode = false
        clearSecrets()
        const profileId = workspace.syncState.profile_id || ""
        if (!prefilled || profileId !== prefillProfileId) {
            syncOrigin.text = workspace.syncState.server_origin || ""
            syncLogin.text = workspace.syncState.login_label || ""
            prefillProfileId = profileId; prefilled = true
        }
        serverExpanded = syncOrigin.text.length === 0
        const view = viewport(); if (view) view.contentY = 0
        x = Math.max(Screen.virtualX, Math.min(transientParent ? transientParent.x + (transientParent.width - width) / 2 : x, Screen.virtualX + Screen.width - width))
        y = Math.max(Screen.virtualY, Math.min(transientParent ? transientParent.y + (transientParent.height - height) / 2 : y, Screen.virtualY + Screen.height - height))
        show(); requestActivate()
        Qt.callLater(function() { if (root.visible && !root.connected) { if (root.serverExpanded) syncOrigin.forceActiveFocus(); else if (!syncLogin.text) syncLogin.forceActiveFocus(); else syncPassword.forceActiveFocus() } })
    }
    function close() { hide(); clearSecrets(); syncRegisterMode = false; dismissed(); if (transientParent) transientParent.requestActivate() }
    function reveal(item) {
        if (!item || !item.visible) return
        let owner = item
        while (owner && owner !== contentColumn) owner = owner.parent
        if (!owner) return
        const flick = viewport(); if (!flick) return
        const y = item.mapToItem(flick.contentItem, 0, 0).y
        if (y < flick.contentY) flick.contentY = y
        else if (y + item.height > flick.contentY + flick.height) flick.contentY = Math.min(Math.max(0, flick.contentHeight - flick.height), y + item.height - flick.height + 12)
    }
    function submit() {
        if (!canSubmit) return
        if (syncRegisterMode) syncRegisterRequested(syncOrigin.text, syncLogin.text, syncPassword.text)
        else syncLoginRequested(syncOrigin.text, syncLogin.text, syncPassword.text)
        clearSecrets()
    }
    onSyncRegisterModeChanged: clearSecrets()
    onVisibleChanged: if (!visible) clearSecrets()
    onClosing: function(event) { event.accepted = false; close() }
    onActiveFocusItemChanged: Qt.callLater(function() { root.reveal(root.activeFocusItem) })
    Shortcut { sequence: "Escape"; enabled: root.visible; onActivated: root.close() }
    Connections {
        target: root.workspace
        function onSyncStateChanged() {
            if (root.connected) {
                root.clearSecrets(); root.syncRegisterMode = false
                root.prefillProfileId = root.workspace.syncState.profile_id || ""; root.prefilled = true
                syncOrigin.text = root.workspace.syncState.server_origin || ""
                syncLogin.text = root.workspace.syncState.login_label || ""
                root.serverExpanded = false
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        ScrollView {
            id: body
            objectName: "accountScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            Item {
                width: body.availableWidth
                implicitHeight: contentColumn.implicitHeight + 48
                ColumnLayout {
                    id: contentColumn
                    objectName: "accountContent"
                    anchors.top: parent.top
                    anchors.topMargin: 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.max(0, Math.min(parent.width - 48, 440 * Math.min(1.4, Theme.scale)))
                    spacing: Theme.blockGap
                    BrandWordmark { objectName: "accountWordmark"; Layout.preferredWidth: 112; Layout.preferredHeight: 112 * 36 / 164; Layout.bottomMargin: 8 }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: Theme.fieldGap
                        Label { objectName: "accountHeading"; Layout.fillWidth: true; text: root.connected ? qsTr("账号与同步") : root.syncRegisterMode ? qsTr("创建账号") : qsTr("登录 Daymark"); font.pixelSize: Theme.headingSize; font.weight: Font.DemiBold; wrapMode: Text.Wrap }
                        Label { Layout.fillWidth: true; text: root.connected ? qsTr("你的任务先保存在此设备，再同步到服务器。") : qsTr("在你的设备之间，同步任务与安排。"); font.pixelSize: Theme.bodySize; color: Theme.secondary; wrapMode: Text.Wrap }
                    }
                    ColumnLayout {
                        visible: !root.connected
                        Layout.fillWidth: true
                        spacing: Theme.blockGap
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            RowLayout {
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: qsTr("同步服务器"); font.pixelSize: Theme.captionSize; color: Theme.secondary }
                                Button { text: root.serverExpanded ? qsTr("收起") : qsTr("更改"); visible: syncOrigin.text.length > 0; enabled: !root.connecting; flat: true; onClicked: { root.serverExpanded = !root.serverExpanded; if (root.serverExpanded) syncOrigin.forceActiveFocus() } }
                            }
                            Label { visible: !root.serverExpanded; Layout.fillWidth: true; text: syncOrigin.text; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.bodySize }
                            TextField { id: syncOrigin; objectName: "syncServerOrigin"; visible: root.serverExpanded; Layout.fillWidth: true; placeholderText: "https://sync.example.com"; Accessible.name: qsTr("同步服务器地址"); enabled: !root.connecting; selectByMouse: true }
                            Label { visible: root.serverExpanded; Layout.fillWidth: true; text: qsTr("填写你的 Daymark 服务器地址，使用 HTTPS 连接。"); wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            Label { text: qsTr("账号"); font.pixelSize: Theme.bodySize }
                            TextField { id: syncLogin; objectName: "syncLogin"; Layout.fillWidth: true; placeholderText: qsTr("输入账号"); Accessible.name: qsTr("账号"); enabled: !root.connecting; selectByMouse: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            Label { text: qsTr("密码"); font.pixelSize: Theme.bodySize }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.fieldGap
                                TextField { id: syncPassword; objectName: "syncPassword"; Layout.fillWidth: true; placeholderText: root.syncRegisterMode ? qsTr("至少15个字符") : qsTr("输入密码"); Accessible.name: qsTr("密码"); echoMode: root.showPassword ? TextInput.Normal : TextInput.Password; enabled: !root.connecting; onAccepted: { if (root.syncRegisterMode) syncConfirmPassword.forceActiveFocus(); else root.submit() } }
                                WidgetAction { objectName: "accountShowPassword"; symbol: "eye"; description: root.showPassword ? qsTr("隐藏密码") : qsTr("显示密码"); enabled: !root.connecting; checked: root.showPassword; onClicked: root.showPassword = !root.showPassword }
                            }
                        }
                        ColumnLayout {
                            visible: root.syncRegisterMode; Layout.fillWidth: true; spacing: Theme.fieldGap
                            Label { text: qsTr("确认密码"); font.pixelSize: Theme.bodySize }
                            TextField { id: syncConfirmPassword; objectName: "syncConfirmPassword"; Layout.fillWidth: true; placeholderText: qsTr("再次输入密码"); Accessible.name: qsTr("确认密码"); echoMode: root.showPassword ? TextInput.Normal : TextInput.Password; enabled: !root.connecting; onAccepted: root.submit() }
                            Label { objectName: "accountPasswordHint"; Layout.fillWidth: true; wrapMode: Text.Wrap; text: syncConfirmPassword.text.length > 0 && syncConfirmPassword.text !== syncPassword.text ? qsTr("两次密码不一致，请重新确认。") : qsTr("密码至少15个字符。请妥善保存，当前不提供邮件找回。"); color: syncConfirmPassword.text.length > 0 && syncConfirmPassword.text !== syncPassword.text ? Theme.danger : Theme.secondary; font.pixelSize: Theme.captionSize }
                        }
                        Label { objectName: "accountError"; visible: !root.connecting && !!root.workspace.syncState.safe_error_code; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.workspace.syncState.status || ""; color: Theme.danger; font.pixelSize: Theme.bodySize; Accessible.role: Accessible.AlertMessage }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            Button { id: syncSubmit; objectName: "syncSubmit"; Layout.fillWidth: true; Layout.minimumHeight: Math.max(40, Theme.controlHeight + 8); highlighted: true; text: root.connecting ? qsTr("正在连接…") : root.syncRegisterMode ? qsTr("注册并同步") : qsTr("登录并同步"); enabled: root.canSubmit; onClicked: root.submit(); BusyIndicator { anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 24; running: root.connecting; visible: running } }
                            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("将安全合并此设备与服务器的资料。"); color: Theme.secondary; font.pixelSize: Theme.captionSize; horizontalAlignment: Text.AlignHCenter }
                            Button { objectName: "syncRegisterMode"; Layout.alignment: Qt.AlignHCenter; text: root.syncRegisterMode ? qsTr("已有账号？返回登录") : qsTr("没有账号？创建账号"); flat: true; enabled: !root.connecting; onClicked: { root.syncRegisterMode = !root.syncRegisterMode; syncLogin.forceActiveFocus() } }
                        }
                    }
                    ColumnLayout {
                        visible: root.connected
                        Layout.fillWidth: true; spacing: Theme.blockGap
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            Label { objectName: "accountIdentity"; Layout.fillWidth: true; text: root.workspace.syncState.login_label || ""; wrapMode: Text.WrapAnywhere; font.pixelSize: Theme.sectionSize; font.weight: Font.DemiBold }
                            Label { Layout.fillWidth: true; text: root.workspace.syncState.server_origin || ""; wrapMode: Text.WrapAnywhere; color: Theme.secondary; font.pixelSize: Theme.captionSize }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: Theme.fieldGap
                            RowLayout {
                                Layout.fillWidth: true
                                BusyIndicator { visible: root.connecting; running: visible; Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
                                Label { objectName: "accountStatus"; Layout.fillWidth: true; text: root.workspace.syncState.status || qsTr("已连接"); wrapMode: Text.Wrap; font.pixelSize: Theme.bodySize }
                            }
                            Label { visible: (root.workspace.syncState.pending_count || 0) > 0; Layout.fillWidth: true; text: qsTr("%1 组修改保留在此设备，等待同步。").arg(root.workspace.syncState.pending_count || 0); wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary }
                            Button { objectName: "syncNow"; text: root.connecting ? qsTr("正在同步…") : qsTr("立即同步"); enabled: !root.connecting; onClicked: root.workspace.syncNow() }
                            Button { objectName: "syncRebuild"; visible: root.workspace.syncState.phase === "RECONCILE_REQUIRED"; text: qsTr("备份并重新合并"); enabled: !root.connecting; onClicked: root.syncRebuildRequested() }
                        }
                        Repeater {
                            model: root.workspace.syncState.conflicts || []
                            delegate: ColumnLayout {
                                id: conflict
                                required property var modelData
                                Layout.fillWidth: true; spacing: Theme.fieldGap
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("处理修改冲突：%1").arg(conflict.modelData.local_title || conflict.modelData.remote_title || qsTr("相关资料")); font.pixelSize: Theme.sectionSize; font.weight: Font.DemiBold }
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("此选择处理 %1 项相关修改，两个版本保留在恢复记录中。").arg(conflict.modelData.related_count || 1); font.pixelSize: Theme.captionSize; color: Theme.secondary }
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("此设备\n%1").arg(conflict.modelData.local_summary || ""); font.pixelSize: Theme.bodySize }
                                Button { objectName: "accountConflictLocal"; text: qsTr("保留此设备"); enabled: !root.connecting; onClicked: root.syncResolveRequested(conflict.modelData.id, "local") }
                                Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("服务器\n%1").arg(conflict.modelData.remote_summary || ""); font.pixelSize: Theme.bodySize }
                                Button { objectName: "accountConflictRemote"; text: qsTr("使用服务器版本"); enabled: !root.connecting && conflict.modelData.remote_available !== false; onClicked: root.syncResolveRequested(conflict.modelData.id, "remote") }
                            }
                        }
                    }
                    ColumnLayout {
                        visible: syncProfiles.count > 1
                        Layout.fillWidth: true; spacing: Theme.fieldGap
                        Label { text: qsTr("此设备的账号资料"); font.pixelSize: Theme.sectionSize }
                        ComboBox { id: syncProfiles; objectName: "syncProfiles"; Layout.fillWidth: true; Accessible.name: qsTr("本地账号资料"); model: root.workspace.syncState.profiles || []; textRole: "label"; enabled: !root.connecting; onActivated: { root.syncProfileRequested(model[currentIndex].id); root.close() } }
                    }
                    ColumnLayout {
                        visible: root.connected
                        Layout.fillWidth: true; spacing: Theme.fieldGap
                        Button { objectName: "syncLogout"; text: qsTr("退出账号"); flat: true; onClicked: root.workspace.syncLogout() }
                        Label { Layout.fillWidth: true; text: qsTr("退出后停止同步，本机资料与未传修改仍会保留。"); wrapMode: Text.Wrap; color: Theme.secondary; font.pixelSize: Theme.captionSize }
                    }
                }
            }
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.separator }
        ColumnLayout {
            objectName: "accountWindowFooter"
            Layout.fillWidth: true; Layout.margins: 16; spacing: Theme.fieldGap
            Label { visible: root.connecting && !root.connected; Layout.fillWidth: true; text: qsTr("关闭窗口后，连接会在后台继续。"); wrapMode: Text.Wrap; font.pixelSize: Theme.captionSize; color: Theme.secondary; horizontalAlignment: Text.AlignHCenter }
            Button { objectName: "accountContinueLocal"; Layout.fillWidth: true; flat: true; text: root.connected ? qsTr("返回任务") : root.connecting ? qsTr("在后台继续") : qsTr("继续本地使用"); onClicked: root.close() }
        }
    }
}
