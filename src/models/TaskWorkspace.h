#pragma once
#include "domain/Task.h"
#include <QAbstractListModel>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <functional>

namespace daymark {

class TaskListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by TaskWorkspace")
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role { Id=Qt::UserRole+1, Title, Notes, Done, Status, StartOn, DeadlineOn, Focus, Version, Overdue, Evening, ProjectName, TagSummary, Section, HeadingTitle, ChecklistPending };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex& parent={}) const override { return parent.isValid()?0:m_tasks.size(); }
    QVariant data(const QModelIndex& index,int role) const override;
    QHash<int,QByteArray> roleNames() const override;
    void replace(const QList<Task>& tasks,const QDate& today);
    static QVariantMap taskMap(const Task& task);
signals:
    void countChanged();
private:
    QList<Task> m_tasks;
    QDate m_today;
};

class TaskWorker;
class TaskWorkspace : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppRuntime")
    Q_PROPERTY(daymark::TaskListModel* tasks READ tasks CONSTANT)
    Q_PROPERTY(daymark::TaskListModel* todayTasks READ todayTasks CONSTANT)
    Q_PROPERTY(daymark::TaskListModel* deadlineTasks READ deadlineTasks CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString view READ view NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY stateChanged)
    Q_PROPERTY(QVariantList catalog READ catalog NOTIFY stateChanged)
    Q_PROPERTY(QVariantList calendarDays READ calendarDays NOTIFY stateChanged)
    Q_PROPERTY(QString calendarTitle READ calendarTitle NOTIFY stateChanged)
    Q_PROPERTY(bool sundayFirst READ sundayFirst WRITE setSundayFirst NOTIFY stateChanged)
    Q_PROPERTY(QVariantList recurrenceRules READ recurrenceRules NOTIFY stateChanged)
    Q_PROPERTY(QVariantList notifications READ notifications NOTIFY stateChanged)
    Q_PROPERTY(QString importSummary READ importSummary NOTIFY stateChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY stateChanged)
    Q_PROPERTY(double textScale READ textScale WRITE setTextScale NOTIFY stateChanged)
    Q_PROPERTY(bool includeCompleted READ includeCompleted WRITE setIncludeCompleted NOTIFY stateChanged)
    Q_PROPERTY(int completedToday READ completedToday NOTIFY stateChanged)
    Q_PROPERTY(QString localToday READ localToday NOTIFY stateChanged)
    Q_PROPERTY(int upcomingDays READ upcomingDays NOTIFY stateChanged)
public:
    explicit TaskWorkspace(QObject* parent=nullptr);
    ~TaskWorkspace() override;
    void start(const QString& databasePath);
    TaskListModel* tasks() { return &m_tasks; }
    TaskListModel* todayTasks() { return &m_todayTasks; }
    TaskListModel* deadlineTasks() {return &m_deadlineTasks;}
    bool ready() const { return m_ready; }
    bool busy() const { return m_busy; }
    QString view() const { return m_view; }
    QString errorText() const;
    QString infoText() const { return m_info; }
    bool canUndo() const { return !m_lastDeleted.isEmpty(); }
    QVariantList catalog() const;
    QVariantList calendarDays() const;
    QString calendarTitle() const;
    bool sundayFirst() const {return m_sundayFirst;}
    void setSundayFirst(bool enabled);
    Q_INVOKABLE void stepCalendar(int offset);
    Q_INVOKABLE bool createForDate(const QString& title,const QString& date);
    Q_INVOKABLE bool moveCalendar(const QString& id,qint64 version,const QString& kind,const QString& date);
    QVariantList recurrenceRules() const;
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
    Q_INVOKABLE bool dropTask(const QString& source,const QString& target);
    int themeMode() const {return m_themeMode;}
    double textScale() const {return m_textScale;}
    void setThemeMode(int mode);
    void setTextScale(double scale);
    bool includeCompleted() const {return m_includeCompleted;}
    void setIncludeCompleted(bool enabled) {m_includeCompleted=enabled;refresh();emit stateChanged();}
    int completedToday() const;
    QString localToday() const {return m_today.toString(QStringLiteral("yyyy年M月d日"));}
    Q_INVOKABLE QString dateOffset(int days) const {return m_today.addDays(days).toString(Qt::ISODate);}
    Q_INVOKABLE QVariantMap datePicker(const QString& anchor,int offset) const;
    Q_INVOKABLE void setTagFilter(const QString& id) {m_tagFilter=id;refresh();emit stateChanged();}
    int upcomingDays()const{return m_upcomingDays;}
    Q_INVOKABLE void loadMoreUpcoming(){m_upcomingDays=qMin(36500,m_upcomingDays+30);refresh();emit stateChanged();}
    Q_INVOKABLE bool todayAfterRemovingFocus(const QString& id) const;
    void settingsFailed() {m_error={ErrorCode::PersistenceError,QStringLiteral("settingsWrite")};emit stateChanged();}
    Q_INVOKABLE QVariantList catalogOf(const QString& kind,const QString& parent=QString()) const;

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setView(const QString& view);
    Q_INVOKABLE void setSearch(const QString& search);
    Q_INVOKABLE QVariantMap task(const QString& id) const;
    Q_INVOKABLE bool createTask(const QString& title);
    Q_INVOKABLE bool saveTask(const QString& id,qint64 version,const QString& title,const QString& notes,
                             const QString& planning,const QString& start,const QString& deadline,bool focus);
    Q_INVOKABLE bool changeState(const QString& id,qint64 version,const QString& action);
    Q_INVOKABLE bool undoDelete();
    Q_INVOKABLE bool createBackup();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool saveCatalog(const QVariantMap& item);
    Q_INVOKABLE bool deleteCatalog(const QString& kind,const QString& id,qint64 version);
    Q_INVOKABLE bool saveDetails(const QString& id,qint64 version,const QVariantMap& details);
signals:
    void stateChanged();
    void operationFinished(const QString& operation,bool success);
    void reminderReady(const QString& title,const QString& message);
private:
    bool begin();
    void refresh();
    bool runTask(const QString& operation,std::function<Result<Task>(TaskWorker&)> command);
    void finishTask(const QString& operation,const Result<Task>& result);
    bool runCatalog(std::function<Result<void>(TaskWorker&)> command,const QString& operation=QStringLiteral("catalog"));
    bool runTransfer(const QString& operation,std::function<Result<QString>(TaskWorker&)> command);
    QThread m_thread;
    TaskWorker* m_worker=nullptr;
    TaskListModel m_tasks, m_todayTasks,m_deadlineTasks;
    QList<Task> m_cache;
    QList<CatalogItem> m_catalog;
    QList<RecurrenceRule> m_rules;
    QString m_path,m_view=QStringLiteral("inbox"),m_search,m_lastDeleted,m_info,m_pendingInfo;
    QString m_importSummary;
    QString m_tagFilter;
    Error m_error;
    bool m_ready=false,m_busy=false;
    SystemClock m_clock;
    QDate m_today;
    QDate m_calendarDate=QDate::currentDate();
    bool m_sundayFirst=false;
    int m_themeMode=0;
    double m_textScale=1.0;
    bool m_includeCompleted=false;
    int m_upcomingDays=30;
    QTimer m_searchTimer,m_dateTimer,m_midnightTimer,m_undoTimer,m_maintenanceTimer;
};

} // namespace daymark
