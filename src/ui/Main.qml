pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow
    required property WidgetViewModel widgetController
    required property TaskWorkspace taskWorkspace
    objectName: "mainWindow"
    width: 1120
    height: 720
    minimumWidth: 880
    minimumHeight: 580
    visible: true
    title: "Daymark"
    color: Theme.content
    font.pixelSize: 14*Theme.scale
    palette.window: Theme.content
    palette.windowText: Theme.text
    palette.base: Theme.content
    palette.text: Theme.text
    palette.placeholderText: Theme.secondary
    palette.button: Theme.sidebar
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.dark ? "#191a1d" : Theme.content
    palette.mid: Theme.separator
    palette.dark: Theme.secondary
    Binding { target: Theme; property: "mode"; value: mainWindow.taskWorkspace.themeMode }
    Binding { target: Theme; property: "scale"; value: mainWindow.taskWorkspace.textScale }

    readonly property var viewCodes: ["inbox", "today", "upcoming", "anytime", "someday", "logbook"]
    readonly property var viewNames: [qsTr("收集箱"), qsTr("今天"), qsTr("即将到来"), qsTr("随时可做"), qsTr("以后再说"), qsTr("已完成")]
    property string editId: ""
    property real editVersion: 0
    property string editStatus: "OPEN"
    property bool draftDirty: false
    property bool fillingDraft: false
    property var pendingAction: null
    property bool forceQuit: false
    readonly property bool calendarVisible: taskWorkspace.view === "month" || taskWorkspace.view === "week"

    function markDirty() { if (!fillingDraft) draftDirty = true }
    function openEditor(taskId) {
        const item = taskWorkspace.task(taskId)
        if (!item.taskId || !taskWorkspace.ready || taskWorkspace.busy) return
        fillingDraft = true
        editId = item.taskId
        editVersion = item.rowVersion
        editStatus = item.taskStatus
        editTitle.text = item.title
        editNotes.text = item.notes
        planning.currentIndex = ["INBOX", "ACTIVE", "SOMEDAY"].indexOf(item.planningState)
        startDate.text = item.startOn
        deadline.text = item.deadlineOn
        focusToday.checked = item.todayFocus
        fillingDraft = false
        draftDirty = false
        taskWorkspace.clearError()
        editor.open()
    }
    function navigateSafely(action) {
        if (taskWorkspace.busy) return
        if (editor.visible && draftDirty) {
            pendingAction = action
            leaveDialog.open()
        } else action()
    }
    function saveEditor() {
        return taskWorkspace.saveTask(editId, editVersion, editTitle.text, editNotes.text,
                                     ["INBOX", "ACTIVE", "SOMEDAY"][planning.currentIndex],
                                     startDate.text, deadline.text, focusToday.checked)
    }
    function currentViewTitle() {
        const index=viewCodes.indexOf(taskWorkspace.view)
        if(index>=0)return viewNames[index]
        if(calendarVisible)return qsTr("日历")
        for(const item of taskWorkspace.catalog)if(taskWorkspace.view.endsWith(":"+item.id))return item.title
        return qsTr("任务")
    }
    function showVerificationPage(page,taskId) {
        editor.close();extendedDetails.close();organization.close();recurrence.close();windowSettings.close();dataTools.close();datePicker.close()
        if(page === "details")extendedDetails.openTask(taskId)
        else if(page === "organization")organization.open()
        else if(page === "recurrence")recurrence.open()
        else if(page === "settings")windowSettings.open()
        else if(page === "data")dataTools.open()
        else if(page === "date")datePicker.showFor(taskWorkspace.dateOffset(0),"start")
        else if(page === "editor")openEditor(taskId)
        else taskWorkspace.setView(page)
    }

    Shortcut { sequences: [StandardKey.New]; onActivated: { if (!editor.visible) { if (mainWindow.taskWorkspace.view === "logbook") mainWindow.taskWorkspace.setView("inbox"); newTitle.forceActiveFocus() } } }
    Shortcut { sequences: [StandardKey.Find]; onActivated: { if (!editor.visible) search.forceActiveFocus() } }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: Math.min(300,220*Theme.scale)
            Layout.fillHeight: true
            color: Theme.sidebar
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 8
                Label { text: "Daymark"; font.pixelSize: 25*Theme.scale; font.bold: true; Layout.bottomMargin: 22 }
                Repeater {
                    model: mainWindow.viewNames
                    NavRow {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        selected: mainWindow.taskWorkspace.view === mainWindow.viewCodes[index]
                        symbol: ["▱","☀","▦","○","◌","✓"][index]
                        count: index===1 ? mainWindow.taskWorkspace.todayTasks.count : -1
                        onClicked: mainWindow.taskWorkspace.setView(mainWindow.viewCodes[index])
                    }
                }
                ScrollView {
                    id: catalogScroller
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    clip: true
                    ColumnLayout {
                        width: catalogScroller.availableWidth
                        Repeater {
                            model: mainWindow.taskWorkspace.catalog
                            NavRow {
                                required property var modelData
                                visible: modelData.kind !== "heading"
                                Layout.fillWidth: true
                                text: modelData.title + (modelData.status !== "OPEN" ? qsTr("（历史）") : "")
                                symbol: modelData.kind === "area" ? "▣" : modelData.kind === "project" ? "▤" : "#"
                                selected: mainWindow.taskWorkspace.view === modelData.kind+":"+modelData.id
                                acceptsTaskDrop: modelData.kind === "project" || modelData.kind === "area"
                                onTaskDropped: function(taskId,version) {mainWindow.taskWorkspace.saveDetails(taskId,version,modelData.kind === "project" ? {projectId:modelData.id} : {areaId:modelData.id})}
                                onClicked: mainWindow.taskWorkspace.setView(modelData.kind+":"+modelData.id)
                            }
                        }
                    }
                }
                NavRow { Layout.fillWidth: true; text: qsTr("日历"); symbol: "▦"; selected: mainWindow.calendarVisible; onClicked: mainWindow.taskWorkspace.setView("month") }
                Button {
                    Layout.fillWidth: true
                    text: qsTr("设置与工具…")
                    onClicked: toolsMenu.open()
                    Menu {
                        id: toolsMenu
                        MenuItem { text: qsTr("管理领域、项目与标签…"); onTriggered: organization.open() }
                        MenuItem { text: qsTr("重复任务规则…"); onTriggered: recurrence.open() }
                        MenuItem { text: qsTr("提醒中心（%1）").arg(mainWindow.taskWorkspace.notifications.length); onTriggered: notifications.open() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("外观、窗口与组件…"); onTriggered: windowSettings.open() }
                        MenuItem { text: qsTr("数据与备份…"); enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy; onTriggered: dataTools.open() }
                        MenuSeparator {}
                        MenuItem { text: qsTr("退出 Daymark"); onTriggered: {mainWindow.forceQuit=true;mainWindow.close()} }
                    }
                }
                Label { text: qsTr("本地保存 · 无需登录"); color: Theme.secondary; font.pixelSize: 11*Theme.scale }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            spacing: 14
            RowLayout {
                Label {
                    text: search.text.trim().length > 0 ? qsTr("搜索结果") : mainWindow.currentViewTitle()
                    font.pixelSize: 27*Theme.scale
                    font.bold: true
                    Layout.fillWidth: true
                }
                BusyIndicator { running: mainWindow.taskWorkspace.busy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                Button { text: qsTr("重新加载"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.taskWorkspace.reload() }
            }
            TextField {
                id: search
                visible: !mainWindow.calendarVisible
                Layout.fillWidth: true
                placeholderText: qsTr("搜索任务与备注…")
                onTextChanged: mainWindow.taskWorkspace.setSearch(text)
                Keys.onEscapePressed: {clear();taskList.forceActiveFocus()}
                selectByMouse: true
            }
            RowLayout {
                visible: !mainWindow.calendarVisible
                CheckBox { visible: search.text.trim().length>0 || mainWindow.taskWorkspace.view.startsWith("project:"); text: qsTr("包括已完成/取消"); checked: mainWindow.taskWorkspace.includeCompleted; onClicked: mainWindow.taskWorkspace.includeCompleted=checked }
                ComboBox { property var tags: mainWindow.taskWorkspace.catalogOf("tag"); Layout.fillWidth: true; model: [qsTr("全部标签")].concat(tags.map(function(t){return t.title})); onActivated: mainWindow.taskWorkspace.setTagFilter(currentIndex===0 ? "" : tags[currentIndex-1].id) }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: mainWindow.taskWorkspace.errorText.length > 0 || mainWindow.taskWorkspace.infoText.length > 0
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: mainWindow.taskWorkspace.errorText || mainWindow.taskWorkspace.infoText
                    color: mainWindow.taskWorkspace.errorText.length > 0 ? Theme.danger : Theme.success
                }
                Button { text: qsTr("关闭提示"); onClicked: mainWindow.taskWorkspace.clearError() }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy && mainWindow.taskWorkspace.view !== "logbook"
                visible: !mainWindow.calendarVisible
                TextField {
                    id: newTitle
                    objectName: "newTaskTitle"
                    Layout.fillWidth: true
                    placeholderText: mainWindow.taskWorkspace.view === "upcoming" ? qsTr("新增任务（默认安排明天）") : qsTr("输入任务标题，Enter 添加")
                    selectByMouse: true
                    onAccepted: mainWindow.taskWorkspace.createTask(text)
                    Keys.onEscapePressed: taskList.forceActiveFocus()
                }
                Button { text: qsTr("添加任务"); onClicked: mainWindow.taskWorkspace.createTask(newTitle.text) }
            }
            Label {
                visible: !mainWindow.calendarVisible
                text: mainWindow.taskWorkspace.view === "someday" ? qsTr("以后再说会暂停日期，激活后重新进入执行视图。") : qsTr("点击任务编辑详情；今天与随时可做可以包含同一任务。")
                color: Theme.secondary
                font.pixelSize: 12*Theme.scale
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label { visible: mainWindow.taskWorkspace.view === "today"; text: mainWindow.taskWorkspace.localToday+qsTr(" · 待办 %1 · 今日完成 %2").arg(mainWindow.taskWorkspace.todayTasks.count).arg(mainWindow.taskWorkspace.completedToday); color: Theme.secondary }
            RowLayout {
                visible: mainWindow.taskWorkspace.view === "today"
                Repeater {
                    model: [qsTr("拖入今天"),qsTr("拖入今晚")]
                    Label {
                        id: eveningDrop
                        required property string modelData
                        required property int index
                        text: modelData
                        padding: 8
                        color: Theme.accent
                        background: Rectangle {color: Theme.hover;radius: 5}
                        DropArea {anchors.fill: parent;onDropped: function(drop){const source=drop.source as TaskRow;if(source){mainWindow.taskWorkspace.saveDetails(source.taskId,source.rowVersion,{evening:eveningDrop.index===1});drop.acceptProposedAction()}}}
                    }
                }
            }
            CalendarPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainWindow.calendarVisible
                workspace: mainWindow.taskWorkspace
                onOpenTask: function(taskId) {mainWindow.openEditor(taskId)}
            }
            ListView {
                id: taskList
                visible: !mainWindow.calendarVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                focus: true
                keyNavigationEnabled: true
                keyNavigationWraps: true
                section.property: mainWindow.taskWorkspace.view === "today" ? "taskSection" : mainWindow.taskWorkspace.view === "upcoming" ? "startOn" : mainWindow.taskWorkspace.view.startsWith("project:") ? "headingTitle" : ""
                section.delegate: Label { required property string section; text: section; color: Theme.secondary; font.pixelSize: 13*Theme.scale; height: implicitHeight+20; verticalAlignment: Text.AlignVCenter }
                Keys.onReturnPressed: {const item=taskList.currentItem as TaskRow;if(item)item.editRequested()}
                Keys.onSpacePressed: {const item=taskList.currentItem as TaskRow;if(item)item.completeRequested()}
                model: mainWindow.taskWorkspace.tasks
                ScrollBar.vertical: ScrollBar {}
                delegate: TaskRow {
                    required property int index
                    width: taskList.width
                    enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy
                    highlighted: taskList.currentIndex === index
                    onEditRequested: {taskList.currentIndex=index;mainWindow.openEditor(taskId)}
                    onCompleteRequested: mainWindow.taskWorkspace.changeState(taskId,rowVersion,isDone ? "reopen" : "complete")
                    onMoveRequested: function(direction) {mainWindow.taskWorkspace.moveTask(taskId,direction)}
                    onDropRequested: function(source) {mainWindow.taskWorkspace.dropTask(source,taskId)}
                }
                Label {
                    anchors.centerIn: parent
                    visible: mainWindow.taskWorkspace.tasks.count === 0
                    text: !mainWindow.taskWorkspace.ready ? qsTr("正在准备本地数据，或请根据上方错误重试。") : search.text.length > 0 ? qsTr("没有匹配的任务") : qsTr("此视图暂无任务，可在上方添加或从其他视图安排。")
                    color: Theme.secondary
                }
            }
            RowLayout {
                visible: mainWindow.taskWorkspace.view === "upcoming" && mainWindow.taskWorkspace.deadlineTasks.count>0
                Layout.fillWidth: true
                Label { text: qsTr("临近截止"); color: Theme.danger }
                ComboBox { id: approaching; Layout.fillWidth: true; model: mainWindow.taskWorkspace.deadlineTasks; textRole: "title"; valueRole: "taskId" }
                Button { text: qsTr("查看任务"); onClicked: mainWindow.openEditor(approaching.currentValue) }
            }
            RowLayout {
                Label { text: mainWindow.calendarVisible ? qsTr("按计划开始与截止显示") : qsTr("%1 项").arg(mainWindow.taskWorkspace.tasks.count); color: Theme.secondary; Layout.fillWidth: true }
                Button {visible: mainWindow.taskWorkspace.view === "upcoming";text: qsTr("未来%1天 · 再显示30天").arg(mainWindow.taskWorkspace.upcomingDays);onClicked: mainWindow.taskWorkspace.loadMoreUpcoming()}
                Button { visible: mainWindow.taskWorkspace.canUndo; text: qsTr("撤销删除"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.taskWorkspace.undoDelete() }
                Label { text: mainWindow.taskWorkspace.busy ? qsTr("正在保存…") : mainWindow.taskWorkspace.ready ? qsTr("已保存在本机") : qsTr("数据未就绪"); color: Theme.secondary }
            }
        }
    }

    Dialog {
        id: editor
        objectName: "taskEditor"
        parent: Overlay.overlay
        x: (parent.width - width) / 2
        y: Math.max(16, (parent.height - height) / 2)
        width: Math.min(620, mainWindow.width - 48)
        height: Math.min(600,mainWindow.height-32)
        title: qsTr("任务详情")
        modal: true
        closePolicy: Popup.NoAutoClose
        contentItem: ScrollView {
            clip: true
            ColumnLayout {
            width: editor.availableWidth-12
            spacing: 10
            enabled: !mainWindow.taskWorkspace.busy
            Label { text: qsTr("标题") }
            TextField { id: editTitle; objectName: "editTaskTitle"; Layout.fillWidth: true; selectByMouse: true; onTextChanged: mainWindow.markDirty() }
            Label { text: qsTr("备注") }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                TextArea { id: editNotes; wrapMode: TextEdit.Wrap; selectByMouse: true; onTextChanged: mainWindow.markDirty() }
            }
            RowLayout {
                Label { text: qsTr("规划状态") }
                ComboBox { id: planning; model: [qsTr("收集箱"), qsTr("已激活"), qsTr("以后再说")]; onCurrentIndexChanged: mainWindow.markDirty() }
                CheckBox { id: focusToday; text: qsTr("持续关注今天"); onCheckedChanged: mainWindow.markDirty();onClicked: if(!checked && mainWindow.taskWorkspace.todayAfterRemovingFocus(mainWindow.editId))removeToday.open() }
            }
            GridLayout {
                columns: 2
                Layout.fillWidth: true
                Label { text: qsTr("计划开始");Layout.fillWidth: true;wrapMode: Text.Wrap }
                Label { text: qsTr("截止日期");Layout.fillWidth: true;wrapMode: Text.Wrap }
                TextField { id: startDate; objectName: "editStartDate"; Layout.fillWidth: true; placeholderText: qsTr("留空清除"); onTextChanged: mainWindow.markDirty() }
                TextField { id: deadline; Layout.fillWidth: true; placeholderText: qsTr("留空清除"); onTextChanged: mainWindow.markDirty() }
            }
            RowLayout {
                Button { text: qsTr("选择开始日…"); onClicked: datePicker.showFor(startDate.text,"start") }
                Button { text: qsTr("选择截止日…"); onClicked: datePicker.showFor(deadline.text,"deadline") }
            }
            RowLayout {
                Button { text: qsTr("开始：今天"); onClicked: startDate.text=mainWindow.taskWorkspace.dateOffset(0) }
                Button { text: qsTr("明天"); onClicked: startDate.text=mainWindow.taskWorkspace.dateOffset(1) }
                Button { text: qsTr("清除开始"); onClicked: startDate.clear() }
                Button { text: qsTr("清除截止"); onClicked: deadline.clear() }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: qsTr("设置日期或今日关注会自动激活收集箱任务；加入今天会调整将来的开始日。以后再说会保留日期并暂停执行。"); font.pixelSize: 11*Theme.scale; color: Theme.secondary }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; visible: mainWindow.taskWorkspace.errorText.length > 0; text: mainWindow.taskWorkspace.errorText; color: Theme.danger }
            }
        }
        footer: Flow {
            spacing: 8
            Button {
                text: qsTr("归属/标签/清单…")
                enabled: !mainWindow.taskWorkspace.busy
                onClicked: mainWindow.navigateSafely(function(){editor.close();extendedDetails.openTask(mainWindow.editId)})
            }
            Button {
                text: qsTr("取消任务")
                visible: mainWindow.editStatus === "OPEN"
                enabled: !mainWindow.taskWorkspace.busy
                onClicked: mainWindow.navigateSafely(function() { mainWindow.taskWorkspace.changeState(mainWindow.editId, mainWindow.editVersion, "cancel"); editor.close() })
            }
            Button {
                text: qsTr("删除")
                enabled: !mainWindow.taskWorkspace.busy
                onClicked: mainWindow.navigateSafely(function() { mainWindow.taskWorkspace.changeState(mainWindow.editId, mainWindow.editVersion, "delete"); editor.close() })
            }
            Button { objectName: "returnFromEditor"; text: qsTr("返回"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.navigateSafely(function() { editor.close() }) }
            Button { text: qsTr("保存"); enabled: !mainWindow.taskWorkspace.busy; highlighted: true; onClicked: { mainWindow.pendingAction = null; mainWindow.saveEditor() } }
        }
    }

    Dialog {
        id: leaveDialog
        objectName: "unsavedChangesDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("有未保存的修改")
        modal: true
        closePolicy: Popup.NoAutoClose
        Label { text: qsTr("保存修改后继续，或明确放弃此次编辑。") }
        footer: DialogButtonBox {
            Button { objectName: "cancelLeave"; text: qsTr("取消离开"); onClicked: { mainWindow.pendingAction = null; mainWindow.forceQuit=false;leaveDialog.close() } }
            Button {
                objectName: "discardDraft"
                text: qsTr("放弃修改")
                onClicked: {
                    mainWindow.draftDirty = false
                    const action = mainWindow.pendingAction
                    mainWindow.pendingAction = null
                    leaveDialog.close()
                    if (action) action()
                }
            }
            Button { text: qsTr("保存后继续"); onClicked: { leaveDialog.close(); mainWindow.saveEditor() } }
        }
    }

    Dialog {
        id: windowSettings
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        height: Math.min(620,mainWindow.height-32)
        title: qsTr("外观、窗口与桌面组件")
        modal: true
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            clip: true
            ColumnLayout {
            width: windowSettings.availableWidth-12
            Label { text: qsTr("主题") }
            ComboBox { model: [qsTr("跟随系统"),qsTr("浅色"),qsTr("深色")]; currentIndex: mainWindow.taskWorkspace.themeMode; onActivated: mainWindow.taskWorkspace.themeMode=currentIndex }
            Label { text: qsTr("文字大小 %1%").arg(Math.round(mainWindow.taskWorkspace.textScale*100)) }
            Slider { from: 1; to: 2; stepSize: 0.25; value: mainWindow.taskWorkspace.textScale; onMoved: mainWindow.taskWorkspace.textScale=value }
            Button { text: mainWindow.widgetController.visible ? qsTr("隐藏组件") : qsTr("显示组件"); onClicked: mainWindow.widgetController.setVisible(!mainWindow.widgetController.visible) }
            Button { text: mainWindow.widgetController.mini ? qsTr("展开组件") : qsTr("切换迷你条"); onClicked: mainWindow.widgetController.setMini(!mainWindow.widgetController.mini) }
            CheckBox { text: qsTr("保持置顶"); checked: mainWindow.widgetController.alwaysOnTop; onClicked: mainWindow.widgetController.setAlwaysOnTop(checked) }
            CheckBox { text: qsTr("鼠标穿透（从托盘或此处恢复）"); enabled: mainWindow.widgetController.trayAvailable; checked: mainWindow.widgetController.clickThrough; onClicked: mainWindow.widgetController.setClickThrough(checked) }
            CheckBox { text: qsTr("锁定拖动"); checked: mainWindow.widgetController.locked; onClicked: mainWindow.widgetController.setLocked(checked) }
            CheckBox { text: qsTr("关闭主窗口到托盘"); enabled: mainWindow.widgetController.trayAvailable; checked: mainWindow.widgetController.closeToTray; onClicked: mainWindow.widgetController.setCloseToTray(checked) }
            Label { text: qsTr("背景透明度") }
            Slider { from: 0.6; to: 1.0; value: mainWindow.widgetController.backgroundOpacity; onMoved: mainWindow.widgetController.setBackgroundOpacity(value) }
            Button { text: qsTr("重置组件位置"); onClicked: mainWindow.widgetController.resetPosition() }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: mainWindow.widgetController.errorText; color: Theme.danger }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: mainWindow.widgetController.trayAvailable ? qsTr("托盘可打开主窗口、恢复穿透或彻底退出。重启默认恢复为可点击模式。") : qsTr("当前系统托盘不可用；关闭主窗口将退出，穿透已禁用。"); color: Theme.secondary }
            }
        }
    }

    Connections {
        target: mainWindow.taskWorkspace
        function onOperationFinished(operation, success) {
            if (operation === "create" && success) { newTitle.clear(); newTitle.forceActiveFocus() }
            if (operation === "save") {
                if (success) {
                    mainWindow.editVersion = mainWindow.taskWorkspace.task(mainWindow.editId).rowVersion
                    mainWindow.draftDirty = false
                    editor.close()
                    const action = mainWindow.pendingAction
                    mainWindow.pendingAction = null
                    if (action) action()
                } else mainWindow.pendingAction = null
            }
        }
    }
    WidgetWindow { controller: mainWindow.widgetController; workspace: mainWindow.taskWorkspace }
    OrganizationDialog { id: organization; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    TaskDetailsDialog { id: extendedDetails; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    RecurrenceDialog { id: recurrence; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    DataToolsDialog { id: dataTools; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    DatePickerDialog {id: datePicker;parent: Overlay.overlay;anchors.centerIn: parent;workspace: mainWindow.taskWorkspace;onDateSelected: function(date,field){if(field==="start")startDate.text=date;else deadline.text=date}}
    Dialog {
        id: removeToday
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(500,mainWindow.width-40)
        height: Math.min(220*Theme.scale,mainWindow.height-40)
        title: qsTr("日期仍让任务留在今天")
        modal: true
        closePolicy: Popup.NoAutoClose
        contentItem: Label {text: qsTr("移除关注不会清除已到的开始/截止日期。请重新安排日期，或设为以后再说。");wrapMode: Text.Wrap}
        footer: Flow {
            spacing: 8
            Button {text: qsTr("保留");onClicked: {focusToday.checked=mainWindow.taskWorkspace.task(mainWindow.editId).todayFocus;removeToday.close()}}
            Button {text: qsTr("重新安排…");onClicked: {removeToday.close();datePicker.showFor(startDate.text,"start")}}
            Button {text: qsTr("以后再说");onClicked: {planning.currentIndex=2;removeToday.close()}}
        }
    }
    Dialog {
        id: notifications
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("提醒中心 · 最近50条")
        width: Math.min(500,mainWindow.width-40)
        height: Math.min(500,mainWindow.height-40)
        modal: true
        standardButtons: Dialog.Close
        contentItem: ScrollView {
            ColumnLayout {
                Repeater {
                    model: mainWindow.taskWorkspace.notifications
                    Button { required property var modelData; Layout.fillWidth: true; text: mainWindow.taskWorkspace.localReminder(modelData.firedAt)+"  "+modelData.title; onClicked: {notifications.close();mainWindow.openEditor(modelData.taskId)} }
                }
                Label { visible: mainWindow.taskWorkspace.notifications.length===0; text: qsTr("暂无已触发提醒。应用关闭期间不发送通知。") }
            }
        }
    }
    onClosing: function(close) {
        if (taskWorkspace.busy) { close.accepted = false; return }
        if (recurrence.visible && recurrence.dirty) {close.accepted=false;recurrence.forceActiveFocus();return}
        if (organization.visible && organization.dirty) {close.accepted=false;organization.forceActiveFocus();return}
        if (extendedDetails.visible && extendedDetails.dirty) {close.accepted=false;extendedDetails.forceActiveFocus();return}
        if (editor.visible && draftDirty) {
            close.accepted = false
            navigateSafely(function() { mainWindow.close() })
        } else if (!forceQuit && widgetController.trayAvailable && widgetController.closeToTray) {
            close.accepted = false
            mainWindow.hide()
        } else Qt.quit()
    }
}
