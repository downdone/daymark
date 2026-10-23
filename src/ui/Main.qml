pragma ComponentBehavior: Bound
import QtQuick
import DaymarkStyle as DS
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow
    required property WidgetViewModel widgetController
    required property TaskWorkspace taskWorkspace
    required property StartupController startupController
    objectName: "mainWindow"
    width: 1120
    height: 720
    minimumWidth: 880
    minimumHeight: 580
    visible: true
    title: "Daymark"
    color: Theme.content
    font.pixelSize: Theme.bodySize
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
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "mode"; value: mainWindow.taskWorkspace.themeMode }
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "scale"; value: mainWindow.taskWorkspace.textScale }
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "compact"; value: mainWindow.taskWorkspace.preferences.compact }

    readonly property var viewCodes: ["inbox", "today", "upcoming", "anytime", "someday", "logbook"]
    readonly property var viewNames: [qsTr("收集箱"), qsTr("今天"), qsTr("即将到来"), qsTr("随时可做"), qsTr("以后再说"), qsTr("已完成")]
    property string editId: ""
    property real editVersion: 0
    property string editStatus: "OPEN"
    property bool draftDirty: false
    property bool fillingDraft: false
    property var pendingAction: null
    property bool forceQuit: false
    property bool batchMode: false
    property bool searchExpanded: false
    property bool filtersExpanded: false
    property bool taskDragging: false
    property string selectedTag: ""
    property string selectedProject: ""
    property Item editorFocusTarget: null
    property string editorFocusLayoutKey: ""
    readonly property bool toolModalOpen: organization.visible || recurrence.visible || windowSettings.visible || dataTools.visible || notifications.visible || batchDialog.visible || firstClose.visible || datePicker.visible || leaveDialog.visible || confirmToday.visible || calendarPanel.modalOpen
    property var batchSelection: []
    function toggleBatch(id,version) {
        const next=batchSelection.slice()
        const index=next.findIndex(item=>item.taskId===id)
        if(index>=0)next.splice(index,1);else next.push({taskId:id,rowVersion:version})
        batchSelection=next
    }
    readonly property bool calendarVisible: taskWorkspace.view === "month" || taskWorkspace.view === "week"
    readonly property real sidebarWidth: taskWorkspace.preferences.sidebarCollapsed ? 0 : Math.min(300,220*Theme.scale)
    readonly property real detailWidth: Math.max(320,280*Theme.scale)
    readonly property bool detailDocked: editor.visible && !calendarVisible && width >= sidebarWidth+440+detailWidth+80

    function markDirty() { if (!fillingDraft) draftDirty = true }
    function openEditor(taskId) {
        if (editor.visible && editId === taskId) { editTitle.forceActiveFocus();return }
        navigateSafely(function(){loadEditor(taskId)})
    }
    function loadEditor(taskId) {
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
        extraFields.loadTask(item)
        fillingDraft = false
        draftDirty = false
        taskWorkspace.clearError()
        editor.open()
        const flick=editorScroll
        if(flick)flick.contentY=0
        editTitle.forceActiveFocus()
    }
    function navigateSafely(action) {
        if (taskWorkspace.busy) return
        if (editor.visible && draftDirty) {
            pendingAction = action
            leaveDialog.open()
        } else action()
    }
    function saveEditor() {
        return taskWorkspace.saveTaskWithDetails(editId, editVersion, editTitle.text, editNotes.text,
                                     ["INBOX", "ACTIVE", "SOMEDAY"][planning.currentIndex],
                                     startDate.text, deadline.text, focusToday.checked,extraFields.detailsPatch())
    }
    function ensureEditorFocus(item) {
        if(!editor.visible || !item)return
        let ancestor=item
        while(ancestor && ancestor!==editorFields)ancestor=ancestor.parent
        if(!ancestor)return
        editorFocusTarget=item
        if(item===editNotes){ensureNotesCursor();return}
        const flick=editorScroll
        if(!flick)return
        const y=item.mapToItem(flick.contentItem,0,0).y
        const bottom=y+Math.min(item.height,flick.height/2)
        if(y<flick.contentY)flick.contentY=y
        else if(bottom>flick.contentY+flick.height)flick.contentY=bottom-flick.height+8
    }
    onActiveFocusItemChanged: {
        if(activeFocusItem)editorFocusTarget=activeFocusItem
        editorFocusDelay.restart()
    }
    Timer {id: editorFocusDelay;interval: 16;onTriggered: mainWindow.ensureEditorFocus(mainWindow.editorFocusTarget)}
    // Nested layouts can move a focused child after their parent's height signal.
    // Observe the settled geometry after polish, without undoing manual scrolling.
    onAfterAnimating: if(editor.visible)Qt.callLater(mainWindow.followEditorLayout)
    function followEditorLayout() {
        const item=activeFocusItem
        if(!editor.visible || !item)return
        let ancestor=item
        while(ancestor && ancestor!==editorFields)ancestor=ancestor.parent
        if(!ancestor)return
        const position=item.mapToItem(editorFields,0,0)
        const key=[position.y,item.height,editorScroll.width,editorScroll.height].join(":")
        if(item===editorFocusTarget && key===editorFocusLayoutKey)return
        editorFocusLayoutKey=key
        if(item===editNotes)ensureNotesCursor();else ensureEditorFocus(item)
    }
    function ensureNotesCursor() {
        if(!editor.visible || fillingDraft || !editNotes.activeFocus)return
        const flick=editorScroll
        if(!flick)return
        const position=editNotes.mapToItem(flick.contentItem,0,editNotes.cursorRectangle.y)
        const bottom=position.y+editNotes.cursorRectangle.height+8
        if(position.y<flick.contentY)flick.contentY=position.y
        else if(bottom>flick.contentY+flick.height)flick.contentY=bottom-flick.height
    }
    function changeView(view) {
        navigateSafely(function(){
            viewPositions[taskWorkspace.view]=({y:taskList.contentY,index:taskList.currentIndex})
            editor.close();batchMode=false;batchSelection=[];taskWorkspace.setView(view)
            Qt.callLater(function(){const saved=viewPositions[view];if(saved){taskList.currentIndex=saved.index;taskList.contentY=Math.max(taskList.originY,Math.min(saved.y,taskList.originY+Math.max(0,taskList.contentHeight-taskList.height)))}else taskList.positionViewAtBeginning();taskList.forceActiveFocus()})
        })
    }
    property var viewPositions: ({})
    property var todayTarget: ({})
    function addToToday(id) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(!item.taskId)return
            todayTarget=item
            if(item.startOn>taskWorkspace.dateOffset(0))confirmToday.open();else applyToday()
        })
    }
    function applyToday() {
        const item=taskWorkspace.task(todayTarget.taskId)
        if(item.taskId){editor.close();taskWorkspace.saveTask(item.taskId,item.rowVersion,item.title,item.notes,"ACTIVE",item.startOn,item.deadlineOn,true)}
    }
    function focusHeader(item) {
        const flick=headerScroll.contentItem as Flickable
        if(flick)flick.contentY=Math.max(0,Math.min(item.y-8,flick.contentHeight-flick.height))
        item.forceActiveFocus()
    }
    function newTask(view) {navigateSafely(function(){editor.close();if(view)changeView(view);else if(calendarVisible || taskWorkspace.view==="logbook")changeView("inbox");Qt.callLater(function(){focusHeader(newTitle)})})}
    function canNavigateByKey() {return !editor.visible && !toolModalOpen && !(activeFocusItem instanceof TextInput) && !(activeFocusItem instanceof TextEdit)}
    function changeTaskState(id,action) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(item.taskId){editor.close();taskWorkspace.changeState(id,item.rowVersion,action)}
        })
    }
    function assignTask(id,kind,parentId) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(item.taskId){editor.close();taskWorkspace.saveDetails(id,item.rowVersion,kind === "project" ? {projectId:parentId} : {areaId:parentId})}
        })
    }
    function setTaskEvening(id,evening) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(item.taskId){editor.close();taskWorkspace.saveDetails(id,item.rowVersion,{evening:evening})}
        })
    }
    function openTool(name) {
        navigateSafely(function(){
            editor.close()
            if(name === "organization")organization.open()
            else if(name === "recurrence")recurrence.open()
            else if(name === "notifications")notifications.open()
            else if(name === "settings")windowSettings.open()
            else if(name === "data")dataTools.open()
        })
    }
    function currentViewTitle() {
        const index=viewCodes.indexOf(taskWorkspace.view)
        if(index>=0)return viewNames[index]
        if(calendarVisible)return qsTr("日历")
        for(const item of taskWorkspace.catalog)if(taskWorkspace.view.endsWith(":"+item.id))return item.title
        return qsTr("任务")
    }
    function showVerificationPage(page,taskId) {
        editor.close();organization.close();recurrence.close();windowSettings.close();dataTools.close();datePicker.close();styleGallery.close()
        if(page === "details")openEditor(taskId)
        else if(page === "organization")organization.open()
        else if(page === "recurrence")recurrence.open()
        else if(page === "settings")windowSettings.open()
        else if(page === "data")dataTools.open()
        else if(page === "date")datePicker.showFor(taskWorkspace.dateOffset(0),"start")
        else if(page === "editor")openEditor(taskId)
        else if(page === "batch"){batchDialog.selection=[taskWorkspace.task(taskId)];batchDialog.open()}
        else if(page === "style")styleGallery.open()
        else if(page === "event")calendarPanel.openEvent(taskId)
        else taskWorkspace.setView(page)
    }

    Shortcut { sequences: [StandardKey.New];enabled: !mainWindow.toolModalOpen;onActivated: mainWindow.newTask("") }
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+1" : "Ctrl+1";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("today")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+2" : "Ctrl+2";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("upcoming")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+3" : "Ctrl+3";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("anytime")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+4" : "Ctrl+4";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("someday")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+5" : "Ctrl+5";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("month")}
    Shortcut {sequences: [StandardKey.Preferences];enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.openTool("settings")}
    Shortcut { sequences: [StandardKey.Find];enabled: !mainWindow.toolModalOpen;onActivated: { if (!editor.visible || mainWindow.detailDocked) {mainWindow.searchExpanded=true;Qt.callLater(function(){mainWindow.focusHeader(search)})} } }
    Shortcut { sequence: "Escape";enabled: editor.visible && !leaveDialog.visible && !datePicker.visible && !startDate.pickerVisible && !deadline.pickerVisible && !removeToday.visible;onActivated: mainWindow.navigateSafely(function(){editor.close();taskList.forceActiveFocus()}) }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            visible: !mainWindow.taskWorkspace.preferences.sidebarCollapsed
            Layout.preferredWidth: mainWindow.sidebarWidth
            Layout.maximumWidth: mainWindow.sidebarWidth
            Layout.fillHeight: true
            color: Theme.sidebar
            ScrollView {
                id: navigationScroll
                objectName: "navigationScroll"
                anchors.fill: parent
                padding: 16
                clip: true
            ColumnLayout {
                width: navigationScroll.availableWidth
                height: Math.max(implicitHeight,navigationScroll.availableHeight)
                spacing: 8
                Label { text: "Daymark"; font.pixelSize: Theme.headingSize; font.bold: true; Layout.bottomMargin: 22 }
                Repeater {
                    model: mainWindow.viewNames
                    NavRow {
                        required property int index
                        required property string modelData
                        Layout.fillWidth: true
                        text: modelData
                        selected: mainWindow.taskWorkspace.view === mainWindow.viewCodes[index]
                        symbol: ["inbox","today","upcoming","circle","someday","logbook"][index]
                        count: index===1 ? mainWindow.taskWorkspace.todayTasks.count : -1
                        acceptsTaskDrop: index===1
                        onTaskDropped: function(taskId,version){mainWindow.addToToday(taskId)}
                        onClicked: mainWindow.changeView(mainWindow.viewCodes[index])
                    }
                }
                ColumnLayout {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                        Repeater {
                            model: mainWindow.taskWorkspace.catalog
                            NavRow {
                                required property var modelData
                                visible: modelData.kind !== "heading" && !modelData.hidden
                                Layout.fillWidth: true
                                text: modelData.title + (modelData.status !== "OPEN" ? qsTr("（历史）") : "")
                                symbol: modelData.kind
                                symbolColor: Theme.catalogColor(modelData.colorKey)
                                selected: mainWindow.taskWorkspace.view === modelData.kind+":"+modelData.id
                                acceptsTaskDrop: modelData.kind === "project" || modelData.kind === "area"
                                onTaskDropped: function(taskId,version) {mainWindow.assignTask(taskId,modelData.kind,modelData.id)}
                                onClicked: mainWindow.changeView(modelData.kind+":"+modelData.id)
                            }
                        }
                }
                NavRow { Layout.fillWidth: true; text: qsTr("日历"); symbol: "calendar"; selected: mainWindow.calendarVisible; onClicked: mainWindow.changeView("month") }
                Button {
                    objectName: "settingsAndTools"
                    Layout.fillWidth: true
                    text: qsTr("设置与工具…")
                    onClicked: toolsMenu.open()
                    Menu {
                        id: toolsMenu
                        MenuItem { text: qsTr("管理领域、项目与标签…"); onTriggered: mainWindow.openTool("organization") }
                        MenuItem { text: qsTr("重复任务规则…"); onTriggered: mainWindow.openTool("recurrence") }
                        MenuItem { text: qsTr("提醒中心（%1）").arg(mainWindow.taskWorkspace.notifications.length); onTriggered: mainWindow.openTool("notifications") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("外观、窗口与组件…"); onTriggered: mainWindow.openTool("settings") }
                        MenuItem { text: qsTr("数据与备份…"); enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy; onTriggered: mainWindow.openTool("data") }
                        MenuSeparator {}
                        MenuItem { text: qsTr("退出 Daymark"); onTriggered: {mainWindow.forceQuit=true;mainWindow.close()} }
                    }
                }
                Label { text: qsTr("本地保存 · 无需登录"); color: Theme.secondary; font.pixelSize: Theme.captionSize }
            }
            }
        }
        ColumnLayout {
            objectName: "taskContent"
            visible: !editor.visible || mainWindow.detailDocked
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 24
            Layout.rightMargin: mainWindow.detailDocked ? mainWindow.detailWidth+40 : 24
            spacing: Theme.fieldGap
            RowLayout {
                ToolButton {Accessible.name: mainWindow.taskWorkspace.preferences.sidebarCollapsed ? qsTr("显示导航") : qsTr("收起导航");contentItem: AppIcon {name: "sidebar"} onClicked: mainWindow.taskWorkspace.setPreference("sidebarCollapsed",!mainWindow.taskWorkspace.preferences.sidebarCollapsed)}
                Label {
                    text: search.text.trim().length > 0 ? qsTr("搜索结果") : mainWindow.currentViewTitle()
                    font.pixelSize: Theme.titleSize
                    elide: Text.ElideRight
                    font.bold: true
                    Layout.fillWidth: true
                }
                BusyIndicator { running: mainWindow.taskWorkspace.busy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                Button {visible: !mainWindow.calendarVisible;text: qsTr("搜索");flat: true;onClicked: {if(mainWindow.searchExpanded || search.text.length>0){search.clear();mainWindow.searchExpanded=false;taskList.forceActiveFocus()}else{mainWindow.searchExpanded=true;search.forceActiveFocus()}}}
                Button {visible: !mainWindow.calendarVisible;text: (mainWindow.selectedTag || mainWindow.selectedProject) ? qsTr("清除筛选") : mainWindow.filtersExpanded ? qsTr("收起筛选") : qsTr("筛选");flat: true;onClicked: {if(mainWindow.selectedTag || mainWindow.selectedProject){tagFilter.currentIndex=0;projectFilter.currentIndex=0;mainWindow.selectedTag="";mainWindow.selectedProject="";mainWindow.taskWorkspace.setTagFilter("");mainWindow.taskWorkspace.setProjectFilter("");mainWindow.filtersExpanded=false}else mainWindow.filtersExpanded=!mainWindow.filtersExpanded}}
                ToolButton {
                    Accessible.name: qsTr("页面操作")
                    contentItem: AppIcon {name: "more"}
                    onClicked: pageMenu.open()
                    Menu {
                        id: pageMenu
                        x: parent.width-width
                        y: parent.height
                        MenuItem {text: mainWindow.batchMode ? qsTr("退出多选") : qsTr("选择多个任务…");enabled: !mainWindow.calendarVisible;onTriggered: mainWindow.navigateSafely(function(){editor.close();mainWindow.batchMode=!mainWindow.batchMode;mainWindow.batchSelection=[]})}
                        MenuItem {text: qsTr("重新加载");enabled: !mainWindow.taskWorkspace.busy;onTriggered: mainWindow.navigateSafely(function(){editor.close();mainWindow.taskWorkspace.reload()})}
                    }
                }
            }
            ScrollView {
                id: headerScroll
                visible: !mainWindow.calendarVisible
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(taskHeader.implicitHeight,mainWindow.height*0.45)
                clip: true
                ColumnLayout {
                id: taskHeader
                width: headerScroll.availableWidth
                spacing: 8
            TextField {
                id: search
                objectName: "taskSearch"
                visible: !mainWindow.calendarVisible && (mainWindow.searchExpanded || text.length>0)
                Layout.fillWidth: true
                placeholderText: qsTr("搜索任务、备注与清单…")
                Accessible.name: qsTr("全局搜索任务、备注与清单")
                onTextChanged: mainWindow.taskWorkspace.setSearch(text)
                Keys.onEscapePressed: {clear();mainWindow.searchExpanded=false;taskList.forceActiveFocus()}
                selectByMouse: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: !mainWindow.calendarVisible && (mainWindow.filtersExpanded || mainWindow.selectedTag.length>0 || mainWindow.selectedProject.length>0 || search.text.length>0 || mainWindow.taskWorkspace.view.startsWith("project:"))
                CheckBox { visible: search.text.trim().length>0 || mainWindow.taskWorkspace.view.startsWith("project:"); text: qsTr("包括已完成/取消"); checked: mainWindow.taskWorkspace.includeCompleted; onClicked: mainWindow.taskWorkspace.includeCompleted=checked }
                RowLayout {
                Layout.fillWidth: true
                visible: mainWindow.filtersExpanded || !!mainWindow.selectedTag || !!mainWindow.selectedProject
                ComboBox {id: tagFilter;property var tags: mainWindow.taskWorkspace.catalogOf("tag"); Layout.fillWidth: true; model: [qsTr("全部标签")].concat(tags.map(function(t){return t.title})); onActivated: {mainWindow.selectedTag=currentIndex===0 ? "" : tags[currentIndex-1].id;mainWindow.taskWorkspace.setTagFilter(mainWindow.selectedTag)} }
                ComboBox {id: projectFilter;property var projects: mainWindow.taskWorkspace.catalogOf("project");Layout.fillWidth: true;model: [qsTr("全部项目")].concat(projects.map(item=>item.title));onActivated: {mainWindow.selectedProject=currentIndex===0 ? "" : projects[currentIndex-1].id;mainWindow.taskWorkspace.setProjectFilter(mainWindow.selectedProject)}}
                ToolButton {text: qsTr("清除");Accessible.name: qsTr("清除标签和项目筛选");enabled: !!mainWindow.selectedTag || !!mainWindow.selectedProject;onClicked: {tagFilter.currentIndex=0;projectFilter.currentIndex=0;mainWindow.selectedTag="";mainWindow.selectedProject="";mainWindow.taskWorkspace.setTagFilter("");mainWindow.taskWorkspace.setProjectFilter("")}}
                }
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
                enabled: mainWindow.taskWorkspace.ready && (!mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy) && mainWindow.taskWorkspace.view !== "logbook"
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
                Button { text: qsTr("添加");highlighted: true;enabled: !mainWindow.taskWorkspace.busy && newTitle.text.trim().length>0;onClicked: mainWindow.taskWorkspace.createTask(newTitle.text) }
            }
            Label {
                visible: !mainWindow.calendarVisible && (mainWindow.taskWorkspace.view==="someday" || mainWindow.taskWorkspace.tasks.count===0)
                text: mainWindow.taskWorkspace.view === "someday" ? qsTr("日期已暂停，激活后重新进入执行视图。") : qsTr("在上方收集新任务，点击列表可安排日期和归属。")
                color: Theme.secondary
                font.pixelSize: Theme.captionSize
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Label { visible: mainWindow.taskWorkspace.view === "today";Layout.fillWidth: true;wrapMode: Text.Wrap;text: mainWindow.taskWorkspace.localToday+qsTr(" · 待办 %1 · 今日完成 %2").arg(mainWindow.taskWorkspace.todayTasks.count).arg(mainWindow.taskWorkspace.completedToday); color: Theme.secondary;font.pixelSize: Theme.captionSize }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                visible: !mainWindow.calendarVisible && (mainWindow.batchMode || mainWindow.taskWorkspace.view==="logbook")
                Button {visible: mainWindow.batchMode;text: qsTr("退出多选");onClicked: {mainWindow.batchMode=false;mainWindow.batchSelection=[]}}
                Button {visible: mainWindow.batchMode;text: qsTr("批量修改（%1）").arg(mainWindow.batchSelection.length);enabled: mainWindow.batchSelection.length>0;onClicked: {batchDialog.selection=mainWindow.batchSelection;batchDialog.open()}}
                ComboBox {visible: mainWindow.taskWorkspace.view==="logbook";model: [qsTr("全部历史"),qsTr("近7天"),qsTr("近30天")];onActivated: mainWindow.taskWorkspace.historyDays=[0,7,30][currentIndex]}
            }
            RowLayout {
                visible: mainWindow.taskWorkspace.view === "today" && mainWindow.taskDragging
                Repeater {
                    model: [qsTr("拖入今天"),qsTr("拖入今晚")]
                    Label {
                        id: eveningDrop
                        required property string modelData
                        required property int index
                        text: modelData
                        padding: 6
                        font.pixelSize: Theme.captionSize
                        color: Theme.accent
                        background: Rectangle {color: Theme.hover;radius: 5}
                        DropArea {anchors.fill: parent;onDropped: function(drop){const source=drop.source as TaskRow;if(source){mainWindow.setTaskEvening(source.taskId,eveningDrop.index===1);drop.acceptProposedAction()}}}
                    }
                }
            }
                }
            }
            CalendarPanel {
                id: calendarPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainWindow.calendarVisible
                workspace: mainWindow.taskWorkspace
                onOpenTask: function(taskId) {mainWindow.openEditor(taskId)}
                onCompleteTask: function(id,version,action) {mainWindow.navigateSafely(function(){editor.close();mainWindow.taskWorkspace.changeState(id,version,action)})}
            }
            ListView {
                DS.ScrollPhysics {target: taskList}
                id: taskList
                objectName: "taskList"
                visible: !mainWindow.calendarVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Layout.minimumHeight: visible ? 96 : 0
                spacing: 6
                focus: true
                keyNavigationEnabled: true
                keyNavigationWraps: true
                section.property: "viewSection"
                section.delegate: Label { required property string section; text: section; color: Theme.secondary; font.pixelSize: Theme.sectionSize; height: section.length>0 ? implicitHeight+20 : 0;visible: section.length>0;verticalAlignment: Text.AlignVCenter }
                Keys.onReturnPressed: {const item=taskList.currentItem as TaskRow;if(item){if(mainWindow.batchMode)item.selectionRequested();else item.editRequested()}}
                Keys.onSpacePressed: {const item=taskList.currentItem as TaskRow;if(item){if(mainWindow.batchMode)item.selectionRequested();else item.completeRequested()}}
                model: mainWindow.taskWorkspace.tasks
                ScrollBar.vertical: ScrollBar {}
                delegate: TaskRow {
                    required property int index
                    width: taskList.width
                    enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy
                    highlighted: taskList.currentIndex === index
                    selectionMode: mainWindow.batchMode
                    canMoveUp: index>0
                    canMoveDown: index<taskList.count-1
                    onMenuRequested: {canMoveUp=mainWindow.taskWorkspace.canMoveTask(taskId,-1);canMoveDown=mainWindow.taskWorkspace.canMoveTask(taskId,1)}
                    onEveningRequested: function(value){mainWindow.setTaskEvening(taskId,value)}
                    onDragStateChanged: function(active){mainWindow.taskDragging=active}
                    selectedForBatch: mainWindow.batchSelection.some(item=>item.taskId===taskId)
                    onSelectionRequested: mainWindow.toggleBatch(taskId,rowVersion)
                    onEditRequested: {taskList.currentIndex=index;mainWindow.openEditor(taskId)}
                    onCompleteRequested: mainWindow.changeTaskState(taskId,isDone ? "reopen" : "complete")
                    onActivateRequested: {const id=taskId;mainWindow.navigateSafely(function(){const item=mainWindow.taskWorkspace.task(id);editor.close();mainWindow.taskWorkspace.saveTask(id,item.rowVersion,item.title,item.notes,"ACTIVE","",item.deadlineOn,false)})}
                    onMoveRequested: function(direction) {const id=taskId;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.moveTask(id,direction)})}
                    onDropRequested: function(source) {const target=taskId;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.dropTask(source,target)})}
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
                Label { text: mainWindow.calendarVisible ? qsTr("任务与事件") : qsTr("%1 项").arg(mainWindow.taskWorkspace.tasks.count); color: Theme.secondary; Layout.fillWidth: true;elide: Text.ElideRight }
                Button {visible: mainWindow.taskWorkspace.view === "upcoming";text: qsTr("未来%1天 · 再显示30天").arg(mainWindow.taskWorkspace.upcomingDays);onClicked: mainWindow.taskWorkspace.loadMoreUpcoming()}
                Button { visible: mainWindow.taskWorkspace.canUndo; text: qsTr("撤销删除"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.taskWorkspace.undoDelete() }
                Button {visible: mainWindow.taskWorkspace.canUndoEvent;text: qsTr("撤销删除事件");enabled: !mainWindow.taskWorkspace.busy;onClicked: mainWindow.taskWorkspace.undoEventDelete()}
                Label { text: mainWindow.taskWorkspace.busy ? qsTr("正在保存…") : mainWindow.taskWorkspace.ready ? qsTr("已保存在本机") : qsTr("数据未就绪"); color: Theme.secondary }
            }
        }
    }

    Pane {
        id: editor
        objectName: "taskEditor"
        visible: false
        z: 1
        x: mainWindow.detailDocked ? mainWindow.width-width-16 : mainWindow.sidebarWidth+16
        y: 16
        width: mainWindow.detailDocked ? mainWindow.detailWidth : mainWindow.width-mainWindow.sidebarWidth-32
        height: mainWindow.height-32
        padding: 0
        function open() {visible=true}
        function close() {startDate.closePicker();deadline.closePicker();visible=false}
        onWidthChanged: if(visible)Qt.callLater(function(){mainWindow.ensureEditorFocus(mainWindow.activeFocusItem)})
        onHeightChanged: if(visible)Qt.callLater(function(){mainWindow.ensureEditorFocus(mainWindow.activeFocusItem)})
        background: Rectangle {color: Theme.content;radius: Theme.panelRadius;border.color: Theme.separator}
        contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Label {text: qsTr("任务详情");font.pixelSize: Theme.headingSize;font.bold: true;Layout.fillWidth: true}
            Label {text: mainWindow.draftDirty ? qsTr("未保存") : qsTr("本地");font.pixelSize: Theme.captionSize;color: Theme.secondary}
        }
        Flickable {
            DS.ScrollPhysics {target: editorScroll}
            id: editorScroll
            objectName: "taskEditorScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            contentWidth: width
            contentHeight: editorFields.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            clip: true
            onContentHeightChanged: if(editor.visible)editorFocusDelay.restart()
            ScrollBar.vertical: ScrollBar {
                objectName: "editorScrollBar"
                x: editorScroll.width-width
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {implicitWidth: 6;implicitHeight: 6;radius: 3;color: Theme.secondary}
            }
            ColumnLayout {
            id: editorFields
            onHeightChanged: if(editor.visible)editorFocusDelay.restart()
            width: editorScroll.width-12
            spacing: Theme.fieldGap
            enabled: !mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy
            TextField { id: editTitle; objectName: "editTaskTitle"; Layout.fillWidth: true; placeholderText: qsTr("任务标题");Accessible.name: qsTr("任务标题");font.bold: true;selectByMouse: true; onTextChanged: mainWindow.markDirty() }
            TextArea { id: editNotes;objectName: "editTaskNotes";Layout.fillWidth: true;implicitHeight: Math.max(72,Theme.bodySize*3,contentHeight+topPadding+bottomPadding);placeholderText: qsTr("添加备注…");Accessible.name: qsTr("备注");wrapMode: TextEdit.Wrap; selectByMouse: true; onTextChanged: mainWindow.markDirty();onCursorRectangleChanged: Qt.callLater(mainWindow.ensureNotesCursor) }
            ColumnLayout {
                SectionLabel { text: qsTr("规划与日期") }
                ComboBox { id: planning;objectName: "editorPlanning";Layout.fillWidth: true;Accessible.name: qsTr("任务规划状态");model: [qsTr("收集箱"), qsTr("已激活"), qsTr("以后再说")]; onCurrentIndexChanged: mainWindow.markDirty() }
                CheckBox { id: focusToday; text: qsTr("持续关注今天"); onCheckedChanged: mainWindow.markDirty();onClicked: if(!checked && mainWindow.taskWorkspace.todayAfterRemovingFocus(mainWindow.editId))removeToday.open() }
            }
            GridLayout {
                columns: 1
                Layout.fillWidth: true
                Label { text: qsTr("计划开始");Layout.fillWidth: true;wrapMode: Text.Wrap }
                DateField { id: startDate; objectName: "editStartDate";workspace: mainWindow.taskWorkspace;dateLabel: qsTr("计划开始"); Layout.fillWidth: true; onTextChanged: mainWindow.markDirty() }
                Label { text: qsTr("截止日期");Layout.fillWidth: true;wrapMode: Text.Wrap }
                DateField { id: deadline;workspace: mainWindow.taskWorkspace;dateLabel: qsTr("截止日期");Layout.fillWidth: true; onTextChanged: mainWindow.markDirty() }
            }
            Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: planning.currentIndex===2 ? qsTr("以后再说：保留日期，暂停执行。") : qsTr("安排日期或关注今天会激活任务。"); font.pixelSize: Theme.captionSize; color: Theme.secondary }
            TaskDetailsFields {
                id: extraFields
                onYChanged: if(editor.visible)editorFocusDelay.restart()
                onHeightChanged: if(editor.visible)editorFocusDelay.restart()
                objectName: "taskDetailsFields"
                Layout.fillWidth: true
                workspace: mainWindow.taskWorkspace
                onFocusRequested: function(item){mainWindow.editorFocusTarget=item;editorFocusDelay.restart()}
                onDraftChanged: mainWindow.markDirty()
                onOpenEvent: function(id){mainWindow.navigateSafely(function(){editor.close();calendarPanel.openEvent(id)})}
            }
            }
        }
        Label {Layout.fillWidth: true;Layout.leftMargin: 16;Layout.rightMargin: 16;wrapMode: Text.Wrap;visible: mainWindow.taskWorkspace.errorText.length>0;text: mainWindow.taskWorkspace.errorText;color: Theme.danger}
        ActionBar {
            Layout.fillWidth: true
            gap: 8
            ToolButton {
                Accessible.name: qsTr("更多任务操作")
                contentItem: AppIcon {name: "more"}
                onClicked: editorMenu.open()
                Menu {
                    id: editorMenu
                    MenuItem {text: qsTr("取消任务");enabled: mainWindow.editStatus==="OPEN" && !mainWindow.taskWorkspace.busy;onTriggered: mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.changeState(mainWindow.editId,mainWindow.editVersion,"cancel");editor.close()})}
                    MenuItem {text: qsTr("删除任务");enabled: !mainWindow.taskWorkspace.busy;onTriggered: mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.changeState(mainWindow.editId,mainWindow.editVersion,"delete");editor.close()})}
                }
            }
            Button { objectName: "returnFromEditor"; text: qsTr("返回"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.navigateSafely(function() { editor.close() }) }
            Button {objectName: "saveTaskEditor";text: qsTr("保存"); enabled: !mainWindow.taskWorkspace.busy; highlighted: true; onClicked: { mainWindow.pendingAction = null; mainWindow.saveEditor() } }
        }
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
            Button { objectName: "saveDraftThenContinue";text: qsTr("保存后继续"); onClicked: { leaveDialog.close(); mainWindow.saveEditor() } }
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
            spacing: Theme.fieldGap
            SectionLabel {text: qsTr("外观")}
            Label { text: qsTr("主题") }
            ComboBox { model: [qsTr("跟随系统"),qsTr("浅色"),qsTr("深色")]; currentIndex: mainWindow.taskWorkspace.themeMode; onActivated: mainWindow.taskWorkspace.themeMode=currentIndex }
            Label { text: qsTr("文字大小 %1%").arg(Math.round(mainWindow.taskWorkspace.textScale*100)) }
            Slider { from: 1; to: 2; stepSize: 0.25; value: mainWindow.taskWorkspace.textScale; onMoved: mainWindow.taskWorkspace.textScale=value }
            CheckBox {text: qsTr("紧凑任务列表");checked: mainWindow.taskWorkspace.preferences.compact;onClicked: mainWindow.taskWorkspace.setPreference("compact",checked)}
            CheckBox {text: qsTr("桌面组件使用不透明背景");checked: mainWindow.taskWorkspace.preferences.solidWidget;onClicked: mainWindow.taskWorkspace.setPreference("solidWidget",checked)}
            SectionLabel {text: qsTr("启动与数据")}
            CheckBox {text: qsTr("开机启动（当前用户）");enabled: mainWindow.startupController.supported;checked: mainWindow.startupController.enabled;onClicked: mainWindow.startupController.setEnabled(checked)}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.danger;text: mainWindow.startupController.errorText}
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;text: qsTr("开机启动登记当前程序路径；移动程序后请重新开启。系统策略可能禁用启动项。")}
            Label {text: qsTr("自动备份周期（启动时检查，保留最近7份）")}
            ComboBox {model: [qsTr("每天"),qsTr("每7天"),qsTr("每30天")];currentIndex: [1,7,30].indexOf(mainWindow.taskWorkspace.preferences.backupDays);onActivated: mainWindow.taskWorkspace.setPreference("backupDays",[1,7,30][currentIndex])}
            CheckBox {text: qsTr("提交系统提醒（应用内记录始终保留）");checked: mainWindow.taskWorkspace.preferences.systemReminders;onClicked: mainWindow.taskWorkspace.setPreference("systemReminders",checked)}
            SectionLabel {text: qsTr("桌面组件")}
            Button { text: mainWindow.widgetController.visible ? qsTr("隐藏组件") : qsTr("显示组件"); onClicked: mainWindow.widgetController.setVisible(!mainWindow.widgetController.visible) }
            Button { text: mainWindow.widgetController.mini ? qsTr("展开组件") : qsTr("切换迷你条"); onClicked: mainWindow.widgetController.setMini(!mainWindow.widgetController.mini) }
            CheckBox { text: qsTr("保持置顶"); checked: mainWindow.widgetController.alwaysOnTop; onClicked: mainWindow.widgetController.setAlwaysOnTop(checked) }
            CheckBox { text: qsTr("鼠标穿透（从托盘或此处恢复）"); enabled: mainWindow.widgetController.trayAvailable; checked: mainWindow.widgetController.clickThrough; onClicked: mainWindow.widgetController.setClickThrough(checked) }
            CheckBox { text: qsTr("锁定拖动"); checked: mainWindow.widgetController.locked; onClicked: mainWindow.widgetController.setLocked(checked) }
            CheckBox { text: qsTr("关闭主窗口到托盘"); enabled: mainWindow.widgetController.trayAvailable; checked: mainWindow.widgetController.closeToTray; onClicked: mainWindow.widgetController.setCloseToTray(checked) }
            Label { text: qsTr("背景透明度") }
            Label {Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary;text: qsTr("组件的显示/隐藏状态会用于下次启动；穿透状态不跨重启恢复。")}
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
    WidgetWindow {
        controller: mainWindow.widgetController
        workspace: mainWindow.taskWorkspace
        onNewTaskRequested: {mainWindow.showNormal();mainWindow.requestActivate();mainWindow.newTask("today")}
        onOpenMainRequested: {mainWindow.showNormal();mainWindow.requestActivate()}
        onSettingsRequested: {mainWindow.showNormal();mainWindow.requestActivate();mainWindow.openTool("settings")}
    }
    OrganizationDialog { id: organization; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    StyleGallery {id: styleGallery;parent: Overlay.overlay;anchors.centerIn: parent}
    BatchTaskDialog {id: batchDialog;parent: Overlay.overlay;anchors.centerIn: parent;workspace: mainWindow.taskWorkspace;onCommitted: {mainWindow.batchSelection=[];mainWindow.batchMode=false}}
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
        footer: ActionBar {
            gap: 8
            Button {text: qsTr("保留");onClicked: {focusToday.checked=mainWindow.taskWorkspace.task(mainWindow.editId).todayFocus;removeToday.close()}}
            Button {text: qsTr("重新安排…");onClicked: {removeToday.close();datePicker.showFor(startDate.text,"start")}}
            Button {text: qsTr("以后再说");onClicked: {planning.currentIndex=2;removeToday.close()}}
        }
    }
    Dialog {
        id: firstClose
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("Daymark 将继续在托盘运行")
        width: Math.min(460,mainWindow.width-40)
        height: Math.min(230*Theme.scale,mainWindow.height-40)
        modal: true
        closePolicy: Popup.NoAutoClose
        contentItem: Label {wrapMode: Text.Wrap;text: qsTr("关闭主窗口后，提醒和桌面组件仍会运行。可从托盘打开主窗或退出；设置中可改为关闭即退出。")}
        footer: DialogButtonBox {Button {text: qsTr("知道了，隐藏主窗口");onClicked: {if(mainWindow.taskWorkspace.setPreference("closeNoticeSeen",true)){firstClose.close();mainWindow.hide()}}}}
    }
    Dialog {
        id: confirmToday
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("将未来任务加入今天？")
        width: Math.min(460,mainWindow.width-40)
        height: Math.min(210*Theme.scale,mainWindow.height-40)
        modal: true
        standardButtons: Dialog.Ok|Dialog.Cancel
        contentItem: Label {wrapMode: Text.Wrap;text: qsTr("计划开始日期将从 %1 调整为今天，截止日期保持不变。").arg(mainWindow.todayTarget.startOn || "")}
        onAccepted: mainWindow.applyToday()
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
        if(calendarPanel.protectClose(function(){mainWindow.close()})){close.accepted=false;return}
        if (recurrence.visible && recurrence.dirty) {close.accepted=false;recurrence.forceActiveFocus();return}
        if (organization.visible && organization.dirty) {close.accepted=false;organization.forceActiveFocus();return}
        if (editor.visible && draftDirty) {
            close.accepted = false
            navigateSafely(function() { mainWindow.close() })
        } else if (!forceQuit && widgetController.trayAvailable && widgetController.closeToTray) {
            close.accepted = false
            if(!taskWorkspace.preferences.closeNoticeSeen)firstClose.open();else mainWindow.hide()
        } else Qt.quit()
    }
}
