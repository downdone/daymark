#pragma once
#include "domain/Task.h"
#include <QSet>
#include "persistence/TaskRepository.h"
#include "services/CatalogService.h"

namespace daymark {

class QueryService final {
public:
    static Task withUnifiedDates(const Task& task);
    static QDate plannedDate(const Task& task);
    static bool isOverdue(const Task& task,const QDate& today);
    static int todayPeriod(const Task& task,const QDate& today);
    static bool effectiveActive(const Task& task);
    static bool inView(const Task& task, const QString& view, const QDate& today);
    static QList<Task> select(const QList<Task>& tasks, const QString& view, const QDate& today, const QString& search = {},bool includeCompleted=false,const QString& tagFilter={},int upcomingDays=0,int historyDays=0,const QString& projectFilter={},const QSet<QString>& collapsed={},bool includeSubtasks=true,bool nestChildren=true);
};

class TaskService final {
public:
    TaskService(TaskRepository& repository, const IClock& clock, CatalogService* catalog=nullptr) : m_repository(repository), m_clock(clock),m_catalog(catalog) {}
    Result<Task> create(const QString& title, const QString& view = QStringLiteral("inbox"),const QDate& date={},const std::optional<TaskSchedule>& schedule={});
    static Task draftForScope(const QString& view,const QDate& today,const QDate& date={},const std::optional<TaskSchedule>& schedule={});
    Result<Task> createWithDetails(const QString& title,const TaskPatch& patch);
    Result<Task> createSubtask(const QString& title,const QString& parentId);
    Result<Task> edit(const QString& id, qint64 version, const TaskPatch& patch);
    Result<Task> changeState(const QString& id, qint64 version, const QString& action);
    Result<Task> moveToView(const QString& id,qint64 version,const QString& view,const QDate& start={});
    Result<void> batch(const QList<QPair<QString,qint64>>& selection,const QString& action,const TaskPatch& patch={});
private:
    Result<void> synchronizeTime(Task& task,const Task& before,const TaskPatch& patch) const;
    Result<Task> editOne(const QString& id,qint64 version,const TaskPatch& patch);
    Result<Task> changeStateOne(const QString& id,qint64 version,const QString& action,const QString& group);
    Result<void> validate(Task& task) const;
    TaskRepository& m_repository;
    const IClock& m_clock;
    CatalogService* m_catalog=nullptr;
};

} // namespace daymark
