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
    font.family: Theme.fontFamily
    palette.window: Theme.content
    palette.windowText: Theme.text
    palette.base: Theme.content
    palette.text: Theme.text
    palette.placeholderText: Theme.secondary
    palette.button: Theme.sidebar
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.mid: Theme.separator
    palette.dark: Theme.secondary
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "mode"; value: mainWindow.taskWorkspace.themeMode }
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "scale"; value: mainWindow.taskWorkspace.textScale }
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "highContrast"; value: mainWindow.taskWorkspace.preferences.highContrast }
    Binding { restoreMode: Binding.RestoreNone; target: Theme; property: "compact"; value: mainWindow.taskWorkspace.preferences.compact }

    readonly property var viewCodes: ["today", "scheduled", "all", "important", "completed"]
    readonly property var viewNames: [qsTr("今天"), qsTr("计划"), qsTr("全部"), qsTr("特别关注"), qsTr("完成")]
    property string editId: ""
    property string editorRootId: ""
    property string editorRootTitle: ""
    property string editorReturnTaskId: ""
    readonly property bool editorIsSubtask: !!(taskWorkspace.task(editId).parentTaskId || "")
    readonly property bool editorHasChildren: taskWorkspace.subtaskChoices(editId).length>0
    property int editorTab: 0
    property real editVersion: 0
    property string editStatus: "OPEN"
    property string draftReminderZone: "UTC"
    property bool draftDirty: false
    property bool draftTodayFocus: false
    property bool draftDatesDirty: false
    property string originalStartOn: ""
    property string originalEndOn: ""
    property bool timeDatesDiffer: false
    property string draftPlanningState: "ACTIVE"
    property var pendingNavigationMove: ({})
    property bool draftEvening: false
    property bool editorSaveError: false
    property bool editorSaving: false
    property Item quickFocusTarget: null
    property bool quickManualScroll: false
    property Item taskPointerRow: null
    property Item draggedTaskRow: null
    property Item completionRow: null
    property Item inlineRow: null
    property int inlineAttachAttempts: 0
    property bool inlineAttachWanted: false
    property var collapsedTaskIds: []
    property var collapsedCategoryIds: []
    property var catalogContextItem: ({})
    property Item catalogContextAnchor: null
    property bool catalogDragActive: false
    property Item draggedCatalogRow: null
    property bool fillingDraft: false
    property var pendingAction: null
    property bool shuttingDown: false
    property bool forceQuit: false
    property bool batchMode: false
    property bool searchExpanded: false
    property bool filtersExpanded: false
        property string selectedTag: ""
    property string selectedProject: ""
    property Item editorFocusTarget: null
    property bool editorManualScroll: false
    property string editorFocusLayoutKey: ""
    Binding {restoreMode: Binding.RestoreNone;target: mainWindow.taskWorkspace;property: "interactionActive";value: !!mainWindow.taskPointerRow || mainWindow.catalogDragActive}
    readonly property bool editingActive: editor.visible || quickEditor.editing
    readonly property bool transientMenuOpen: libraryMenu.visible || libraryCreateMenu.visible || categoryContextMenu.visible || catalogDeleteConfirm.visible || pageMenu.visible || categorySectionMenu.visible || tagSectionMenu.visible
    readonly property bool toolModalOpen: transientMenuOpen || catalogEdit.visible || deleteParentConfirm.visible || catalogCreate.visible || quickEditor.editing || organization.modalOpen || windowSettings.visible || dataTools.visible || batchDialog.visible || firstClose.visible || datePicker.visible || leaveDialog.visible || confirmToday.visible || calendarPanel.modalOpen || timingFields.modalOpen || extraFields.parentPickerVisible || startDate.pickerVisible || deadline.pickerVisible || reminder.pickerVisible
    property string toolPage: ""
    readonly property bool toolPageVisible: toolPage.length>0
    readonly property bool taskContentVisible: !toolPageVisible && !calendarVisible
    property var batchSelection: []
    function toggleBatch(id,version) {
        const next=batchSelection.slice()
        const index=next.findIndex(item=>item.taskId===id)
        if(index>=0)next.splice(index,1);else next.push({taskId:id,rowVersion:version})
        batchSelection=next
    }
    readonly property bool calendarVisible: !toolPageVisible && (taskWorkspace.view === "month" || taskWorkspace.view === "week")
    readonly property real sidebarWidth: taskWorkspace.preferences.sidebarCollapsed ? 44 : Math.min(288,208*Theme.scale)
    readonly property real detailWidth: Math.max(344,280*Theme.scale)
    readonly property bool detailDocked: editor.visible && !toolPageVisible && !calendarVisible && width >= sidebarWidth+440+detailWidth+80

    function markDirty() { if (!fillingDraft) draftDirty = true }
    function openEditor(taskId) {
        editorManualScroll=false
        if (editingActive && editId === taskId) {quickEditor.close();editor.open();editTitle.forceActiveFocus();return}
        navigateSafely(function(){loadEditor(taskId)})
    }
    function openQuickEditor(taskId) {
        if(editingActive && editId===taskId){if(quickEditor.editing)quickEditor.focusTitle();else editTitle.forceActiveFocus();return}
        navigateSafely(function(){
            loadEditor(taskId)
            if(editId!==taskId || !editor.visible)return
            editor.visible=false;inlineAttachWanted=true;inlineAttachAttempts=0;attachQuickRow()
        })
    }
    Timer {id: inlineAttachRetry;interval: 16;onTriggered: mainWindow.attachQuickRow()}
    function attachQuickRow() {
        if(!inlineAttachWanted)return
        const index=taskWorkspace.taskIndex(editId)
        if(index<0 || inlineAttachAttempts++>3){quickEditor.close();editor.open();return}
        taskList.currentIndex=index;taskList.positionViewAtIndex(index,ListView.Contain)
        const row=taskList.currentItem as TaskRow
        if(!row || row.taskId!==editId){inlineAttachRetry.restart();return}
        inlineRow=row;quickEditor.open()
    }
    Timer {
        interval: 16;repeat: true;running: !!mainWindow.draggedTaskRow
        onTriggered: {
            const row=mainWindow.draggedTaskRow as TaskRow;if(!row)return
            const point=row.dragPointIn(taskList),before=taskList.contentY
            const delta=point.x<0 || point.x>taskList.width ? 0 : point.y<40 ? -10 : point.y>taskList.height-40 ? 10 : 0
            if(delta){taskList.contentY=Math.max(taskList.originY,Math.min(taskList.contentY+delta,taskList.originY+taskList.contentHeight-taskList.height));row.compensateScroll(taskList.contentY-before)}
        }
    }

    Timer {
        interval: 16;repeat: true;running: !!mainWindow.draggedCatalogRow
        onTriggered: {const flick=navigationScroll.contentItem as Flickable;if(!flick || !mainWindow.draggedCatalogRow)return;const point=mainWindow.draggedCatalogRow.catalogDragPoint(flick);if(point.x<0 || point.x>flick.width)return;const delta=point.y<28 ? -8 : point.y>flick.height-28 ? 8 : 0;if(delta)flick.contentY=Math.max(flick.originY,Math.min(flick.originY+Math.max(0,flick.contentHeight-flick.height),flick.contentY+delta))}
    }
    Timer {id: quickFocusDelay;interval: 16;onTriggered: mainWindow.revealQuick(mainWindow.quickFocusTarget)}
    function revealQuick(item) {
        if(!quickEditor.editing || !item || quickManualScroll)return
        let owner=item;while(owner && owner!==quickEditor)owner=owner.parent
        if(!owner)return
        taskList.cancelFlick()
        // Keep the small expanded card together when it fits; otherwise reveal
        // the focused control in the one list viewport.
        const target=quickEditor.height+16<=taskList.height ? quickEditor : item
        const y=target.mapToItem(taskList.contentItem,0,0).y
        if(y<taskList.contentY)taskList.contentY=y
        else if(y+target.height>taskList.contentY+taskList.height)taskList.contentY=y+target.height-taskList.height+8
    }
    function completeRow(row) {
        const id=row.taskId
        const wasDone=row.isDone
        navigateSafely(function(){
            const task=taskWorkspace.task(id)
            if(!task.taskId)return
            const currentRow=taskList.itemAtIndex(taskWorkspace.taskIndex(id)) as TaskRow
            completionRow=currentRow
            if(currentRow && !wasDone)currentRow.prepareCompletion()
            editor.close();taskWorkspace.changeState(task.taskId,task.rowVersion,wasDone ? "reopen" : "complete")
        })
    }
    property var pendingSubtaskTitles: ({})
    function loadEditor(taskId) {
        const item=taskWorkspace.task(taskId)
        newSubtask.text=pendingSubtaskTitles[taskId] || "";editorRootId=taskId;editorRootTitle=item.title || "";editorReturnTaskId=item.parentTaskId && taskWorkspace.task(item.parentTaskId).taskId ? item.parentTaskId : "";editorTab=0;fillEditor(taskId)
    }
    function openSubtask(taskId) {navigateSafely(function(){loadEditor(taskId)})}
    function returnToParentTasks() {
        const parentId=editorReturnTaskId
        navigateSafely(function(){if(parentId && taskWorkspace.task(parentId).taskId){loadEditor(parentId);editorTab=1;subtasks.refresh()}else editor.close()})
    }
    function returnFromDetails() {if(editorReturnTaskId)returnToParentTasks();else navigateSafely(function(){editor.close()})}
    function selectEditorTab(index) {
        if(index===editorTab || (index===1 && editorIsSubtask && !editorHasChildren))return
        navigateSafely(function(){fillEditor(editorRootId);editorTab=index;if(index===1)subtasks.refresh()})
    }
    function fillEditor(taskId) {
        toolPage=""
        const item = taskWorkspace.task(taskId)
        if (!item.taskId || !taskWorkspace.ready || taskWorkspace.busy) return
        quickEditor.close()
        startDate.closePicker();deadline.closePicker();reminder.closePicker()
        fillingDraft = true
        editId = item.taskId
        editVersion = item.rowVersion
        editStatus = item.taskStatus
        editTitle.text = item.title
        editNotes.text = item.notes
        draftPlanningState=item.planningState
        startDate.text = item.startOn
        deadline.text = item.deadlineOn
        originalStartOn=item.storedStartOn===undefined ? item.startOn : item.storedStartOn
        originalEndOn=item.storedEndOn===undefined ? item.deadlineOn : item.storedEndOn
        timeDatesDiffer=!!item.timeDatesDiffer
        draftDatesDirty=false
        mainWindow.draftTodayFocus = item.todayFocus
        mainWindow.draftEvening=item.evening
        reminder.text=taskWorkspace.localReminder(item.remindAtUtc);reminder.repeatFrequency=item.reminderRepeat || "NONE";reminder.repeatInterval=item.reminderEvery || 1;draftReminderZone=item.reminderRepeat && item.reminderRepeat!=="NONE" ? item.reminderZone : taskWorkspace.systemTimeZone()
        extraFields.loadTask(item);timingFields.loadTask(item)
        fillingDraft = false
        draftDirty = false
        editorSaveError=false
        if(taskWorkspace.validationError)taskWorkspace.clearError()
        editor.open()
        const flick=editorScroll
        if(flick)flick.contentY=0
        editTitle.forceActiveFocus()
    }
    function navigateSafely(action) {
        if (taskWorkspace.busy) return
        if (organization.visible && organization.dirty) {organization.safely(action);return}
        if (quickEditor.editing && draftDirty) {
            pendingAction=action
            if(!saveEditor())pendingAction=null
        } else if (editingActive && draftDirty) {
            pendingAction = action
            leaveDialog.open()
        } else action()
    }
    function taskRowForVerification(id){const index=taskWorkspace.taskIndex(id);const row=index>=0 ? taskList.itemAtIndex(index) : null;return row && row.taskId===id ? row : null}
    function revealTaskForVerification(id){Qt.callLater(function(){const index=mainWindow.taskWorkspace.taskIndex(id);taskList.currentIndex=index;if(index>=0)taskList.positionViewAtIndex(index,ListView.Contain)})}
    function revealTimingForVerification(){editorManualScroll=true;editorFocusDelay.stop();editorScroll.cancelFlick();const y=timingFields.mapToItem(editorFields,0,0).y;editorScroll.contentY=Math.max(0,Math.min(editorScroll.contentHeight-editorScroll.height,y));return timingFields.width>0}
    function saveEditor() {
        if(editorSaving)return false
        const patch=Object.assign(extraFields.detailsPatch(),timingFields.detailsPatch())
        patch.evening=mainWindow.draftEvening;patch.remindAtLocal=reminder.text;patch.reminderRepeat=reminder.repeatFrequency;patch.reminderEvery=reminder.repeatInterval;patch.reminderZone=mainWindow.draftReminderZone
        editorSaveError=false
        editorSaving=true
        const accepted=taskWorkspace.saveTaskWithDetails(editId, editVersion, editTitle.text, editNotes.text,
                                     draftPlanningState,
                                     (draftDatesDirty || timingFields.timeDirty) ? startDate.text : originalStartOn, (draftDatesDirty || timingFields.timeDirty) ? deadline.text : originalEndOn, mainWindow.draftTodayFocus,patch)
        if(!accepted){editorSaving=false;if(taskWorkspace.errorText)editorSaveError=true}
        return accepted
    }
    function dateDraftChanged(value) {
        if(!fillingDraft){draftDatesDirty=true;if(value)draftPlanningState="ACTIVE";draftTodayFocus=false;draftEvening=false}
        markDirty()
    }
    function moveTaskToNavigation(id,version,view) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(!item.taskId)return
            editor.close()
            pendingNavigationMove={taskId:id,rowVersion:item.rowVersion,view:view}
            if(view==="upcoming")datePicker.showFor(taskWorkspace.dateOffset(1),"upcoming")
            else commitNavigationMove("")
        })
    }
    function commitNavigationMove(date) {
        const move=pendingNavigationMove
        if(move.taskId)taskWorkspace.moveTaskToView(move.taskId,move.rowVersion,move.view,date)
        pendingNavigationMove=({})
    }
    function verifyTaskDetailLayout(sideBySide) {
        const start=startDateGroup.mapToItem(editorFields,0,0)
        const end=deadlineGroup.mapToItem(editorFields,0,0)
        if(sideBySide) {
            if(Math.abs(start.y-end.y)>1 || start.x+startDateGroup.width>end.x-8)return "dates are not distinct aligned columns"

        } else if(start.y+startDateGroup.height>end.y-4)return "narrow dates overlap"
        for(const field of [startDate,deadline,reminder]) {
            if(!field.visible)continue
            const at=field.mapToItem(editorFields,0,0)
            if(at.x<0 || at.x+field.width>editorFields.width+1)return "field clipped horizontally"
        }
        if(!editNotes.text && Theme.scale===1 && editNotes.height>44)return "empty notes reserve too much space"
        return "PASS"
    }
    function ensureEditorFocus(item) {
        if(!editor.visible || !item || editorManualScroll)return
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
        editorManualScroll=false
        if(activeFocusItem)editorFocusTarget=activeFocusItem
        if(quickEditor.editing && activeFocusItem){quickManualScroll=false;quickFocusTarget=activeFocusItem;quickFocusDelay.restart()}
        editorFocusDelay.restart()
    }
    Timer {id: editorFocusDelay;interval: 16;onTriggered: mainWindow.ensureEditorFocus(mainWindow.editorFocusTarget)}
    // Nested layouts can move a focused child after their parent's height signal.
    // Observe the settled geometry after polish, without undoing manual scrolling.
    onAfterAnimating: {
        if(editor.visible)Qt.callLater(mainWindow.followEditorLayout)
        if(quickEditor.editing)Qt.callLater(mainWindow.followQuickLayout)
    }
    function followQuickLayout() {
        const item=quickFocusTarget
        if(!quickEditor.editing || !item)return
        let owner=item;while(owner && owner!==quickEditor)owner=owner.parent
        if(!owner)return
        revealQuick(item)
    }
    function followEditorLayout() {
        const item=activeFocusItem
        if(!editor.visible || !item || editorManualScroll)return
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
        if(!editor.visible || fillingDraft || editorManualScroll || !editNotes.activeFocus)return
        const flick=editorScroll
        if(!flick)return
        const position=editNotes.mapToItem(flick.contentItem,0,editNotes.cursorRectangle.y)
        const bottom=position.y+editNotes.cursorRectangle.height+8
        if(position.y<flick.contentY)flick.contentY=position.y
        else if(bottom>flick.contentY+flick.height)flick.contentY=bottom-flick.height
    }
    property string lastListView: "today"
    function changeView(view) {
        navigateSafely(function(){
            if(taskWorkspace.view!=="month" && taskWorkspace.view!=="week")lastListView=taskWorkspace.view
            viewPositions[taskWorkspace.view]=({y:taskList.contentY,index:taskList.currentIndex})
            expandCategoryParent(view.substring(view.indexOf(":")+1))
            editor.close();toolPage="";batchMode=false;batchSelection=[];taskWorkspace.setView(view)
            if(view==="inbox" || viewCodes.indexOf(view)>=0){const nav=navigationScroll.contentItem as Flickable;if(nav)nav.contentY=nav.originY}
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
            if(!item.schedule && item.startOn>taskWorkspace.dateOffset(0))confirmToday.open();else applyToday()
        })
    }
    function applyToday() {
        const item=taskWorkspace.task(todayTarget.taskId)
        if(item.taskId){editor.close();taskWorkspace.saveDetails(item.taskId,item.rowVersion,{planningState:"ACTIVE",todayFocus:true})}
    }
    function focusHeader(item) {
        const flick=headerScroll.contentItem as Flickable
        if(flick)flick.contentY=Math.max(0,Math.min(item.y-8,flick.contentHeight-flick.height))
        item.forceActiveFocus()
    }
    function newTask(view) {navigateSafely(function(){editor.close();toolPage="";if(view)changeView(view);else if(calendarVisible || ["logbook","completed","trash"].indexOf(taskWorkspace.view)>=0)changeView("inbox");Qt.callLater(function(){focusHeader(newTitle)})})}
    function canNavigateByKey() {return !editor.visible && !toolModalOpen && !(activeFocusItem instanceof TextInput) && !(activeFocusItem instanceof TextEdit)}
    function changeTaskState(id,action) {
        if(action==="delete"){requestDeleteTask(id);return}
        navigateSafely(function(){
            const item=taskWorkspace.task(id)
            if(item.taskId){editor.close();taskWorkspace.changeState(id,item.rowVersion,action)}
        })
    }
    property var deleteTaskTarget: ({})
    function requestDeleteTask(id) {
        navigateSafely(function(){
            const item=taskWorkspace.task(id);if(!item.taskId)return
            if(taskWorkspace.subtaskChoices(id).length>0){deleteTaskTarget=item;deleteParentConfirm.open()}
            else {editor.close();taskWorkspace.changeState(item.taskId,item.rowVersion,"delete")}
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
    function returnToContent() {
        navigateSafely(function(){toolPage="";Qt.callLater(function(){if(mainWindow.calendarVisible)calendarPanel.forceActiveFocus();else taskList.forceActiveFocus()})})
    }
    function activateToolPage(name) {
        editor.close();batchMode=false;batchSelection=[];toolPage=name
        if(name==="organization")organization.begin()
        Qt.callLater(function(){workspacePageHost.forceActiveFocus()})
    }
    function openTool(name) {
        if(toolPage===name)return
        navigateSafely(function(){
            if(name==="recurrence" || name==="schedules"){toolPage="";taskWorkspace.setView("repeating")}
            else if(["organization","notifications"].indexOf(name)>=0)activateToolPage(name)
            else if(name === "settings"){editor.close();windowSettings.open()}
            else if(name === "data"){editor.close();dataTools.open()}
        })
    }
    function createLibrary(kind) {
        navigateSafely(function(){editor.close();catalogCreate.showForLibrary(kind)})
    }
    function createChildCategory(parentId){navigateSafely(function(){editor.close();catalogCreate.showChild(parentId)})}
    function expandCategoryParent(id){const item=taskWorkspace.categoryTree.find(value=>value.id===id);if(item && item.parentId)collapsedCategoryIds=collapsedCategoryIds.filter(value=>value!==item.parentId)}
    function showCatalogOptions(item,anchor,x,y){const point=anchor.mapToItem(mainWindow.contentItem,x,y);catalogContextItem=item;catalogContextAnchor=anchor;categoryContextMenu.x=point.x;categoryContextMenu.y=point.y;categoryContextMenu.open()}
    function catalogAction(action){const selected=catalogContextItem;navigateSafely(function(){const item=taskWorkspace.categoryTree.concat(taskWorkspace.catalogOf("tag")).find(value=>value.id===selected.id);if(!item)return;if(action==="archive")taskWorkspace.saveCatalog(Object.assign({},item,{archived:!item.archived,planningState:"ACTIVE",status:"OPEN"}));else if(action==="delete"){catalogContextItem=item;catalogDeleteConfirm.open()}else taskWorkspace.moveCatalog(item.kind,item.id,item.rowVersion,action==="up" ? -1 : 1)})}
    function manageCatalogItem(item,move){navigateSafely(function(){editor.close();const current=taskWorkspace.categoryTree.concat(taskWorkspace.catalogOf("tag")).find(value=>value.id===item.id);if(current){if(move){organization.choose(current);organization.showMove()}else catalogEdit.showFor(current,mainWindow.catalogContextAnchor || contentViewSwitch)}})}
    function manageCatalog(kind) {
        navigateSafely(function(){activateToolPage("organization");organization.showKind(kind)})
    }
    function editCurrentCatalog() {
        const item=taskWorkspace.catalog.find(entry=>taskWorkspace.view===entry.kind+":"+entry.id)
        if(item)manageCatalogItem(item,false)
    }
    function currentViewTitle() {
        if(toolPage==="organization")return organization.archivedOnly ? qsTr("已归档分类") : qsTr("整理")
        if(taskWorkspace.view==="repeating")return qsTr("循环任务")
        if(toolPage==="notifications")return qsTr("提醒记录")
        const index=viewCodes.indexOf(taskWorkspace.view)
        if(index>=0)return viewNames[index]
        if(taskWorkspace.view==="inbox")return qsTr("收集箱")
        if(taskWorkspace.view==="trash")return qsTr("最近删除")
        if(taskWorkspace.view==="upcoming")return qsTr("计划")
        if(taskWorkspace.view==="anytime")return qsTr("全部")
        if(taskWorkspace.view==="logbook")return qsTr("完成")
        if(calendarVisible)return qsTr("日历")
        for(const item of taskWorkspace.catalog)if(taskWorkspace.view.endsWith(":"+item.id))return item.kind==="tag" ? qsTr("标签 · %1").arg(item.title) : item.title
        return qsTr("任务")
    }
    function verifyWorkspacePage() {
        const page=toolPage==="organization" ? organization : notifications
        if(!toolPageVisible || !page.visible || !workspacePageHost.visible)return "page is not visible"
        if(toolModalOpen || taskList.visible || headerScroll.visible || calendarPanel.visible)return "navigation leaked a modal or task surface"
        let owner=page.parent;while(owner && owner!==workspacePageHost)owner=owner.parent
        if(!owner)return "page is outside the content host"
        const position=page.mapToItem(mainWindow.contentItem,0,0)
        if(page.width<1 || page.height<96 || position.x<sidebarWidth-1 || position.x+page.width>width+1 || position.y+page.height>height+1)return "page clipped: "+JSON.stringify({page: toolPage,x: position.x,y: position.y,width: page.width,height: page.height,windowWidth: width,windowHeight: height,sidebarWidth: sidebarWidth})
        const footer=page.footer
        if(footer && footer.visible){const bottom=footer.mapToItem(mainWindow.contentItem,0,footer.height);if(bottom.y>height+1 || footer.width>page.width+1)return "page actions clipped"}
        return "PASS"
    }
    function showVerificationPage(page,taskId) {
        toolPage="";organization.dirty=false;editor.close();windowSettings.close();dataTools.close();datePicker.close();styleGallery.close()
        if(page === "details")openEditor(taskId)
        else if(page === "organization")activateToolPage("organization")
        else if(page === "recurrence")taskWorkspace.setView("repeating")
        else if(page === "settings")windowSettings.open()
        else if(page === "data")dataTools.open()
        else if(page === "date")datePicker.showFor(taskWorkspace.dateOffset(0),"start")
        else if(page === "editor")openEditor(taskId)
        else if(page === "batch"){batchDialog.selection=[taskWorkspace.task(taskId)];batchDialog.open()}
        else if(page === "style")styleGallery.open()
        else if(page === "event")calendarPanel.openEvent(taskId)
        else taskWorkspace.setView(page)
    }

    Shortcut {sequence: "Escape";enabled: (mainWindow.toolPage==="notifications" || mainWindow.toolPage==="schedules") && !mainWindow.toolModalOpen;onActivated: mainWindow.returnToContent()}
    Shortcut { sequences: [StandardKey.New];enabled: !mainWindow.toolModalOpen;onActivated: mainWindow.newTask("") }
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+1" : "Ctrl+1";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("today")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+2" : "Ctrl+2";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("scheduled")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+3" : "Ctrl+3";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("all")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+4" : "Ctrl+4";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("important")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+5" : "Ctrl+5";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("completed")}
    Shortcut {sequences: [StandardKey.Preferences];enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.openTool("settings")}
    Shortcut {sequence: Qt.platform.os==="osx" ? "Meta+6" : "Ctrl+6";enabled: mainWindow.canNavigateByKey();onActivated: mainWindow.changeView("month")}
    Shortcut { sequences: [StandardKey.Find];enabled: !mainWindow.toolModalOpen && mainWindow.taskContentVisible;onActivated: { if (!editor.visible || mainWindow.detailDocked) {mainWindow.searchExpanded=true;Qt.callLater(function(){mainWindow.focusHeader(search)})} } }
    Shortcut { sequence: "Escape";enabled: editor.visible && !timingFields.modalOpen && !taskHistoryDialog.visible && !editorMenu.visible && !extraFields.parentPickerVisible && !catalogCreate.visible && !leaveDialog.visible && !datePicker.visible && !startDate.pickerVisible && !deadline.pickerVisible && !reminder.pickerVisible;onActivated: mainWindow.returnFromDetails() }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            Layout.preferredWidth: mainWindow.sidebarWidth
            Layout.maximumWidth: mainWindow.sidebarWidth
            Layout.fillHeight: true
            color: Theme.sidebar
            ColumnLayout {
                anchors.fill: parent;spacing: 0
                RowLayout {
                    Layout.fillWidth: true;Layout.margins: 6;spacing: 0
                    Label {text: "Daymark";visible: !mainWindow.taskWorkspace.preferences.sidebarCollapsed;Layout.fillWidth: true;Layout.leftMargin: 10;font.pixelSize: Theme.bodySize;font.weight: Font.DemiBold;elide: Text.ElideRight}
                    IconToolButton {objectName: "toggleSidebar";Accessible.name: mainWindow.taskWorkspace.preferences.sidebarCollapsed ? qsTr("显示导航") : qsTr("收起导航");contentItem: AppIcon {name: "sidebar"} onClicked: mainWindow.taskWorkspace.setPreference("sidebarCollapsed",!mainWindow.taskWorkspace.preferences.sidebarCollapsed)}
                }
            Item {visible: mainWindow.taskWorkspace.preferences.sidebarCollapsed;Layout.fillWidth: true;Layout.fillHeight: true}
            ScrollView {
                id: navigationScroll
                visible: !mainWindow.taskWorkspace.preferences.sidebarCollapsed
                objectName: "navigationScroll"
                Layout.fillWidth: true;Layout.fillHeight: true
                padding: 10
                clip: true
            ColumnLayout {
                width: navigationScroll.availableWidth
                height: implicitHeight
                spacing: 2
                NavSection {
                    objectName: "sidebarTasks";title: qsTr("任务")
                    expanded: mainWindow.taskWorkspace.preferences.sidebarTasksExpanded
                    containsSelection: !mainWindow.toolPageVisible && (mainWindow.taskWorkspace.view==="inbox" || mainWindow.viewCodes.indexOf(mainWindow.taskWorkspace.view)>=0)
                    onExpansionRequested: function(value){mainWindow.taskWorkspace.setPreference("sidebarTasksExpanded",value)}
                GridLayout {
                    Layout.fillWidth: true;columns: 2;columnSpacing: 6;rowSpacing: 6
                    Repeater {
                        model: [qsTr("收集箱")].concat(mainWindow.viewNames)
                        SmartViewTile {
                            required property int index
                            required property string modelData
                            readonly property string viewCode: index===0 ? "inbox" : mainWindow.viewCodes[index-1]
                            objectName: "navigation-"+viewCode
                            Layout.fillWidth: true;Layout.preferredWidth: 1
                            text: modelData;selected: !mainWindow.toolPageVisible && mainWindow.taskWorkspace.view===viewCode
                            symbol: ["inbox","today","calendar","list","flag","logbook"][index]
                            tint: viewCode==="scheduled" ? Theme.danger : viewCode==="all" || viewCode==="completed" ? Theme.secondary : viewCode==="important" ? Theme.catalogColor("orange") : Theme.accent
                            count: mainWindow.taskWorkspace.viewCounts[viewCode] || 0
                            acceptsTaskDrop: viewCode!=="all" && mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy
                            onTaskDropped: function(taskId,version){mainWindow.moveTaskToNavigation(taskId,version,viewCode==="scheduled" ? "upcoming" : viewCode==="completed" ? "logbook" : viewCode)}
                            onClicked: mainWindow.changeView(viewCode)
                        }
                    }
                }
                }
                NavSection {
                    id: categorySection
                    objectName: "sidebarLibrary";title: qsTr("分类")
                    onOptionsRequested: {const at=categorySection.mapToItem(mainWindow.contentItem,8,28);categorySectionMenu.x=at.x;categorySectionMenu.y=at.y;categorySectionMenu.open()}
                    Menu {id: categorySectionMenu;parent: Overlay.overlay;MenuItem {text: qsTr("新建分类…");onTriggered: mainWindow.createLibrary("project")} MenuItem {objectName: "showArchivedCategories";text: qsTr("已归档分类…");onTriggered: {mainWindow.manageCatalog("project");organization.archivedOnly=true}}}
                    expanded: mainWindow.taskWorkspace.preferences.sidebarLibraryExpanded
                    containsSelection: (mainWindow.toolPage==="organization" && organization.filterKind!=="tag") || (!mainWindow.toolPageVisible && (mainWindow.taskWorkspace.view.startsWith("area:") || mainWindow.taskWorkspace.view.startsWith("project:")))
                    showActions: true
                    actionsSelected: mainWindow.toolPage==="organization" && organization.filterKind!=="tag"
                    createDescription: qsTr("新建一级分类");manageDescription: qsTr("管理分类")
                    onCreateRequested: mainWindow.createLibrary("project")
                    onManageRequested: mainWindow.manageCatalog("project")
                    Menu {
                        id: libraryCreateMenu
                        MenuItem {objectName: "createSidebarProject";text: qsTr("新建分类…");onTriggered: mainWindow.createLibrary("project")}
                    }
                    Menu {id: libraryMenu;MenuItem {objectName: "manageLibrary";text: qsTr("管理分类");onTriggered: mainWindow.manageCatalog("project")}}
                    onExpansionRequested: function(value){mainWindow.taskWorkspace.setPreference("sidebarLibraryExpanded",value)}
                ColumnLayout {
                    Layout.fillWidth: true
                        Repeater {
                            model: mainWindow.taskWorkspace.navigationCatalog
                            NavRow {
                                id: categoryNavRow
                                required property var modelData
                                objectName: "categoryNav-"+modelData.id
                                visible: !modelData.parentId || mainWindow.collapsedCategoryIds.indexOf(modelData.parentId)<0
                                leftPadding: 4+modelData.depth*16
                                canDragCatalog: modelData.kind==="project" && mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy
                                catalogId: modelData.id;catalogVersion: modelData.rowVersion;catalogParentId: modelData.parentId
                                catalogDragLayer: mainWindow.contentItem
                                onCatalogDragChanged: function(active){mainWindow.catalogDragActive=active;mainWindow.draggedCatalogRow=active ? categoryNavRow : null}
                                onCatalogDropped: function(sourceId,sourceVersion,placement){const targetId=modelData.id,targetVersion=modelData.rowVersion;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.dropCategory(sourceId,sourceVersion,targetId,targetVersion,placement)})}
                                showBranchSlot: true
                                hasChildren: modelData.childCount>0
                                branchExpanded: mainWindow.collapsedCategoryIds.indexOf(modelData.id)<0
                                ancestorSelected: !mainWindow.toolPageVisible && mainWindow.taskWorkspace.categoryTree.some(item=>item.parentId===modelData.id && mainWindow.taskWorkspace.view===item.kind+":"+item.id)
                                detailsText: modelData.path
                                suppressCatalogTooltips: mainWindow.catalogDragActive
                                showCatalogActions: true
                                onExpansionRequested: function(value){mainWindow.collapsedCategoryIds=value ? mainWindow.collapsedCategoryIds.filter(id=>id!==modelData.id) : mainWindow.collapsedCategoryIds.concat([modelData.id])}
                                onOptionsRequested: function(x,y){mainWindow.showCatalogOptions(modelData,categoryNavRow,x,y)}
                                Layout.fillWidth: true
                                text: modelData.title
                                iconBadge: false
                                symbol: "folder"
                                symbolColor: Theme.catalogColor(modelData.colorKey)
                                selected: !mainWindow.toolPageVisible && mainWindow.taskWorkspace.view === modelData.kind+":"+modelData.id
                                acceptsTaskDrop: modelData.kind === "project" || modelData.kind === "area"
                                onTaskDropped: function(taskId,version) {mainWindow.assignTask(taskId,modelData.kind,modelData.id)}
                                onClicked: mainWindow.changeView(modelData.kind+":"+modelData.id)
                            }
                        }
                }
                }
                NavSection {
                    id: tagSection
                    objectName: "sidebarTags";title: qsTr("标签")
                    onOptionsRequested: {const at=tagSection.mapToItem(mainWindow.contentItem,8,28);tagSectionMenu.x=at.x;tagSectionMenu.y=at.y;tagSectionMenu.open()}
                    Menu {id: tagSectionMenu;parent: Overlay.overlay;MenuItem {text: qsTr("新建标签…");onTriggered: mainWindow.createLibrary("tag")}}
                    expanded: mainWindow.taskWorkspace.preferences.sidebarTagsExpanded
                    containsSelection: (mainWindow.toolPage==="organization" && organization.filterKind==="tag") || (!mainWindow.toolPageVisible && mainWindow.taskWorkspace.view.startsWith("tag:"))
                    onExpansionRequested: function(value){mainWindow.taskWorkspace.setPreference("sidebarTagsExpanded",value)}
                    showActions: true;createDescription: qsTr("新建标签");manageDescription: qsTr("管理标签")
                    actionsSelected: mainWindow.toolPage==="organization" && organization.filterKind==="tag"
                    onCreateRequested: mainWindow.createLibrary("tag")
                    onManageRequested: mainWindow.manageCatalog("tag")
                    Repeater {
                        model: mainWindow.taskWorkspace.catalog.filter(item=>item.kind==="tag")
                        NavRow {id: tagNavRow;required property var modelData;objectName: "tagNav-"+modelData.id;iconBadge: false;showBranchSlot: true;showCatalogActions: true;onOptionsRequested: function(x,y){mainWindow.showCatalogOptions(modelData,tagNavRow,x,y)} Layout.fillWidth: true;text: modelData.title;symbol: "tag";symbolColor: Theme.catalogColor(modelData.colorKey);selected: !mainWindow.toolPageVisible && mainWindow.taskWorkspace.view==="tag:"+modelData.id;onClicked: mainWindow.changeView("tag:"+modelData.id)}
                    }
                }
            }
            }
            ColumnLayout {
                objectName: "sidebarFooter"
                visible: !mainWindow.taskWorkspace.preferences.sidebarCollapsed
                Layout.fillWidth: true;Layout.margins: 10;spacing: 2
                Rectangle {Layout.fillWidth: true;implicitHeight: 1;color: Theme.separator;Layout.bottomMargin: 6}
                NavRow {objectName: "navigation-trash";iconBadge: false;Layout.fillWidth: true;text: qsTr("最近删除");symbol: "trash";count: mainWindow.taskWorkspace.viewCounts.trash || 0;selected: !mainWindow.toolPageVisible && mainWindow.taskWorkspace.view==="trash";onClicked: mainWindow.changeView("trash")}
                RowLayout {
                    Layout.fillWidth: true;spacing: 4
                    NavRow {objectName: "settingsAndTools";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("设置");symbol: "settings";onClicked: mainWindow.openTool("settings")}
                    NavRow {objectName: "dataAndBackup";Layout.fillWidth: true;Layout.preferredWidth: 1;text: qsTr("备份");Accessible.name: qsTr("数据与备份");symbol: "backup";onClicked: mainWindow.openTool("data")}
                }
            }
            }
        }
        ColumnLayout {
            objectName: "taskContent"
            visible: !editor.visible || mainWindow.detailDocked
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.pagePadding
            Layout.rightMargin: mainWindow.detailDocked ? mainWindow.detailWidth+Theme.pagePadding+16 : Theme.pagePadding
            spacing: Theme.fieldGap
            RowLayout {
                id: pageToolbar
                readonly property real reservedHeight: Math.max(52,Theme.titleSize*1.3+Theme.captionSize*1.6+2)
                Layout.minimumHeight: reservedHeight;Layout.maximumHeight: reservedHeight
                WidgetAction {objectName: "returnFromWorkspacePage";visible: mainWindow.toolPageVisible;symbol: "left";description: qsTr("返回之前的内容");onClicked: mainWindow.returnToContent()}
                RowLayout {
                    id: contentViewSwitch;objectName: "contentViewSwitch";visible: !mainWindow.toolPageVisible;spacing: 2
                    WidgetAction {objectName: "showListView";symbol: "list";description: qsTr("列表视图");checked: !mainWindow.calendarVisible;onClicked: if(mainWindow.calendarVisible)mainWindow.changeView(mainWindow.lastListView)}
                    WidgetAction {objectName: "showCalendarView";symbol: "calendar";description: qsTr("日历视图");checked: mainWindow.calendarVisible;onClicked: if(!mainWindow.calendarVisible)mainWindow.changeView("month")}

                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                    text: mainWindow.taskContentVisible && search.text.trim().length > 0 ? qsTr("搜索结果") : mainWindow.currentViewTitle()
                    font.pixelSize: Theme.titleSize
                    color: !mainWindow.taskContentVisible ? Theme.text : mainWindow.taskWorkspace.view==="today" ? Theme.accent : mainWindow.taskWorkspace.view==="scheduled" ? Theme.danger : mainWindow.taskWorkspace.view==="important" ? Theme.catalogColor("orange") : mainWindow.taskWorkspace.view==="completed" ? Theme.secondary : Theme.text
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length>0
                        font.pixelSize: Theme.captionSize;color: Theme.secondary;elide: Text.ElideRight
                        text: !mainWindow.taskContentVisible ? "" : search.text.trim().length>0 ? (mainWindow.taskWorkspace.view==="trash" ? qsTr("搜索最近删除 · %1 项结果") : mainWindow.taskWorkspace.view==="completed" ? qsTr("搜索已完成 · %1 项结果") : mainWindow.taskWorkspace.view==="logbook" ? qsTr("搜索已完成与已取消 · %1 项结果") : qsTr("全局任务搜索 · %1 项结果")).arg(mainWindow.taskWorkspace.tasks.count) : mainWindow.taskWorkspace.view==="today" ? mainWindow.taskWorkspace.localToday+qsTr(" · 待办 %1 · 完成 %2").arg(mainWindow.taskWorkspace.todayTasks.count).arg(mainWindow.taskWorkspace.completedToday) : ""
                    }
                }
                WidgetAction {objectName: "openReminderHistory";symbol: "bell";description: qsTr("提醒记录");checked: mainWindow.toolPage==="notifications";onClicked: mainWindow.openTool("notifications")}
                BusyIndicator { running: mainWindow.taskWorkspace.busy && !mainWindow.taskWorkspace.backgroundBusy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
                WidgetAction {visible: mainWindow.taskContentVisible;symbol: "search";description: mainWindow.searchExpanded ? qsTr("关闭搜索") : qsTr("搜索任务");checked: mainWindow.searchExpanded || search.text.length>0;onClicked: {if(mainWindow.searchExpanded || search.text.length>0){search.clear();mainWindow.searchExpanded=false;taskList.forceActiveFocus()}else{mainWindow.searchExpanded=true;search.forceActiveFocus()}}}
                WidgetAction {objectName: "toggleListSubtasks";visible: mainWindow.taskContentVisible && !mainWindow.taskWorkspace.smartView;symbol: "subtasks";description: mainWindow.taskWorkspace.preferences.listShowSubtasks ? qsTr("隐藏子任务") : qsTr("显示子任务");checked: mainWindow.taskWorkspace.preferences.listShowSubtasks;onClicked: mainWindow.navigateSafely(function(){quickEditor.close();mainWindow.taskWorkspace.setPreference("listShowSubtasks",!mainWindow.taskWorkspace.preferences.listShowSubtasks)})}
                WidgetAction {visible: mainWindow.taskContentVisible;symbol: "filter";description: (mainWindow.selectedTag || mainWindow.selectedProject) ? qsTr("清除筛选") : mainWindow.filtersExpanded ? qsTr("收起筛选") : qsTr("筛选任务");checked: mainWindow.filtersExpanded || !!mainWindow.selectedTag || !!mainWindow.selectedProject;onClicked: {if(mainWindow.selectedTag || mainWindow.selectedProject){tagFilter.currentIndex=0;projectFilter.currentIndex=0;mainWindow.selectedTag="";mainWindow.selectedProject="";mainWindow.taskWorkspace.setTagFilter("");mainWindow.taskWorkspace.setProjectFilter("");mainWindow.filtersExpanded=false}else mainWindow.filtersExpanded=!mainWindow.filtersExpanded}}
                IconToolButton {
                    Accessible.name: qsTr("页面操作")
                    contentItem: AppIcon {name: "more"}
                    onClicked: pageMenu.open()
                    Menu {
                        id: pageMenu
                        x: parent.width-width
                        y: parent.height
                        MenuItem {text: mainWindow.batchMode ? qsTr("退出多选") : qsTr("选择多个任务…");enabled: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view!=="trash";onTriggered: mainWindow.navigateSafely(function(){editor.close();mainWindow.batchMode=!mainWindow.batchMode;mainWindow.batchSelection=[]})}
                        MenuItem {text: qsTr("已取消与完成记录");visible: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view==="completed";onTriggered: mainWindow.changeView("logbook")}
                        MenuItem {objectName: "showRepeatingTasks";text: qsTr("循环任务");visible: mainWindow.taskContentVisible;onTriggered: mainWindow.openTool("recurrence")}
                        MenuItem {objectName: "editCurrentCatalog";text: qsTr("编辑当前分类…");visible: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view.indexOf(":")>=0;onTriggered: mainWindow.editCurrentCatalog()}
                        MenuItem {text: qsTr("重新加载");enabled: !mainWindow.taskWorkspace.busy;onTriggered: mainWindow.navigateSafely(function(){editor.close();mainWindow.taskWorkspace.reload()})}
                    }
                }
            }
            ScrollView {
                id: headerScroll
                visible: mainWindow.taskContentVisible
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
                visible: mainWindow.taskContentVisible && (mainWindow.searchExpanded || text.length>0)
                Layout.fillWidth: true
                placeholderText: qsTr("搜索任务与备注…")
                Accessible.name: qsTr("搜索任务和备注")
                onTextChanged: mainWindow.taskWorkspace.setSearch(text)
                Keys.onEscapePressed: {clear();mainWindow.searchExpanded=false;taskList.forceActiveFocus()}
                selectByMouse: true
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: mainWindow.taskContentVisible && (mainWindow.filtersExpanded || mainWindow.selectedTag.length>0 || mainWindow.selectedProject.length>0 || search.text.length>0 || mainWindow.taskWorkspace.view.startsWith("project:"))
                CheckBox { visible: search.text.trim().length>0 || mainWindow.taskWorkspace.view.startsWith("project:"); text: qsTr("包括已完成/取消"); checked: mainWindow.taskWorkspace.includeCompleted; onClicked: mainWindow.taskWorkspace.includeCompleted=checked }
                RowLayout {
                Layout.fillWidth: true
                visible: mainWindow.filtersExpanded || !!mainWindow.selectedTag || !!mainWindow.selectedProject
                ComboBox {id: tagFilter;property var tags: mainWindow.taskWorkspace.catalogOf("tag"); Layout.fillWidth: true; model: [qsTr("全部标签")].concat(tags.map(function(t){return t.title})); onActivated: {mainWindow.selectedTag=currentIndex===0 ? "" : tags[currentIndex-1].id;mainWindow.taskWorkspace.setTagFilter(mainWindow.selectedTag)} }
                ComboBox {id: projectFilter;property var projects: mainWindow.taskWorkspace.catalogOf("project");Layout.fillWidth: true;model: [qsTr("全部分类")].concat(projects.map(item=>item.title));onActivated: {mainWindow.selectedProject=currentIndex===0 ? "" : projects[currentIndex-1].id;mainWindow.taskWorkspace.setProjectFilter(mainWindow.selectedProject)}}
                WidgetAction {symbol: "filterClear";description: qsTr("清除标签和分类筛选");enabled: !!mainWindow.selectedTag || !!mainWindow.selectedProject;onClicked: {tagFilter.currentIndex=0;projectFilter.currentIndex=0;mainWindow.selectedTag="";mainWindow.selectedProject="";mainWindow.taskWorkspace.setTagFilter("");mainWindow.taskWorkspace.setProjectFilter("")}}
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: !mainWindow.editingActive && (mainWindow.taskWorkspace.errorText.length > 0 || mainWindow.taskWorkspace.infoText.length > 0)
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: mainWindow.taskWorkspace.errorText || mainWindow.taskWorkspace.infoText
                    color: mainWindow.taskWorkspace.errorText.length > 0 ? Theme.danger : Theme.success
                }
                WidgetAction {symbol: "close";description: qsTr("关闭提示"); onClicked: mainWindow.taskWorkspace.clearError() }
            }
            RowLayout {
                Layout.fillWidth: true
                enabled: mainWindow.taskWorkspace.ready && (!mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy) && mainWindow.taskWorkspace.view !== "logbook"
                visible: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view!=="completed" && mainWindow.taskWorkspace.view!=="trash"
                TextField {
                    id: newTitle
                    objectName: "newTaskTitle"
                    Layout.fillWidth: true
                    placeholderText: mainWindow.taskWorkspace.view === "upcoming" || mainWindow.taskWorkspace.view === "scheduled" ? qsTr("新增任务（默认安排明天）") : qsTr("输入任务标题，Enter 添加")
                    selectByMouse: true
                    onAccepted: mainWindow.taskWorkspace.createTask(text)
                    Keys.onEscapePressed: taskList.forceActiveFocus()
                }
                WidgetAction {objectName: "quickAddTask";symbol: "plus";description: qsTr("添加任务");enabled: !mainWindow.taskWorkspace.busy && newTitle.text.trim().length>0;onClicked: mainWindow.taskWorkspace.createTask(newTitle.text) }
            }
            Label {
                visible: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view==="someday" && mainWindow.taskWorkspace.tasks.count>0
                text: qsTr("日期已暂停；移到随时可做后恢复。")
                color: Theme.secondary
                font.pixelSize: Theme.captionSize
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                visible: mainWindow.taskContentVisible && (mainWindow.batchMode || mainWindow.taskWorkspace.view==="logbook")
                Button {visible: mainWindow.batchMode;text: qsTr("退出多选");onClicked: {mainWindow.batchMode=false;mainWindow.batchSelection=[]}}
                ActionButton {symbol: "batch";visible: mainWindow.batchMode;text: qsTr("批量修改（%1）").arg(mainWindow.batchSelection.length);enabled: mainWindow.batchSelection.length>0;onClicked: {batchDialog.selection=mainWindow.batchSelection;batchDialog.open()}}
                ComboBox {visible: mainWindow.taskWorkspace.view==="logbook";model: [qsTr("全部历史"),qsTr("近7天"),qsTr("近30天")];onActivated: mainWindow.taskWorkspace.historyDays=[0,7,30][currentIndex]}
            }
                }
            }
            Item {
                id: workspacePageHost
                objectName: "workspacePageHost"
                visible: mainWindow.toolPageVisible
                Layout.fillWidth: true;Layout.fillHeight: true
                ColumnLayout {
                    anchors.fill: parent;spacing: Theme.fieldGap
                    OrganizationPage {id: organization;navigationMenuOpen: mainWindow.transientMenuOpen;Layout.fillWidth: true;Layout.fillHeight: true;visible: mainWindow.toolPage==="organization";workspace: mainWindow.taskWorkspace;onCreateRequested: function(kind,parentId){catalogCreate.showForManagement(kind,parentId)};onOptionsRequested: function(item,anchor,x,y){mainWindow.showCatalogOptions(item,anchor,x,y)};onCategoryMoved: function(id){mainWindow.expandCategoryParent(id)};onBackRequested: mainWindow.returnToContent()}
                    ReminderPage {id: notifications;Layout.fillWidth: true;Layout.fillHeight: true;visible: mainWindow.toolPage==="notifications";workspace: mainWindow.taskWorkspace;onBackRequested: mainWindow.returnToContent();onOpenTask: function(id){mainWindow.openEditor(id)};onOpenEvent: function(id){calendarPanel.inspectEvent(id,"")}}
                }
            }
            CalendarPanel {
                id: calendarPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: mainWindow.calendarVisible
                workspace: mainWindow.taskWorkspace
                onDesktopRequested: mainWindow.taskWorkspace.setPreference("desktopCalendarVisible",true)
                onOpenTask: function(taskId) {mainWindow.openEditor(taskId)}
                onCompleteTask: function(id,version,action) {if(action==="delete")mainWindow.requestDeleteTask(id);else mainWindow.navigateSafely(function(){editor.close();mainWindow.taskWorkspace.changeState(id,version,action)})}
            }
            ListView {
                // Avoid background row incubation across model resets on Qt 6.10.2.
                cacheBuffer: 0
                DS.ScrollPhysics {target: taskList;keyboardNavigationEnabled: !quickEditor.editing && !mainWindow.draggedTaskRow}
                id: taskList
                objectName: "taskList"
                visible: mainWindow.taskContentVisible
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Layout.minimumHeight: visible ? 96 : 0
                spacing: 2
                focus: true
                highlightFollowsCurrentItem: !quickEditor.editing && !mainWindow.draggedTaskRow
                onContentHeightChanged: if(quickEditor.editing && !mainWindow.quickManualScroll)quickFocusDelay.restart()
                onHeightChanged: if(quickEditor.editing && !mainWindow.quickManualScroll)quickFocusDelay.restart()
                onMovementStarted: mainWindow.quickManualScroll=true
                WheelHandler {
                    target: null
                    blocking: false
                    acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                    onWheel: function(event){mainWindow.quickManualScroll=true;event.accepted=false}
                }
                TapHandler {
                    onTapped: function(point){
                        if(!quickEditor.editing || quickEditor.pickerVisible || leaveDialog.visible || mainWindow.draggedTaskRow)return
                        if(!taskList.itemAt(point.position.x+taskList.contentX,point.position.y+taskList.contentY))
                            mainWindow.navigateSafely(function(){quickEditor.close();taskList.currentIndex=-1;taskList.forceActiveFocus()})
                    }
                }
                keyNavigationWraps: true
                section.property: "viewSection"
                section.delegate: Label { required property string section; text: section; color: Theme.secondary; font.pixelSize: Theme.captionSize; height: section.length>0 ? implicitHeight+12 : 0;visible: section.length>0;verticalAlignment: Text.AlignVCenter }
                Keys.onReturnPressed: {const item=taskList.currentItem as TaskRow;if(item){if(mainWindow.batchMode)item.selectionRequested();else item.editRequested()}}
                Keys.onPressed: function(event){if((event.modifiers & Qt.ControlModifier) && (event.key===Qt.Key_Up || event.key===Qt.Key_Down)){const row=taskList.currentItem as TaskRow;if(row)row.moveRequested(event.key===Qt.Key_Up ? -1 : 1);event.accepted=true}}
                Keys.onSpacePressed: {const item=taskList.currentItem as TaskRow;if(item){if(mainWindow.batchMode)item.selectionRequested();else item.completeRequested()}}
                model: mainWindow.taskWorkspace.tasks
                ScrollBar.vertical: ScrollBar {}
                delegate: TaskRow {
                    id: taskRow
                    required property int index
                    width: taskList.width
                    showSeparator: index<taskList.count-1
                    smartView: mainWindow.taskWorkspace.smartView
                    deleted: mainWindow.taskWorkspace.view==="trash"
                    completionText: mainWindow.taskWorkspace.view==="completed" ? qsTr("完成于 %1").arg(mainWindow.taskWorkspace.localReminder(mainWindow.taskWorkspace.task(taskId).completedAtUtc || "")) : ""
                    onSubtaskRequested: {const id=taskId;mainWindow.navigateSafely(function(){mainWindow.loadEditor(id);mainWindow.selectEditorTab(1)})}
                    onRestoreRequested: mainWindow.taskWorkspace.changeState(taskId,rowVersion,"restore")
                    enabled: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy
                    onHeightChanged: if(expanded)quickFocusDelay.restart()
                    childrenExpanded: mainWindow.collapsedTaskIds.indexOf(taskId)<0 || search.text.length>0
                    onChildrenExpansionRequested: function(value){const id=taskId;mainWindow.navigateSafely(function(){quickEditor.close();mainWindow.collapsedTaskIds=value ? mainWindow.collapsedTaskIds.filter(item=>item!==id) : mainWindow.collapsedTaskIds.concat([id]);mainWindow.taskWorkspace.setTaskChildrenExpanded(id,value)})}
                    expanded: quickEditor.editing && mainWindow.editId===taskId
                    inlineEditor: expanded ? quickEditor : null
                    reduceMotion: mainWindow.taskWorkspace.preferences.reduceMotion
                    onEditorDetached: {if(!mainWindow.shuttingDown && mainWindow.inlineRow===taskRow){const reopen=quickEditor.editing;quickEditor.close();mainWindow.inlineRow=null;if(reopen){mainWindow.inlineAttachWanted=true;mainWindow.inlineAttachAttempts=0;Qt.callLater(mainWindow.attachQuickRow)}}}
                    highlighted: taskList.currentIndex === index
                    showTodayMarker: mainWindow.taskWorkspace.view!=="today"
                    selectionMode: mainWindow.batchMode
                    canReorder: mainWindow.taskWorkspace.taskReorderingAllowed && !isDone
                    dragThreshold: mainWindow.taskWorkspace.dragDistance
                    canDrag: mainWindow.taskWorkspace.ready && !mainWindow.taskWorkspace.busy && !isDone && !deleted
                    dragLayer: mainWindow.contentItem
                    onPointerStateChanged: function(held){if(held)mainWindow.taskPointerRow=taskRow;else if(mainWindow.taskPointerRow===taskRow)mainWindow.taskPointerRow=null}
                    onDragStateChanged: function(active){mainWindow.draggedTaskRow=active ? taskRow : null;if(active)taskList.currentIndex=index}
                    canMoveUp: index>0
                    canMoveDown: index<taskList.count-1
                    onMenuRequested: {
                        canMoveUp=mainWindow.taskWorkspace.canMoveTask(taskId,-1);canMoveDown=mainWindow.taskWorkspace.canMoveTask(taskId,1)
                        const task=mainWindow.taskWorkspace.task(taskId)
                        hasReminder=!!task.remindAtUtc || (!!task.schedule && task.schedule.alertMinutes>=0)
                        canActivate=task.taskStatus==="OPEN" && (task.planningState==="INBOX" || task.planningState==="SOMEDAY")
                    }
                    onImportantRequested: function(value){const id=taskId;mainWindow.navigateSafely(function(){const item=mainWindow.taskWorkspace.task(id);mainWindow.taskWorkspace.saveDetails(id,item.rowVersion,{important:value})})}
                    onCancelReminderRequested: {const id=taskId;mainWindow.navigateSafely(function(){const item=mainWindow.taskWorkspace.task(id);mainWindow.taskWorkspace.saveDetails(id,item.rowVersion,{remindAtUtc:"",reminderRepeat:"NONE"})})}
                    onDeleteRequested: mainWindow.requestDeleteTask(taskId)
                    onPinRequested: function(value){const id=taskId;mainWindow.navigateSafely(function(){const item=mainWindow.taskWorkspace.task(id);mainWindow.taskWorkspace.saveDetails(id,item.rowVersion,{pinned:value})})}
                    onMoveToViewRequested: function(view){mainWindow.moveTaskToNavigation(taskId,rowVersion,view)}
                    onEveningRequested: function(value){mainWindow.setTaskEvening(taskId,value)}
                    selectedForBatch: mainWindow.batchSelection.some(item=>item.taskId===taskId)
                    onSelectionRequested: mainWindow.toggleBatch(taskId,rowVersion)
                    onEditRequested: {if(!deleted){taskList.currentIndex=index;mainWindow.openEditor(taskId)}}
                    onQuickEditRequested: {if(!deleted){taskList.currentIndex=index;mainWindow.openQuickEditor(taskId)}}
                    onCompleteRequested: {if(deleted)mainWindow.taskWorkspace.changeState(taskId,rowVersion,"restore");else mainWindow.completeRow(taskRow)}
                    onActivateRequested: {const id=taskId;mainWindow.navigateSafely(function(){const item=mainWindow.taskWorkspace.task(id);editor.close();mainWindow.taskWorkspace.saveTaskWithDetails(id,item.rowVersion,item.title,item.notes,"ACTIVE","","",false,{schedule:null})})}
                    onMoveRequested: function(direction) {const id=taskId;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.moveTask(id,direction)})}
                    onDropRequested: function(source) {const target=taskId;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.dropTask(source,target)})}
                }
                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(parent.width,420*Theme.scale)
                    visible: mainWindow.taskWorkspace.tasks.count === 0
                    heading: mainWindow.taskWorkspace.view==="trash" ? qsTr("没有最近删除的任务") : mainWindow.taskWorkspace.view==="completed" ? qsTr("还没有已完成的任务") : !mainWindow.taskWorkspace.ready ? qsTr("正在准备任务") : search.text.length>0 ? qsTr("没有匹配的任务") : mainWindow.taskWorkspace.view==="today" ? qsTr("今天暂无待办") : mainWindow.taskWorkspace.view==="inbox" ? qsTr("收集箱已清空") : qsTr("这里暂时没有任务")
                    detail: mainWindow.taskWorkspace.view==="trash" ? qsTr("删除的任务可在这里恢复。") : mainWindow.taskWorkspace.view==="completed" ? qsTr("完成的任务按完成时间保留在这里。") : mainWindow.taskWorkspace.view==="logbook" ? qsTr("已结束的任务会保留在这里，方便回顾。") : search.text.length>0 ? qsTr("换一个关键词，或清除筛选再试。") : qsTr("在上方记下任务，再安排时间和归属。")
                    actionText: !mainWindow.taskWorkspace.ready || mainWindow.taskWorkspace.view==="completed" || mainWindow.taskWorkspace.view==="trash" || mainWindow.taskWorkspace.view==="logbook" ? "" : search.text.length>0 ? qsTr("清除搜索") : mainWindow.selectedTag || mainWindow.selectedProject ? qsTr("清除筛选") : qsTr("新建任务")
                    onTriggered: {if(search.text.length>0)search.clear();else if(mainWindow.selectedTag || mainWindow.selectedProject){tagFilter.currentIndex=0;projectFilter.currentIndex=0;mainWindow.selectedTag="";mainWindow.selectedProject="";mainWindow.taskWorkspace.setTagFilter("");mainWindow.taskWorkspace.setProjectFilter("")}else mainWindow.newTask("")}
                }
            }
            RowLayout {
                visible: mainWindow.taskContentVisible && mainWindow.taskWorkspace.view === "upcoming" && mainWindow.taskWorkspace.deadlineTasks.count>0
                Layout.fillWidth: true
                Label { text: qsTr("临近截止"); color: Theme.danger }
                ComboBox { id: approaching; Layout.fillWidth: true; model: mainWindow.taskWorkspace.deadlineTasks; textRole: "title"; valueRole: "taskId" }
                ActionButton {symbol: "expand"; text: qsTr("查看任务"); onClicked: mainWindow.openEditor(approaching.currentValue) }
            }
            RowLayout {
                visible: !mainWindow.toolPageVisible
                Label { text: mainWindow.calendarVisible ? qsTr("任务") : qsTr("%1 项").arg(mainWindow.taskWorkspace.tasks.count); color: Theme.secondary; Layout.fillWidth: true;elide: Text.ElideRight }
                ActionButton {symbol: "plus";visible: mainWindow.taskWorkspace.view === "upcoming";text: qsTr("未来%1天 · 再显示30天").arg(mainWindow.taskWorkspace.upcomingDays);onClicked: mainWindow.taskWorkspace.loadMoreUpcoming()}
                ActionButton {symbol: "undo"; visible: mainWindow.taskWorkspace.canUndo; text: qsTr("撤销删除"); enabled: !mainWindow.taskWorkspace.busy; onClicked: mainWindow.taskWorkspace.undoDelete() }
                ActionButton {symbol: "undo";visible: mainWindow.taskWorkspace.canUndoEvent;text: qsTr("撤销删除事件");enabled: !mainWindow.taskWorkspace.busy;onClicked: mainWindow.taskWorkspace.undoEventDelete()}
                Label {
                    visible: !mainWindow.editingActive && (!mainWindow.taskWorkspace.ready || (mainWindow.taskWorkspace.busy && !mainWindow.taskWorkspace.backgroundBusy))
                    text: mainWindow.editingActive && mainWindow.draftDirty ? qsTr("详情有未保存修改") : !mainWindow.taskWorkspace.ready ? qsTr("数据未就绪") : qsTr("正在处理…")
                    color: Theme.secondary
                }
            }
        }
    }

    Pane {
        id: editor
        objectName: "taskEditor"
        visible: false
        z: 1
        readonly property real bodyWidth: Math.min(width-32,720*Theme.scale)
        readonly property real bodyInset: (width-bodyWidth)/2
        x: mainWindow.detailDocked ? mainWindow.width-width : mainWindow.sidebarWidth
        y: 0
        width: mainWindow.detailDocked ? mainWindow.detailWidth : mainWindow.width-mainWindow.sidebarWidth
        height: mainWindow.height
        padding: 0
        function open() {visible=true}
        function verifyLeafDetails() {
            if(!visible || mainWindow.editorTab!==0 || !mainWindow.editorIsSubtask || !editorScroll.visible || subtasks.visible || newSubtask.visible)return "child surface state"
            const at=editTitle.mapToItem(editorScroll,0,0)
            if(at.y<0 || at.y+editTitle.height>editorScroll.height+1 || editorScroll.height<96)return "child detail title outside viewport"
            return "PASS"
        }
        function close() {mainWindow.inlineAttachWanted=false;timingFields.closePickers();startDate.closePicker();deadline.closePicker();reminder.closePicker();quickEditor.close();visible=false;if(mainWindow.editorSaveError && mainWindow.taskWorkspace.validationError)mainWindow.taskWorkspace.clearError();mainWindow.editorSaveError=false}
        onWidthChanged: if(visible)Qt.callLater(function(){mainWindow.ensureEditorFocus(mainWindow.activeFocusItem)})
        onHeightChanged: if(visible)Qt.callLater(function(){mainWindow.ensureEditorFocus(mainWindow.activeFocusItem)})
        background: Rectangle {
            color: Theme.sidebar
            Rectangle {visible: mainWindow.detailDocked;width: 1;height: parent.height;color: Theme.separator}
        }
        contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 12
            Layout.bottomMargin: 8
            Layout.leftMargin: editor.bodyInset
            Layout.rightMargin: editor.bodyInset
            IconToolButton {objectName: "returnFromEditor";contentItem: AppIcon {name: "left"} Accessible.name: mainWindow.editorReturnTaskId ? qsTr("返回父任务的子任务列表，Esc") : mainWindow.calendarVisible ? qsTr("返回日历，Esc") : qsTr("返回任务列表，Esc");enabled: !mainWindow.taskWorkspace.busy;onClicked: mainWindow.returnFromDetails()}
            Label {text: mainWindow.editorIsSubtask ? qsTr("子任务详情") : qsTr("任务详情");font.pixelSize: Theme.headingSize;font.weight: Font.DemiBold;Layout.fillWidth: true}
            Label {visible: mainWindow.draftDirty || (mainWindow.taskWorkspace.busy && !mainWindow.taskWorkspace.backgroundBusy);text: mainWindow.taskWorkspace.busy && !mainWindow.taskWorkspace.backgroundBusy ? qsTr("正在处理…") : qsTr("未保存");font.pixelSize: Theme.captionSize;color: Theme.secondary}
            Button {objectName: "saveTaskEditor";text: qsTr("保存");highlighted: true;enabled: !mainWindow.taskWorkspace.busy;onClicked: {mainWindow.pendingAction=null;mainWindow.saveEditor()}}
            IconToolButton {
                objectName: "editorMore";contentItem: AppIcon {name: "more"} Accessible.name: qsTr("任务操作");enabled: !mainWindow.taskWorkspace.busy
                onClicked: editorMenu.open()
                Menu {
                    id: editorMenu
                    MenuItem {objectName: "taskInfoToggle";text: qsTr("查看编辑记录");onTriggered: {mainWindow.taskWorkspace.requestTaskEditHistory(mainWindow.editId);taskHistoryDialog.open()}}
                    MenuSeparator {}
                    MenuItem {text: qsTr("取消任务");enabled: mainWindow.editStatus==="OPEN";onTriggered: mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.changeState(mainWindow.editId,mainWindow.editVersion,"cancel");editor.close()})}
                    MenuItem {text: qsTr("删除任务");palette.text: Theme.danger;onTriggered: mainWindow.requestDeleteTask(mainWindow.editId)}
                }
            }
        }
        Label {objectName: "editorSaveError";Layout.fillWidth: true;Layout.leftMargin: editor.bodyInset;Layout.rightMargin: editor.bodyInset;Layout.bottomMargin: 8;wrapMode: Text.Wrap;visible: mainWindow.taskWorkspace.errorText.length>0;text: mainWindow.taskWorkspace.errorText;color: Theme.danger;font.pixelSize: Theme.captionSize}
        SegmentedPicker {
            objectName: "taskEditorTabs";visible: !mainWindow.editorIsSubtask || mainWindow.editorHasChildren;Layout.fillWidth: true;Layout.leftMargin: editor.bodyInset;Layout.rightMargin: editor.bodyInset
            model: [qsTr("详情"),mainWindow.editorIsSubtask ? qsTr("已有下级") : qsTr("子任务")];selectionIsExternal: true;currentIndex: mainWindow.editorTab;onActivated: function(index){mainWindow.selectEditorTab(index)}
        }
        ActionButton {
            objectName: "returnToParentSubtasks";visible: mainWindow.editorReturnTaskId.length>0 && mainWindow.editorTab===0
            Layout.leftMargin: editor.bodyInset;Layout.rightMargin: editor.bodyInset;Layout.bottomMargin: 4
            symbol: "left";text: qsTr("%1 · 子任务").arg(mainWindow.taskWorkspace.task(mainWindow.editorReturnTaskId).title || qsTr("父任务"));font.pixelSize: Theme.captionSize
            onClicked: mainWindow.returnToParentTasks()
        }
        GridLayout {
            Layout.fillWidth: true;Layout.fillHeight: true;columns: 1
            ColumnLayout {
                visible: mainWindow.editorTab===1;Layout.fillWidth: true;Layout.fillHeight: true
                Layout.leftMargin: editor.bodyInset;Layout.rightMargin: editor.bodyInset;Layout.topMargin: 8;Layout.bottomMargin: 8;spacing: 4
                Label {text: mainWindow.editorRootTitle;font.pixelSize: Theme.navigationSize;font.weight: Font.DemiBold;Layout.fillWidth: true;elide: Text.ElideRight}
                TextField {id: newSubtask;enabled: !mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy;objectName: "newSubtaskTitle";visible: !mainWindow.editorIsSubtask;Layout.fillWidth: true;placeholderText: qsTr("添加子任务，Enter 确认");font.pixelSize: Theme.listTitleSize;onTextEdited: {const drafts=Object.assign({},mainWindow.pendingSubtaskTitles);drafts[mainWindow.editorRootId]=text;mainWindow.pendingSubtaskTitles=drafts} onAccepted: {const textValue=text;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.createSubtask(textValue,mainWindow.editorRootId)})}}
                ListView {
                    id: subtasks;objectName: "subtaskList";Layout.fillWidth: true;Layout.fillHeight: true;Layout.minimumHeight: 96;clip: true;model: []
                    cacheBuffer: 0
                    DS.ScrollPhysics {target: subtasks}
                    ScrollBar.vertical: ScrollBar {}
                    function refresh(){model=mainWindow.taskWorkspace.subtaskChoices(mainWindow.editorRootId)}
                    function centerForVerification(id){for(let i=0;i<count;i++){const row=itemAtIndex(i);if(!row || row.taskId!==id || !row.visible)continue;const at=row.mapToItem(subtasks,0,0);if(at.y<0 || at.y+row.height>height)return {};const scene=row.mapToItem(null,row.width*0.45,row.height/2);return {x:scene.x,y:scene.y}}return {}}
                    function openForVerification(id){for(let i=0;i<count;i++){const row=itemAtIndex(i);if(row && row.taskId===id && row.visible && row.width>0){row.clicked();return true}}return false}
                    delegate: ItemDelegate {
                        id: childRow
                        required property var modelData
                        readonly property string taskId: modelData.taskId
                        objectName: "subtaskEntry-"+taskId
                        enabled: !mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy
                        width: subtasks.width;implicitHeight: Math.max(40,contentItem.implicitHeight+4)
                        leftPadding: 0;rightPadding: 0;topPadding: 2;bottomPadding: 2
                        Accessible.name: qsTr("打开子任务详情：%1").arg(modelData.title)
                        onClicked: mainWindow.openSubtask(taskId)
                        contentItem: RowLayout {
                            spacing: 6
                            IconToolButton {
                                objectName: "completeSubtask-"+childRow.taskId
                                contentItem: AppIcon {name: childRow.modelData.taskStatus==="OPEN" ? "circle" : "check"}
                                Accessible.name: (childRow.modelData.taskStatus==="OPEN" ? qsTr("完成子任务：") : qsTr("重新打开子任务："))+childRow.modelData.title
                                onClicked: {const item=childRow.modelData;mainWindow.navigateSafely(function(){mainWindow.taskWorkspace.changeState(item.taskId,item.rowVersion,item.taskStatus==="OPEN" ? "complete" : "reopen")})}
                            }
                            ColumnLayout {
                                Layout.fillWidth: true;spacing: 2
                                Label {text: childRow.modelData.title;Layout.fillWidth: true;elide: Text.ElideRight;font.pixelSize: Theme.listTitleSize;font.strikeout: childRow.modelData.taskStatus!=="OPEN"}
                                Label {text: [childRow.modelData.notes || "",childRow.modelData.startOn || "",childRow.modelData.deadlineOn ? qsTr("结束 %1").arg(childRow.modelData.deadlineOn) : ""].filter(value=>value.length>0).join(" · ");visible: text.length>0;Layout.fillWidth: true;maximumLineCount: 1;elide: Text.ElideRight;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                            }
                            WidgetAction {objectName: "subtaskDetails-"+childRow.taskId;symbol: "info";description: qsTr("子任务详情：%1").arg(childRow.modelData.title);onClicked: mainWindow.openSubtask(childRow.taskId)}
                        }
                    }
                    Label {anchors.centerIn: parent;visible: subtasks.count===0;text: qsTr("还没有子任务");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                    Connections {target: mainWindow.taskWorkspace;function onStateChanged(){subtasks.refresh()}}
                    Connections {target: mainWindow;function onEditorRootIdChanged(){subtasks.refresh()}}
                }
            }
        Flickable {
            DS.ScrollPhysics {target: editorScroll}
            id: editorScroll
            visible: mainWindow.editorTab===0
            Layout.fillWidth: true
            objectName: "taskEditorScroll"
            Layout.preferredWidth: editor.bodyWidth
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: editorFields.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            clip: true
            // A deliberate scroll wins over a queued focus reveal.
            onMovementStarted: {mainWindow.editorManualScroll=true;editorFocusDelay.stop()}
            WheelHandler {
                target: null
                blocking: false
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: function(event) {mainWindow.editorManualScroll=true;editorFocusDelay.stop();event.accepted=false}
            }
            onContentHeightChanged: if(editor.visible)editorFocusDelay.restart()
            ScrollBar.vertical: ScrollBar {
                objectName: "editorScrollBar"
                x: editorScroll.width-width
                policy: ScrollBar.AsNeeded
            }
            ColumnLayout {
            id: editorFields
            onHeightChanged: if(editor.visible)editorFocusDelay.restart()
            width: Math.min(editorScroll.width-12,760*Theme.scale)
            x: (editorScroll.width-width)/2
            spacing: Theme.fieldGap
            enabled: !mainWindow.taskWorkspace.busy || mainWindow.taskWorkspace.backgroundBusy
            Control {
                Layout.fillWidth: true
                padding: 12
                implicitHeight: implicitContentHeight+topPadding+bottomPadding
                background: Rectangle {radius: Theme.panelRadius;color: Theme.dark ? Theme.grouped : Theme.content}
                contentItem: ColumnLayout {
                    spacing: 4
            TextField {
                id: editTitle;objectName: "editTaskTitle";Layout.fillWidth: true
                placeholderText: qsTr("任务标题");Accessible.name: qsTr("任务标题")
                font.pixelSize: Theme.taskTitleSize;font.weight: Font.DemiBold
                leftPadding: 0;rightPadding: 0
                background: Rectangle {color: "transparent";Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;visible: editTitle.activeFocus;color: Theme.accent}}
                onTextChanged: mainWindow.markDirty()
            }
            TextArea {
                id: editNotes;objectName: "editTaskNotes";Layout.fillWidth: true
                implicitHeight: Math.max(40,contentHeight+topPadding+bottomPadding)
                leftPadding: 0;rightPadding: 0
                placeholderText: qsTr("添加备注…");Accessible.name: qsTr("备注");wrapMode: TextEdit.Wrap
                background: Rectangle {color: "transparent";Rectangle {anchors.bottom: parent.bottom;width: parent.width;height: 1;visible: editNotes.activeFocus;color: Theme.accent}}
                onTextChanged: {mainWindow.markDirty();if(activeFocus)mainWindow.editorManualScroll=false}
                // Native IME/cursor updates can arrive after a wheel event.
                // Only deliberate editing input overrides the manual scroll.
                Keys.onPressed: function(event){mainWindow.editorManualScroll=false;Qt.callLater(mainWindow.ensureNotesCursor);event.accepted=false}
                TapHandler {onPressedChanged: if(pressed){mainWindow.editorManualScroll=false;Qt.callLater(mainWindow.ensureNotesCursor)}}
                onCursorPositionChanged: Qt.callLater(mainWindow.ensureNotesCursor)
                onCursorRectangleChanged: Qt.callLater(mainWindow.ensureNotesCursor)
            }
                }
            }
            DetailSection {
                objectName: "editorDateTimeSection"
                heading: qsTr("日期与时间");symbol: "calendar"
                RowLayout {
                    Layout.fillWidth: true
                    Label {visible: mainWindow.draftPlanningState==="SOMEDAY";text: qsTr("此旧任务的日期与提醒已暂停");font.pixelSize: Theme.captionSize;color: Theme.secondary;Layout.fillWidth: true;wrapMode: Text.Wrap}
                    Button {visible: mainWindow.draftPlanningState==="SOMEDAY";text: qsTr("恢复安排");onClicked: {mainWindow.draftPlanningState="ACTIVE";mainWindow.markDirty()}}
                    Button {objectName: "scheduleToday";text: qsTr("今天");onClicked: startDate.text=mainWindow.taskWorkspace.dateOffset(0)}
                    Item {Layout.fillWidth: true}
                }
                GridLayout {
                    objectName: "editorDatesGrid"
                    columns: editorFields.width-24>=440*Theme.scale ? 2 : 1
                    Layout.fillWidth: true
                    columnSpacing: 16;rowSpacing: 8
                    ColumnLayout {
                        id: startDateGroup
                        Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                        Label {text: qsTr("开始");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                        DateField {id: startDate;objectName: "editStartDate";workspace: mainWindow.taskWorkspace;dateLabel: qsTr("开始日期");Layout.fillWidth: true;onPickerVisibleChanged: if(pickerVisible)deadline.closePicker();onTextChanged: mainWindow.dateDraftChanged(text)}
                    }
                    ColumnLayout {
                        id: deadlineGroup
                        Layout.fillWidth: true;Layout.preferredWidth: 1;spacing: 4
                        Label {text: qsTr("结束");font.pixelSize: Theme.captionSize;color: Theme.secondary}
                        DateField {id: deadline;objectName: "editDeadlineDate";workspace: mainWindow.taskWorkspace;dateLabel: qsTr("结束日期");Layout.fillWidth: true;onPickerVisibleChanged: if(pickerVisible)startDate.closePicker();onTextChanged: mainWindow.dateDraftChanged(text)}
                    }
                }
                Label {visible: mainWindow.timeDatesDiffer && !mainWindow.draftDatesDirty && !timingFields.timeDirty;Layout.fillWidth: true;wrapMode: Text.Wrap;font.pixelSize: Theme.captionSize;color: Theme.secondary;text: qsTr("原日期安排：%1 — %2；修改安排时会统一。").arg(mainWindow.originalStartOn || qsTr("未设置")).arg(mainWindow.originalEndOn || qsTr("未设置"))}
                RowLayout {
                    Layout.fillWidth: true;spacing: 12
                    Label {text: qsTr("提醒");font.pixelSize: Theme.bodySize;Layout.preferredWidth: Theme.bodySize*3}
                    ReminderField {id: reminder;workspace: mainWindow.taskWorkspace;objectName: "editorReminder";Layout.fillWidth: true;placeholderText: qsTr("添加提醒…");Accessible.name: qsTr("本地时间提醒，留空清除");onTextChanged: mainWindow.markDirty();onRepeatFrequencyChanged: mainWindow.markDirty();onRepeatIntervalChanged: mainWindow.markDirty()}
                }
                Label {visible: reminder.repeatSummary.length>0;Layout.fillWidth: true;text: reminder.repeatSummary;font.pixelSize: Theme.captionSize;color: Theme.secondary}
                TaskTimingFields {id: timingFields;objectName: "taskTimingFields";workspace: mainWindow.taskWorkspace;startDateValue: startDate.text;endDateValue: deadline.text;datesEdited: mainWindow.draftDatesDirty;Layout.fillWidth: true;onDateDefaultsRequested: {if(!startDate.text)startDate.text=mainWindow.taskWorkspace.dateOffset(0);if(!deadline.text)deadline.text=startDate.text} onDraftChanged: mainWindow.markDirty()}
            }
            TaskDetailsFields {
                id: extraFields
                onYChanged: if(editor.visible)editorFocusDelay.restart()
                onHeightChanged: if(editor.visible)editorFocusDelay.restart()
                infoExpanded: taskHistoryDialog.visible
                objectName: "taskDetailsFields"
                Layout.fillWidth: true
                workspace: mainWindow.taskWorkspace
                onFocusRequested: function(item){mainWindow.editorFocusTarget=item;editorFocusDelay.restart()}
                onCreateCatalogRequested: function(kind){catalogCreate.showFor(kind)}
                onDraftChanged: mainWindow.markDirty()
                onOpenEvent: function(id){mainWindow.navigateSafely(function(){editor.close();calendarPanel.openEvent(id)})}
            }

            }
        }
        }

        }
    }

    Dialog {
        id: taskHistoryDialog;objectName: "taskEditHistoryDialog";parent: Overlay.overlay;anchors.centerIn: parent
        title: qsTr("编辑记录");modal: true;width: Math.min(560,parent.width-32);height: Math.min(480,parent.height-32)
        contentItem: ScrollView {
            clip: true
            ColumnLayout {width: taskHistoryDialog.availableWidth-12;spacing: 12
                Label {text: qsTr("创建于 %1").arg(mainWindow.taskWorkspace.localReminder(extraFields.taskData.createdAtUtc || ""));Layout.fillWidth: true;wrapMode: Text.Wrap;color: Theme.secondary}
                Label {visible: mainWindow.taskWorkspace.historyLoading;text: qsTr("正在读取…")}
                Label {visible: !mainWindow.taskWorkspace.historyLoading && mainWindow.taskWorkspace.taskEditHistory.length===0;text: qsTr("尚无编辑记录")}
                Repeater {model: mainWindow.taskWorkspace.taskEditHistory
                    ColumnLayout {required property var modelData;Layout.fillWidth: true
                        Label {text: mainWindow.taskWorkspace.localReminder(modelData.editedAt);font.pixelSize: Theme.captionSize;color: Theme.secondary}
                        Label {text: modelData.summary;textFormat: Text.PlainText;Layout.fillWidth: true;wrapMode: Text.Wrap}
                    }
                }
            }
        }
        footer: ActionBar {Button {text: qsTr("关闭");onClicked: taskHistoryDialog.close()}}
    }

    TaskQuickEditor {
        id: quickEditor
        parent: mainWindow.inlineRow ? mainWindow.inlineRow.editorHost : mainWindow.contentItem
        width: parent ? parent.width : 0
        height: implicitHeight
        workspace: mainWindow.taskWorkspace
        guardActive: leaveDialog.visible || catalogCreate.visible
        dirty: mainWindow.draftDirty
        destinations: extraFields.destinations;tags: extraFields.tags;tagIds: extraFields.tagIds;destinationId: extraFields.selectedDestination.id
        reminderRepeat: reminder.repeatFrequency;reminderEvery: reminder.repeatInterval
        onEditingChanged: if(!editing){mainWindow.inlineAttachWanted=false;mainWindow.inlineRow=null;mainWindow.quickFocusTarget=null;quickFocusDelay.stop()}
        onReminderCycleEdited: function(frequency,interval){reminder.repeatFrequency=frequency;reminder.repeatInterval=interval}
        reminderText: reminder.text
        onReminderEdited: function(value){reminder.text=value;mainWindow.pendingAction=null;mainWindow.saveEditor()}
        onDestinationEdited: function(kind,id){extraFields.selectDestination(kind,id,"")}
        onTagEdited: function(id,checked){extraFields.toggleTag(id,checked)}
        onCreateCatalogRequested: function(kind){catalogCreate.showFor(kind)}
        onFocusRequested: function(item){mainWindow.quickManualScroll=false;mainWindow.quickFocusTarget=item;quickFocusDelay.restart()}
        onCompleteRequested: if(mainWindow.inlineRow)mainWindow.completeRow(mainWindow.inlineRow)
        titleText: editTitle.text;notesText: editNotes.text;startOn: startDate.text;deadlineOn: deadline.text
        onTitleEdited: function(value){editTitle.text=value}
        onNotesEdited: function(value){editNotes.text=value}
        onStartEdited: function(value){startDate.text=value}
        onDeadlineEdited: function(value){deadline.text=value}
        onDetailsRequested: {close();editor.open();editTitle.forceActiveFocus()}
        onCloseRequested: if(!leaveDialog.visible)mainWindow.navigateSafely(function(){editor.close();taskList.forceActiveFocus()})
        onSaveRequested: {mainWindow.pendingAction=null;mainWindow.saveEditor()}
    }

    Dialog {
        id: leaveDialog
        objectName: "unsavedChangesDialog"
        parent: Overlay.overlay
        anchors.centerIn: parent
        title: qsTr("有未保存的修改")
        modal: true
        closePolicy: Popup.NoAutoClose
        width: Math.min(560,parent.width-32)
        Label {width: leaveDialog.availableWidth;wrapMode: Text.Wrap;text: qsTr("保存修改后继续，或明确放弃此次编辑。")}
        Shortcut {sequence: "Escape";enabled: leaveDialog.visible;onActivated: {mainWindow.pendingAction=null;mainWindow.forceQuit=false;leaveDialog.close()}}
        footer: Flow {
            padding: 16;spacing: 8
            Button { objectName: "cancelLeave"; text: qsTr("继续编辑"); onClicked: { mainWindow.pendingAction = null; mainWindow.forceQuit=false;leaveDialog.close() } }
            Button {
                objectName: "discardDraft"
                text: qsTr("放弃修改")
                palette.buttonText: Theme.danger
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
        id: deleteParentConfirm;objectName: "deleteParentConfirm";parent: Overlay.overlay;anchors.centerIn: parent
        title: qsTr("删除父任务？");modal: true;width: Math.min(420*Theme.scale,parent.width-32)
        contentItem: Label {text: qsTr("“%1”及其全部子任务将移到最近删除。你可以一并恢复它们；此前单独删除的子任务不会被恢复。").arg(mainWindow.deleteTaskTarget.title || "");wrapMode: Text.Wrap}
        footer: ActionBar {
            Button {objectName: "cancelDeleteParent";text: qsTr("取消");onClicked: deleteParentConfirm.close()}
            Button {objectName: "confirmDeleteParent";text: qsTr("删除任务及子任务");palette.buttonText: Theme.danger;enabled: !mainWindow.taskWorkspace.busy;onClicked: {const item=mainWindow.deleteTaskTarget;deleteParentConfirm.close();editor.close();mainWindow.taskWorkspace.changeState(item.taskId,item.rowVersion,"delete")}}
        }
    }
    Menu {
        id: categoryContextMenu;objectName: "categoryContextMenu";parent: Overlay.overlay
        MenuItem {objectName: "renameCatalog";text: qsTr("重命名…");onTriggered: mainWindow.manageCatalogItem(mainWindow.catalogContextItem,false)}
        MenuItem {text: qsTr("编辑%1…").arg(mainWindow.catalogContextItem.kind==="tag" ? qsTr("标签") : qsTr("分类"));onTriggered: mainWindow.manageCatalogItem(mainWindow.catalogContextItem,false)}
        MenuItem {text: qsTr("新建二级分类…");visible: mainWindow.catalogContextItem.kind!=="tag" && !mainWindow.catalogContextItem.parentId;onTriggered: mainWindow.createChildCategory(mainWindow.catalogContextItem.id)}
        MenuItem {text: mainWindow.catalogContextItem.parentId ? qsTr("移动 / 提升为一级…") : qsTr("移为二级分类…");visible: mainWindow.catalogContextItem.kind==="project";onTriggered: mainWindow.manageCatalogItem(mainWindow.catalogContextItem,true)}
        MenuSeparator {}
        MenuItem {text: qsTr("上移");enabled: mainWindow.taskWorkspace.canMoveCatalog(mainWindow.catalogContextItem.kind || "",mainWindow.catalogContextItem.id || "",-1);onTriggered: mainWindow.catalogAction("up")}
        MenuItem {text: qsTr("下移");enabled: mainWindow.taskWorkspace.canMoveCatalog(mainWindow.catalogContextItem.kind || "",mainWindow.catalogContextItem.id || "",1);onTriggered: mainWindow.catalogAction("down")}
        MenuItem {text: mainWindow.catalogContextItem.archived ? qsTr("取消归档") : qsTr("归档分类");visible: mainWindow.catalogContextItem.kind!=="tag";onTriggered: mainWindow.catalogAction("archive")}
        MenuItem {text: qsTr("删除…");onTriggered: mainWindow.catalogAction("delete")}
    }
    Dialog {
        id: catalogDeleteConfirm;parent: Overlay.overlay;anchors.centerIn: parent;width: Math.min(460,parent.width-32);modal: true;title: qsTr("删除 %1？").arg(mainWindow.catalogContextItem.title || "");standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: Label {wrapMode: Text.Wrap;text: mainWindow.catalogContextItem.kind==="tag" ? qsTr("仅移除标签及任务上的标记，任务和分类保持不变。") : qsTr("任务会保留并移出此分类；二级分类会提升为一级。")}
        onAccepted: mainWindow.taskWorkspace.deleteCatalog(mainWindow.catalogContextItem.kind,mainWindow.catalogContextItem.id,mainWindow.catalogContextItem.rowVersion)
    }
    CatalogEditPopup {id: catalogEdit;parent: Overlay.overlay;workspace: mainWindow.taskWorkspace;onSaved: function(id){mainWindow.expandCategoryParent(id)}}
    CatalogCreateDialog {
        id: catalogCreate;parent: Overlay.overlay;anchors.centerIn: parent;workspace: mainWindow.taskWorkspace
        onCreated: function(kind,id){
            if(catalogCreate.selectAfterCreate)extraFields.applyCreated(kind,id)
            else if(catalogCreate.forManagement){const item=mainWindow.taskWorkspace.categoryTree.concat(mainWindow.taskWorkspace.catalogOf("tag")).find(value=>value.id===id);if(item)organization.choose(item);mainWindow.expandCategoryParent(id)}
            else mainWindow.changeView(kind+":"+id)
        }
    }

    SettingsDialog {
        id: windowSettings
        onQuitRequested: {windowSettings.close();mainWindow.forceQuit=true;mainWindow.close()}
        onCalendarVisibilityRequested: function(visible){if(visible)mainWindow.taskWorkspace.setPreference("desktopCalendarVisible",true);else desktopCalendar.close()}
        parent: Overlay.overlay
        anchors.centerIn: parent
        workspace: mainWindow.taskWorkspace
        widgetController: mainWindow.widgetController
        startupController: mainWindow.startupController
    }

    Connections {
        target: mainWindow.taskWorkspace
        function onCategoryRelocated(id){mainWindow.expandCategoryParent(id)}
        function onSubtaskCreated(id){const drafts=Object.assign({},mainWindow.pendingSubtaskTitles);delete drafts[mainWindow.editorRootId];mainWindow.pendingSubtaskTitles=drafts;newSubtask.clear();mainWindow.editorTab=1;subtasks.refresh();newSubtask.forceActiveFocus()}
        function onOperationFinished(operation, success) {
            if(operation==="complete"){if(!success && mainWindow.completionRow)mainWindow.completionRow.cancelCompletion();mainWindow.completionRow=null}
            if (operation === "create" && success) { newTitle.clear(); newTitle.forceActiveFocus() }
            if (operation === "save" && mainWindow.editorSaving) {
                mainWindow.editorSaving=false
                if (success) {
                    mainWindow.editVersion = mainWindow.taskWorkspace.task(mainWindow.editId).rowVersion
                    mainWindow.draftDirty = false
                    const item=mainWindow.taskWorkspace.task(mainWindow.editId)
                    if(editor.visible && mainWindow.editorReturnTaskId && item.parentTaskId===mainWindow.editorReturnTaskId && !mainWindow.pendingAction){const parentId=mainWindow.editorReturnTaskId;mainWindow.loadEditor(parentId);mainWindow.editorTab=1;subtasks.refresh()}else editor.close()
                    const action = mainWindow.pendingAction
                    mainWindow.pendingAction = null
                    if (action) action()
                } else {mainWindow.pendingAction = null;mainWindow.editorSaveError=true;if(quickEditor.editing)quickEditor.focusTitle()}
            }
        }
    }
    DesktopCalendarWindow {
        id: desktopCalendar
        workspace: mainWindow.taskWorkspace
        onOpenTask: function(taskId){mainWindow.showNormal();mainWindow.requestActivate();mainWindow.openEditor(taskId)}
    }
    WidgetWindow {
        controller: mainWindow.widgetController
        workspace: mainWindow.taskWorkspace
        onNewTaskRequested: {mainWindow.showNormal();mainWindow.requestActivate();mainWindow.newTask("today")}
        onOpenMainRequested: {mainWindow.showNormal();mainWindow.requestActivate()}
        onSettingsRequested: {mainWindow.showNormal();mainWindow.requestActivate();mainWindow.openTool("settings")}
    }
    StyleGallery {id: styleGallery;parent: Overlay.overlay;anchors.centerIn: parent}
    BatchTaskDialog {id: batchDialog;parent: Overlay.overlay;anchors.centerIn: parent;workspace: mainWindow.taskWorkspace;onCommitted: {mainWindow.batchSelection=[];mainWindow.batchMode=false}}
    DataToolsDialog { id: dataTools; parent: Overlay.overlay; anchors.centerIn: parent; workspace: mainWindow.taskWorkspace }
    DatePickerDialog {id: datePicker;objectName: "taskDatePicker";parent: Overlay.overlay;anchors.centerIn: parent;workspace: mainWindow.taskWorkspace;onRejected: mainWindow.pendingNavigationMove=({});onDateSelected: function(date,field){if(field==="upcoming")mainWindow.commitNavigationMove(date);else if(field==="start")startDate.text=date;else deadline.text=date}}
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
        contentItem: Label {wrapMode: Text.Wrap;text: qsTr("开始日期将从 %1 调整为今天，结束日期保持不变。").arg(mainWindow.todayTarget.startOn || "")}
        onAccepted: mainWindow.applyToday()
    }
    onClosing: function(close) {
        if (taskWorkspace.busy) { close.accepted = false; return }
        if(catalogEdit.visible && catalogEdit.dirty){close.accepted=false;catalogEdit.requestClose();return}
        if(desktopCalendar.protectClose(function(){mainWindow.close()})){close.accepted=false;return}
        if(calendarPanel.protectClose(function(){mainWindow.close()})){close.accepted=false;return}
        if (organization.visible && organization.dirty) {close.accepted=false;organization.safely(function(){organization.close();mainWindow.close()});return}
        if (editingActive && draftDirty) {
            close.accepted = false
            navigateSafely(function() { mainWindow.close() })
        } else if (!forceQuit && widgetController.trayAvailable && widgetController.closeToTray) {
            close.accepted = false
            if(!taskWorkspace.preferences.closeNoticeSeen)firstClose.open();else mainWindow.hide()
        } else {shuttingDown=true;editor.close();Qt.quit()}
    }
}
