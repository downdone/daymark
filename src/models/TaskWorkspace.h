#pragma once
#include "domain/Task.h"
#include "domain/CalendarEvent.h"
#include <QAbstractListModel>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <memory>
#include <QWindow>
#include <QTimeZone>
#include <QJsonObject>

namespace daymark {
class QtWindowPlatform;

class TaskListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by TaskWorkspace")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(bool hasHierarchy READ hasHierarchy NOTIFY countChanged)
public:
    enum Role { Id=Qt::UserRole+1, Title, Notes, Done, Status, StartOn, DeadlineOn, Focus, Version, Overdue, Evening, ProjectName, TagSummary, Section, HeadingTitle, SubtaskPending, ContextGroup, InToday, ViewSection, ParentTask, ParentTitle, Depth, ChildCount, Pinned, Important, Planning, ParentDeleted, PlannedDate, CompletionMonth, DeletedAt, ArrangementSummary, SubtaskTotal };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex& parent={}) const override { return parent.isValid()?0:m_tasks.size(); }
    QVariant data(const QModelIndex& index,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    void replace(const QList<Task>& tasks,const QDate& today,int groupRole=0);
    const QList<Task>& items() const {return m_tasks;}
    bool hasHierarchy() const {return m_hasHierarchy;}
    static QVariantMap taskMap(const Task& task);
    Q_INVOKABLE QString taskIdAt(int row) const { return row>=0 && row<m_tasks.size() ? m_tasks[row].id : QString(); }
    Q_INVOKABLE QVariantList selectionEntries(int first,int last) const;
signals:
    void countChanged();
private:
    QList<Task> m_tasks;
    bool m_hasHierarchy = false;
    QDate m_today;
    int m_groupRole=0;
    QHash<QString,QString> m_groupHeaders;
};

