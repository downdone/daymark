#pragma once
#include "domain/Task.h"
#include <QSqlDatabase>

namespace daymark {

class TaskRepository final {
public:
    explicit TaskRepository(QSqlDatabase db) : m_db(std::move(db)) {}
    Result<QList<Task>> all() const;
    Result<Task> find(const QString& id) const;
    Result<Task> insert(Task task);
    Result<Task> update(Task task, qint64 expectedVersion);
    Result<QList<Task>> claimDueReminders(const QString& now);
    Result<void> reorder(const QList<Task>& tasks,bool today,const QString& now,const OrderChange& change={});
    Result<bool> hasOpenCompletionSibling(const Task& task) const;
private:
    QSqlDatabase m_db;
};

} // namespace daymark
