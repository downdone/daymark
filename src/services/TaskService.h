#pragma once
#include "domain/Task.h"
#include "persistence/TaskRepository.h"
#include "services/CatalogService.h"

namespace daymark {

class QueryService final {
public:
    static bool effectiveActive(const Task& task);
    static bool inView(const Task& task, const QString& view, const QDate& today);
    static QList<Task> select(const QList<Task>& tasks, const QString& view, const QDate& today, const QString& search = {},bool includeCompleted=false,const QString& tagFilter={},int upcomingDays=0);
};

class TaskService final {
public:
    TaskService(TaskRepository& repository, const IClock& clock, CatalogService* catalog=nullptr) : m_repository(repository), m_clock(clock),m_catalog(catalog) {}
    Result<Task> create(const QString& title, const QString& view = QStringLiteral("inbox"));
    Result<Task> edit(const QString& id, qint64 version, const TaskPatch& patch);
    Result<Task> changeState(const QString& id, qint64 version, const QString& action);
private:
    Result<void> validate(Task& task) const;
    TaskRepository& m_repository;
    const IClock& m_clock;
    CatalogService* m_catalog=nullptr;
};

} // namespace daymark
