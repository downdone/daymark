#pragma once

#include "domain/Task.h"
#include <QSqlDatabase>
#include <QVariantList>

namespace daymark {
class TaskCycleRepository final {
public:
    explicit TaskCycleRepository(QSqlDatabase db):m_db(std::move(db)){}
    Result<void> load(QList<Task>& tasks,const QString& id={}) const;
    Result<QStringList> due(const QDateTime& now,int limit) const;
    Result<bool> hasAny() const;
    Result<void> save(const QString& taskId,TaskCycle cycle,qint64 expected);
    Result<void> record(const Task& before,const TaskCycle& cycle,const QString& occurrence,
                        const QString& outcome,const QDateTime& now,const QString& group,const QStringList& members);
    Result<QPair<QString,QString>> revoke(const QString& taskId,const QString& occurrence,const QDateTime& now);
    Result<QVariantList> history(const QString& taskId,int offset) const;
    Result<void> detachLegacy(const Task& task,qint64 ruleVersion,const QDateTime& now);

private:
    QSqlDatabase m_db;
};
}
