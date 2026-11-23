#include "TaskCycleRepository.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QUuid>

namespace daymark {
namespace {
const QStringList cycleColumns={"frequency","interval_n","anchor_on","weekdays_mask","month_day","year_month",
                                "until_on","zone","active","current_on","current_start_utc","current_end_utc",
                                "next_on","next_start_utc","rule_revision"};
QVariant dateValue(const QDate& date){return date.isValid()?QVariant(date.toString(Qt::ISODate)):QVariant();}
QVariant instantValue(const QDateTime& date){return date.isValid()?QVariant(date.toUTC().toString(Qt::ISODateWithMs)):QVariant();}
Result<void> writeError(){return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));}
Result<void> conflict(){return Result<void>::failure(ErrorCode::Conflict,QStringLiteral("cycleConflict"));}
}

Result<void> TaskCycleRepository::load(QList<Task>& tasks,const QString& id) const
{
    if(tasks.isEmpty())return {};
    QHash<QString,qsizetype> indices;
    for(qsizetype i=0;i<tasks.size();++i)indices.insert(tasks[i].id,i);
    QSqlQuery q(m_db);
    q.prepare("SELECT task_id,"+cycleColumns.join(',')+",row_version,(SELECT h.performed_at_utc FROM task_cycle_completions h "
              "WHERE h.task_id=task_cycles.task_id AND h.outcome='COMPLETED' AND h.revoked_at_utc IS NULL "
              "ORDER BY h.performed_at_utc DESC LIMIT 1) FROM task_cycles"+(id.isEmpty()?QString():QStringLiteral(" WHERE task_id=?")));
    if(!id.isEmpty())q.addBindValue(id);
    if(!q.exec())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    while(q.next()){
        const auto index=indices.constFind(q.value(0).toString());if(index==indices.cend())continue;
        TaskCycle cycle;
        cycle.rule.frequency=q.value(1).toString();cycle.rule.interval=q.value(2).toInt();
        cycle.rule.anchor=QDate::fromString(q.value(3).toString(),Qt::ISODate);cycle.rule.weekdays=q.value(4).toInt();
        cycle.rule.monthDay=q.value(5).toInt();cycle.rule.yearMonth=q.value(6).toInt();
        cycle.rule.end=QDate::fromString(q.value(7).toString(),Qt::ISODate);cycle.zone=q.value(8).toString();
        cycle.rule.active=q.value(9).toBool();cycle.currentDate=QDate::fromString(q.value(10).toString(),Qt::ISODate);
        cycle.currentStartUtc=QDateTime::fromString(q.value(11).toString(),Qt::ISODateWithMs);
        cycle.currentEndUtc=QDateTime::fromString(q.value(12).toString(),Qt::ISODateWithMs);
        cycle.nextDate=QDate::fromString(q.value(13).toString(),Qt::ISODate);
        cycle.nextStartUtc=QDateTime::fromString(q.value(14).toString(),Qt::ISODateWithMs);
        cycle.ruleRevision=q.value(15).toLongLong();cycle.rowVersion=q.value(16).toLongLong();
        cycle.latestCompletion=q.value(17).toString();
        tasks[*index].cycle=std::move(cycle);
    }
    return {};
}

Result<bool> TaskCycleRepository::hasAny() const
{
    QSqlQuery q(m_db);
    if(!q.exec("SELECT 1 FROM task_cycles LIMIT 1"))return Result<bool>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    return {q.next(),{}};
}

Result<QStringList> TaskCycleRepository::due(const QDateTime& now,int limit) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT c.task_id FROM task_cycles c JOIN tasks t ON t.id=c.task_id WHERE c.active=1 AND c.next_start_utc<=? AND t.status='COMPLETED' AND t.deleted_at_utc IS NULL AND t.purged_at_utc IS NULL AND t.local_removed_at_utc IS NULL ORDER BY c.next_start_utc,c.task_id LIMIT ?");
    q.addBindValue(instantValue(now));q.addBindValue(limit);
    if(!q.exec())return Result<QStringList>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QStringList rows;while(q.next())rows.append(q.value(0).toString());return {rows,{}};
}

Result<void> TaskCycleRepository::save(const QString& taskId,TaskCycle cycle,qint64 expected)
{
    QSqlQuery q(m_db);
    if(expected==0){
        QStringList placeholders;for(qsizetype i=0;i<cycleColumns.size();++i)placeholders.append(QStringLiteral("?"));
        q.prepare("INSERT INTO task_cycles("+cycleColumns.join(',')+",task_id,row_version) VALUES("+placeholders.join(',')+",?,1) ON CONFLICT(task_id) DO NOTHING");
    }else{
        QStringList assignments;for(const auto& column:cycleColumns)assignments.append(column+"=?");
        q.prepare("UPDATE task_cycles SET "+assignments.join(',')+",row_version=row_version+1 WHERE task_id=? AND row_version=?");
    }
    for(const auto& value:QList<QVariant>{cycle.rule.frequency,cycle.rule.interval,dateValue(cycle.rule.anchor),cycle.rule.weekdays,
            cycle.rule.monthDay,cycle.rule.yearMonth,dateValue(cycle.rule.end),cycle.zone,cycle.rule.active,dateValue(cycle.currentDate),
            instantValue(cycle.currentStartUtc),instantValue(cycle.currentEndUtc),dateValue(cycle.nextDate),instantValue(cycle.nextStartUtc),cycle.ruleRevision,taskId})q.addBindValue(value);
    if(expected!=0)q.addBindValue(expected);
    if(!q.exec())return writeError();
    return q.numRowsAffected()==1?Result<void>{}:conflict();
}