class TaskWorker;
class TaskWorkspace : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppRuntime")
    Q_PROPERTY(daymark::TaskListModel* tasks READ tasks CONSTANT)
    Q_PROPERTY(daymark::TaskListModel* todayTasks READ todayTasks CONSTANT)
    Q_PROPERTY(daymark::TaskListModel* deadlineTasks READ deadlineTasks CONSTANT)
    Q_PROPERTY(int dragDistance READ dragDistance CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(bool backgroundBusy READ backgroundBusy NOTIFY stateChanged)
    Q_PROPERTY(bool interactionActive READ interactionActive WRITE setInteractionActive NOTIFY interactionActiveChanged)
    Q_PROPERTY(QString view READ view NOTIFY stateChanged)
    Q_PROPERTY(QString scope READ scope NOTIFY stateChanged)
    Q_PROPERTY(bool calendarMode READ calendarMode NOTIFY stateChanged)
    Q_PROPERTY(QString calendarScale READ calendarScale NOTIFY stateChanged)
    Q_PROPERTY(QString calendarDate READ calendarDate NOTIFY stateChanged)
    Q_PROPERTY(bool smartView READ smartView NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap viewCounts READ viewCounts NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(bool validationError READ validationError NOTIFY stateChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY stateChanged)
    Q_PROPERTY(QVariantList categoryTree READ categoryTree NOTIFY stateChanged)
    Q_PROPERTY(QVariantList navigationCatalog READ navigationCatalog NOTIFY stateChanged)
    Q_PROPERTY(bool taskReorderingAllowed READ taskReorderingAllowed NOTIFY stateChanged)
    Q_PROPERTY(QVariantList calendarDays READ calendarDays NOTIFY stateChanged)
    Q_PROPERTY(QVariantList calendarEvents READ calendarEvents NOTIFY stateChanged)
    Q_PROPERTY(bool canUndoEvent READ canUndoEvent NOTIFY stateChanged)
    Q_PROPERTY(QString calendarTitle READ calendarTitle NOTIFY stateChanged)
    Q_PROPERTY(bool sundayFirst READ sundayFirst WRITE setSundayFirst NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recurrenceRules READ recurrenceRules NOTIFY stateChanged)
    Q_PROPERTY(QVariantList taskEditHistory READ taskEditHistory NOTIFY historyChanged)
    Q_PROPERTY(bool historyLoading READ historyLoading NOTIFY historyChanged)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY stateChanged)
    Q_PROPERTY(QString importSummary READ importSummary NOTIFY stateChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY stateChanged)
    Q_PROPERTY(double textScale READ textScale WRITE setTextScale NOTIFY stateChanged)
    Q_PROPERTY(bool includeCompleted READ includeCompleted WRITE setIncludeCompleted NOTIFY stateChanged)
    Q_PROPERTY(int completedToday READ completedToday NOTIFY stateChanged)
    Q_PROPERTY(QString firstTodayTitle READ firstTodayTitle NOTIFY stateChanged)
    Q_PROPERTY(QString localToday READ localToday NOTIFY stateChanged)
    Q_PROPERTY(int upcomingDays READ upcomingDays NOTIFY stateChanged)
    Q_PROPERTY(int historyDays READ historyDays WRITE setHistoryDays NOTIFY stateChanged)
    Q_PROPERTY(bool calendarCompleted READ calendarCompleted WRITE setCalendarCompleted NOTIFY stateChanged)
    Q_PROPERTY(QVariantList unscheduledTasks READ unscheduledTasks NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap systemAccessibility READ systemAccessibility NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap backupInfo READ backupInfo NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap syncState READ syncState NOTIFY syncStateChanged)
    Q_PROPERTY(bool syncApplyPaused READ syncApplyPaused WRITE setSyncApplyPaused NOTIFY syncApplyPausedChanged)
public:
    explicit TaskWorkspace(QObject* parent=nullptr);
    ~TaskWorkspace() override;
    void start(const QString& databasePath);
    QString databasePath() const {return m_path;}
    QVariantMap syncState() const {return m_syncState;}
    bool syncApplyPaused()const{return m_syncApplyPaused;}
    void setSyncApplyPaused(bool paused){if(m_syncApplyPaused==paused)return;m_syncApplyPaused=paused;emit syncApplyPausedChanged();}
    void setSyncState(const QVariantMap& state){m_syncState=state;emit syncStateChanged();}
    void syncExecute(const QString& operation,const QJsonObject& input,std::function<void(Result<QJsonObject>)> completed);
    void switchSyncDatabase(const QString& path,std::function<void(Result<void>)> completed);
    Q_INVOKABLE void syncOpenProfile(const QString& id){emit syncActionRequested(QStringLiteral("open_profile"),{{"id",id}});}
    Q_INVOKABLE void syncLogin(const QString& origin,const QString& login,const QString& password){emit syncActionRequested(QStringLiteral("login"),{{"origin",origin},{"login",login},{"password",password}});}
    Q_INVOKABLE void syncRegister(const QString& origin,const QString& login,const QString& password){emit syncActionRequested(QStringLiteral("register"),{{"origin",origin},{"login",login},{"password",password}});}
    Q_INVOKABLE void syncLogout(){emit syncActionRequested(QStringLiteral("logout"),{});}
    Q_INVOKABLE void syncNow(){emit syncActionRequested(QStringLiteral("now"),{});}
    Q_INVOKABLE void syncRebuild(){emit syncActionRequested(QStringLiteral("rebuild"),{});}
    Q_INVOKABLE void syncResolveConflict(const QString& id,const QString& decision){emit syncActionRequested(QStringLiteral("resolve"),{{"conflict_id",id},{"decision",decision}});}
    TaskListModel* tasks() { return &m_tasks; }
    TaskListModel* todayTasks() { return &m_todayTasks; }
    TaskListModel* deadlineTasks() {return &m_deadlineTasks;}
    int dragDistance() const;
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    bool backgroundBusy() const { return m_busy && m_backgroundMaintenance; }
    bool interactionActive() const {return m_interactionActive;}
    void setInteractionActive(bool active);
    QString view() const { return m_view; }
    QString scope() const {return m_scope;}
    bool calendarMode() const {return QStringList{"day","week","month","year"}.contains(m_view);}
    QString calendarScale() const {return m_calendarScale;}
    QString calendarDate() const {return m_calendarDate.toString(Qt::ISODate);}
    Q_INVOKABLE void setScope(const QString& scope);
    bool smartView() const;
    QVariantMap viewCounts() const;
    QString errorText() const;
    bool validationError() const {return m_error.code==ErrorCode::ValidationError;}
    QString infoText() const { return m_info; }
    bool canUndo() const { return !m_lastDeleted.isEmpty() || !m_lastBatchDeleted.isEmpty(); }
    QVariantList catalog() const;
    QVariantList navigationCatalog() const;
    QVariantList categoryTree() const;
    QVariantList calendarDays() const;
    Q_INVOKABLE QVariantMap timelineFor(const QString& anchor,bool week,bool scoped=true) const;
    Q_INVOKABLE QVariantMap timeRangePreview(const QString& date,const QString& zone,const QString& omittedTask) const;
    Q_INVOKABLE QVariantMap timeRangeDraft(const QVariantMap& values) const;
    Q_INVOKABLE QVariantList calendarWeeksFor(const QString& anchor,int before=1,int after=2,bool scoped=true) const;
    Q_INVOKABLE QVariantList calendarMonthsFor(const QString& anchor) const;
    Q_INVOKABLE QString shiftCalendarScale(const QString& anchor,int offset,const QString& scale) const;
    Q_INVOKABLE QString titleForScale(const QString& anchor,const QString& scale) const;
    Q_INVOKABLE bool moveTaskTime(const QString& id,qint64 version,const QString& date,int minute,const QString& edge);
    Q_INVOKABLE QString defaultReminderLocal() const;
    Q_INVOKABLE QString reminderPreview(const QString& local,const QString& frequency,int interval) const;
    Q_INVOKABLE QString minuteLabel(const QString& date,int minute,const QString& zoneId=QString()) const;
    Q_INVOKABLE bool moveTaskDate(const QString& id,qint64 version,const QString& from,const QString& date,const QString& edge=QStringLiteral("move"));
    Q_INVOKABLE QVariantList unscheduledFor(bool scoped=true)const;
    Q_INVOKABLE QVariantList calendarDaysFor(const QString& anchor,bool week,bool scoped=false) const;
    Q_INVOKABLE QString calendarTitleFor(const QString& anchor,bool week) const;
    Q_INVOKABLE QString shiftCalendar(const QString& anchor,int offset,bool week) const;
    QVariantList calendarEvents() const;
    Q_INVOKABLE QVariantMap calendarEvent(const QString& id) const;
    Q_INVOKABLE QVariantList recurringEvents() const;
    Q_INVOKABLE QVariantMap calendarEventPreview(const QString& id,const QString& occurrenceKey=QString()) const;
    Q_INVOKABLE QVariantList eventsForTask(const QString& id) const;
    Q_INVOKABLE QVariantList eventTaskChoices(const QString& search) const;
    Q_INVOKABLE QString systemTimeZone() const;
    Q_INVOKABLE bool saveEvent(const QVariantMap& event);
    Q_INVOKABLE bool deleteEvent(const QString& id,qint64 version);
    Q_INVOKABLE bool undoEventDelete();
    bool canUndoEvent() const{return !m_eventUndoId.isEmpty();}
    QString calendarTitle() const;
    bool sundayFirst() const {return m_sundayFirst;}
    void setSundayFirst(bool enabled);
    Q_INVOKABLE void stepCalendar(int offset);
    Q_INVOKABLE QString shiftDate(const QString& date,int days)const{return QDate::fromString(date,Qt::ISODate).addDays(days).toString(Qt::ISODate);}
    Q_INVOKABLE void selectCalendarDate(const QString& date){const auto d=QDate::fromString(date,Qt::ISODate);if(d.isValid()){m_calendarDate=d;emit stateChanged();}}
    Q_INVOKABLE bool createForTime(const QString& title,const QString& date,int minute,int duration=60,bool global=false);
    Q_INVOKABLE bool createForDate(const QString& title,const QString& date,bool global=false);
    Q_INVOKABLE QVariantMap calendarDraft(const QString& date,int minute=-1,int duration=60,bool global=false) const;
    Q_INVOKABLE bool createTaskWithDetails(const QString& title,const QString& notes,const QString& start,const QString& end,const QVariantMap& details);
    Q_INVOKABLE bool moveCalendar(const QString& id,qint64 version,const QString& kind,const QString& date);
    QVariantList recurrenceRules() const;
    Q_INVOKABLE QString recurrencePreview(const QVariantMap& values) const;
    Q_INVOKABLE int recurrenceWeekdayMask(const QString& anchor) const;
    Q_INVOKABLE bool saveRecurrence(const QVariantMap& rule);
    Q_INVOKABLE bool stopRecurrence(const QString& id,qint64 version,bool removeFuture);
    QVariantList notifications() const;
    Q_INVOKABLE void maintain();
    Q_INVOKABLE QString localReminder(const QString& utc) const;
    QString importSummary() const {return m_importSummary;}
    Q_INVOKABLE bool exportData(const QString& path,bool csv=false);
    Q_INVOKABLE bool previewImport(const QString& path,bool snapshot=false);
    Q_INVOKABLE bool applyImport(bool replace);
    Q_INVOKABLE bool moveTask(const QString& id,int direction);
    Q_INVOKABLE bool moveTaskToView(const QString& id,qint64 version,const QString& view,const QString& start);
    bool taskReorderingAllowed() const;
    Q_INVOKABLE void setTaskChildrenExpanded(const QString& id,bool expanded);
    Q_INVOKABLE int taskIndex(const QString& id) const;
    Q_INVOKABLE bool selectionHasStatus(const QVariantList& selection,const QString& status) const;
    Q_INVOKABLE bool canMoveTask(const QString& id,int direction) const;
    Q_INVOKABLE bool canDropTask(const QString& source,const QString& target) const;
    Q_INVOKABLE QString dropTaskReason(const QString& source,const QString& target) const;
    Q_INVOKABLE bool dropTask(const QString& source,const QString& target);
    int themeMode() const {return m_themeMode;}
    double textScale() const {return m_textScale;}
    void setThemeMode(int mode);
    void setTextScale(double scale);
    bool includeCompleted() const {return m_includeCompleted;}
    void setIncludeCompleted(bool enabled) {m_includeCompleted=enabled;refresh();emit stateChanged();}
    int completedToday() const;
    QString firstTodayTitle()const{return m_todayTasks.rowCount()>0 ? m_todayTasks.data(m_todayTasks.index(0),TaskListModel::Title).toString() : QString();}
    QString localToday() const {return m_today.toString(QStringLiteral("yyyy年M月d日"));}
    Q_INVOKABLE QString dateOffset(int days) const {return m_today.addDays(days).toString(Qt::ISODate);}
    Q_INVOKABLE QVariantMap datePicker(const QString& anchor,int offset) const;
    Q_INVOKABLE void setTagFilter(const QString& id) {m_tagFilter=id;refresh();emit stateChanged();}
    Q_INVOKABLE void setProjectFilter(const QString& id){m_projectFilter=id;refresh();emit stateChanged();}
    Q_INVOKABLE void refreshClock();
    int upcomingDays()const{return m_upcomingDays;}
    Q_INVOKABLE void loadMoreUpcoming(){m_upcomingDays=qMin(36500,m_upcomingDays+30);refresh();emit stateChanged();}
    Q_INVOKABLE bool todayAfterRemovingFocus(const QString& id) const;
    Q_INVOKABLE int openProjectTasks(const QString& id)const;
    void settingsFailed() {m_error={ErrorCode::PersistenceError,QStringLiteral("settingsWrite")};emit stateChanged();}
    Q_INVOKABLE QVariantList catalogOf(const QString& kind,const QString& parent=QString()) const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setView(const QString& view);
    Q_INVOKABLE void setSearch(const QString& search);
    Q_INVOKABLE QVariantMap task(const QString& id) const;
    QVariantList taskEditHistory() const {return m_taskEditHistory;}
    bool historyLoading() const {return m_historyLoading;}
    Q_INVOKABLE void requestTaskEditHistory(const QString& id);
    Q_INVOKABLE bool createSubtask(const QString& title,const QString& parentId);
    Q_INVOKABLE QVariantList subtaskChoices(const QString& parentId) const;
    Q_INVOKABLE QVariantList parentTaskChoices(const QString& taskId,const QString& search) const;
    Q_INVOKABLE bool requestCalendarRange(const QString& anchor);
    Q_INVOKABLE bool requestCalendarSpan(const QString& from,const QString& through);
    Q_INVOKABLE bool createTask(const QString& title);
    Q_INVOKABLE QVariantMap listDraft() const;
    Q_INVOKABLE bool createInlineTask(const QString& title,const QString& notes,const QString& planning,const QString& start,const QString& end,bool focus,const QVariantMap& details);
    Q_INVOKABLE bool saveTask(const QString& id,qint64 version,const QString& title,const QString& notes,
                             const QString& planning,const QString& start,const QString& deadline,bool focus);
    Q_INVOKABLE bool saveTaskWithDetails(const QString& id,qint64 version,const QString& title,const QString& notes,
                                        const QString& planning,const QString& start,const QString& deadline,bool focus,const QVariantMap& details);
    Q_INVOKABLE bool changeState(const QString& id,qint64 version,const QString& action);
    Q_INVOKABLE bool batchTasks(const QVariantList& selection,const QString& action,const QString& value);
    int historyDays()const{return m_historyDays;}
    void setHistoryDays(int days);
    bool calendarCompleted()const{return m_calendarCompleted;}
    void setCalendarCompleted(bool value){m_calendarCompleted=value;emit stateChanged();}
    QVariantList unscheduledTasks()const;
    QVariantMap preferences()const{return m_preferences;}
    QVariantMap systemAccessibility() const;
    QVariantMap backupInfo() const;
    Q_INVOKABLE QVariantMap catalogImpact(const QString& kind,const QString& id) const;
    Q_INVOKABLE int batchImpact(const QVariantList& selection,const QString& action) const;
    Q_INVOKABLE bool setPreference(const QString& key,const QVariant& value);
    Q_INVOKABLE bool attachDesktopCalendarWindow(QWindow* window);
    Q_INVOKABLE bool undoDelete();
    Q_INVOKABLE bool createBackup();
    bool verifyStorage();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool saveCatalog(const QVariantMap& item);
    Q_INVOKABLE bool dropCategory(const QString& id,qint64 version,const QString& targetId,qint64 targetVersion,const QString& placement);
    Q_INVOKABLE bool reparentCategory(const QString& id,qint64 version,const QString& parentId);
    Q_INVOKABLE bool deleteCatalog(const QString& kind,const QString& id,qint64 version);
    Q_INVOKABLE bool canMoveCatalog(const QString& kind,const QString& id,int direction) const;
    Q_INVOKABLE bool moveCatalog(const QString& kind,const QString& id,qint64 version,int direction);
    Q_INVOKABLE bool saveDetails(const QString& id,qint64 version,const QVariantMap& details);
signals:
    void syncStateChanged();
    void syncApplyPausedChanged();
    void syncActionRequested(const QString& action,const QVariantMap& values);
    void stateChanged();
    void interactionActiveChanged();
    void categoryRelocated(const QString& id);
    void catalogSaved(const QString& id,const QString& kind);
    void operationFinished(const QString& operation,bool success);
    void calendarTaskCreated(const QString& id);
    void subtaskCreated(const QString& id);
    void historyChanged();
    void reminderReady(const QString& title,const QString& message);
private:
    QVariantMap m_syncState{{"phase",QStringLiteral("GUEST")},{"status",QStringLiteral("仅存储在此设备")},{"authenticated",false},{"busy",false}};
    bool m_syncApplyPaused=false;
    QVariantList m_taskEditHistory;
    QVariantList m_lastBatchDeleted;
    QVariantMap timelineInZone(const QString& anchor,bool week,bool scoped,const QTimeZone& zone,const QString& omittedTask) const;
    QString m_historyTaskId;
    bool m_historyLoading=false;
    bool appendDetailsPatch(const QString& id,const QVariantMap& details,TaskPatch& patch);
    bool prepareTaskPatch(const QString& id,const QString& title,const QString& notes,const QString& planning,const QString& start,const QString& deadline,bool focus,const QVariantMap& details,TaskPatch& patch);
    bool begin(bool clearFeedback=true);
    QVariantMap eventMap(const CalendarEvent& event) const;
    void refresh();
    QList<Task> calendarSelection() const;
    bool runTask(const QString& operation,std::function<Result<Task>(TaskWorker&)> command);
    void finishTask(const QString& operation,const Result<Task>& result);
    bool runCatalog(std::function<Result<void>(TaskWorker&)> command,const QString& operation=QStringLiteral("catalog"),std::function<void()> completed={});
    bool runTransfer(const QString& operation,std::function<Result<QString>(TaskWorker&)> command);
    QThread m_thread;
    TaskWorker* m_worker=nullptr;
    TaskListModel m_tasks, m_todayTasks,m_deadlineTasks;
    QList<Task> m_cache;
    QHash<QString,Task> m_taskIndex;
    QSet<QString> m_collapsedTasks;
    QList<CalendarEvent> m_events;
    QString m_eventUndoId,m_eventDeleteCandidate;
    qint64 m_eventUndoVersion=0,m_eventDeleteVersion=0;
    QTimer m_eventUndoTimer;
    QList<CatalogItem> m_catalog;
    QList<RecurrenceRule> m_rules;
    QString m_scope=QStringLiteral("inbox"),m_calendarScale=QStringLiteral("month");
    QString m_path,m_view=QStringLiteral("inbox"),m_search,m_lastDeleted,m_info,m_pendingInfo;
    QString m_importSummary;
    QString m_tagFilter;
    QString m_projectFilter;
    Error m_error;
    bool m_ready=false,m_busy=false;
    bool m_interactionActive=false;
    bool m_backgroundMaintenance=false;
    bool m_refreshPending=false;
    QTimer m_pendingMaintenance;
    SystemClock m_clock;
    QDate m_today;
    QDate m_calendarDate=QDate::currentDate();
    bool m_sundayFirst=false;
    int m_themeMode=0;
    double m_textScale=1.0;
    bool m_includeCompleted=false;
    int m_upcomingDays=30;
    int m_historyDays=0;
    bool m_calendarCompleted=false;
    QVariantList m_taskNotifications;
    QVariantMap m_viewCounts;
    QVariantMap m_preferences{{"followSystemAccessibility",true},{"calendarScale","month"},{"desktopCalendarScale","month"},{"listShowSubtasks",true},{"reduceMotion",false},{"highContrast",false},{"compact",true},{"sidebarCollapsed",false},{"sidebarTasksExpanded",true},{"sidebarPlanningExpanded",true},{"sidebarLibraryExpanded",true},{"sidebarTagsExpanded",true},{"solidWidget",false},{"closeNoticeSeen",false},{"backupDays",1},{"systemReminders",true},{"desktopCalendarVisible",false},{"desktopCalendarTopmost",false},{"desktopCalendarPinned",false},{"desktopCalendarOpacity",90}};
    QTimer m_searchTimer,m_dateTimer,m_midnightTimer,m_undoTimer,m_maintenanceTimer;
    bool applyCalendarLayer(bool desktop,bool topmost);
    QPointer<QWindow> m_calendarWindow;
    std::unique_ptr<QtWindowPlatform> m_calendarPlatform;
};

} // namespace daymark
