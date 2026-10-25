#pragma once
#include "domain/Task.h"
#include <QSqlDatabase>
#include <QVariantList>
#include <QJsonObject>
#include <functional>

namespace daymark {

class TaskRepository final {
public:
    explicit TaskRepository(QSqlDatabase db) : m_db(std::move(db)) {}
    Result<QList<Task>> all() const;
    Result<Task> find(const QString& id) const;
    Result<QVariantList> reminderHistory() const;
    Result<QVariantList> editHistory(const QString& id) const;
    Result<Task> insert(Task task);
    Result<Task> update(Task task, qint64 expectedVersion,bool recordEdit=true);
    Result<void> writeComponents(const Task& task);
    static QJsonObject seedFor(const Task& task);
    static Result<Task> fromSeed(const RecurrenceRule& rule,const QDate& date);
    Result<void> atomically(const std::function<Result<void>()>& command);
    Result<QList<Task>> claimDueReminders(const QString& now);
    Result<void> reorder(const QList<Task>& tasks,bool today,const QString& now,const OrderChange& change={});
    Result<bool> hasOpenCompletionSibling(const Task& task) const;
private:
    Result<void> writeRepeat(const Task& task);
    QSqlDatabase m_db;
};

} // namespace daymark