Result<void> TaskCycleRepository::record(const Task& before,const TaskCycle& cycle,const QString& occurrence,
                                         const QString& outcome,const QDateTime& now,const QString& group,const QStringList& members)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO task_cycle_completions(task_id,occurrence_key,rule_revision,scheduled_start_utc,scheduled_end_utc,outcome,performed_at_utc,mutation_id,completion_group_id,members_json,prior_reminder_utc,prior_reminder_fired_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) "
              "ON CONFLICT(task_id,occurrence_key) DO UPDATE SET outcome=excluded.outcome,performed_at_utc=excluded.performed_at_utc,mutation_id=excluded.mutation_id,completion_group_id=excluded.completion_group_id,members_json=excluded.members_json,prior_reminder_utc=excluded.prior_reminder_utc,prior_reminder_fired_utc=excluded.prior_reminder_fired_utc,revoked_at_utc=NULL WHERE revoked_at_utc IS NOT NULL");
    const auto optional=[](const QString& value){return value.isEmpty()?QVariant():QVariant(value);};
    for(const auto& value:QList<QVariant>{before.id,occurrence,cycle.ruleRevision,instantValue(cycle.currentStartUtc),instantValue(cycle.currentEndUtc),outcome,instantValue(now),
            QUuid::createUuid().toString(QUuid::WithoutBraces),group.isEmpty()?QVariant():QVariant(group),
            QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(members)).toJson(QJsonDocument::Compact)),
            optional(before.remindAtUtc),optional(before.reminderFiredAtUtc)})q.addBindValue(value);
    if(!q.exec())return writeError();
    return q.numRowsAffected()==1?Result<void>{}:conflict();
}

Result<QPair<QString,QString>> TaskCycleRepository::revoke(const QString& taskId,const QString& occurrence,const QDateTime& now)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE task_cycle_completions SET revoked_at_utc=? WHERE task_id=? AND occurrence_key=? AND revoked_at_utc IS NULL RETURNING prior_reminder_utc,prior_reminder_fired_utc");
    q.addBindValue(instantValue(now));q.addBindValue(taskId);q.addBindValue(occurrence);
    if(!q.exec())return {{},writeError().error};
    if(!q.next())return {{},conflict().error};
    return {{q.value(0).toString(),q.value(1).toString()},{}};
}

Result<QVariantList> TaskCycleRepository::history(const QString& taskId,int offset) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT occurrence_key,scheduled_start_utc,scheduled_end_utc,outcome,performed_at_utc,revoked_at_utc FROM task_cycle_completions WHERE task_id=? ORDER BY performed_at_utc DESC,occurrence_key LIMIT 50 OFFSET ?");
    q.addBindValue(taskId);q.addBindValue(qMax(0,offset));
    if(!q.exec())return Result<QVariantList>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    QVariantList rows;while(q.next())rows.append(QVariantMap{{"occurrenceKey",q.value(0)},{"startAt",q.value(1)},
        {"endAt",q.value(2)},{"outcome",q.value(3)},{"performedAt",q.value(4)},{"revokedAt",q.value(5)}});
    return {rows,{}};
}

Result<void> TaskCycleRepository::detachLegacy(const Task& task,qint64 ruleVersion,const QDateTime& now)
{
    if(task.recurrenceId.isEmpty() || !task.repeat || task.repeat->rowVersion!=ruleVersion)return conflict();
    QSqlQuery q(m_db);
    q.prepare("UPDATE recurrence_templates SET active=0,updated_at_utc=?,row_version=row_version+1 WHERE id=? AND row_version=? AND deleted_at_utc IS NULL");
    q.addBindValue(instantValue(now));q.addBindValue(task.recurrenceId);q.addBindValue(ruleVersion);
    if(!q.exec())return writeError();if(q.numRowsAffected()!=1)return conflict();
    q.prepare("UPDATE tasks SET recurrence_template_id=NULL,occurrence_on=NULL,sync_occurrence_on=NULL,sync_occurrence_key=NULL,instance_overridden=0 WHERE id=? AND row_version=? AND recurrence_template_id=?");
    q.addBindValue(task.id);q.addBindValue(task.rowVersion);q.addBindValue(task.recurrenceId);
    if(!q.exec())return writeError();return q.numRowsAffected()==1?Result<void>{}:conflict();
}
}
