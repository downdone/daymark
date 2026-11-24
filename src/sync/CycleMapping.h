#pragma once

#include "Protocol.h"
#include "services/TaskCycleService.h"
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace daymark::sync {
class CycleMapping final {
  public:
    explicit CycleMapping(QSqlDatabase db) : db(db) {}

    Result<QHash<QString,QJsonArray>> history(const QString &task = {}) const {
        QSqlQuery q(db);
        q.prepare("SELECT task_id,rule_revision,scheduled_start_utc,scheduled_end_utc,outcome,"
                  "performed_at_utc,mutation_id,completion_group_id,members_json,revoked_at_utc,"
                  "prior_reminder_utc FROM task_cycle_completions" +
                  (task.isEmpty() ? QString() : QStringLiteral(" WHERE task_id=?")));
        if (!task.isEmpty())
            q.addBindValue(task);
        if (!q.exec())
            return {{}, {ErrorCode::PersistenceError, QStringLiteral("databaseRead")}};
        QHash<QString,QMap<QString,QJsonObject>> rows;
        while (q.next()) {
            const auto version = q.value(1).toLongLong();
            const auto start = q.value(2).toString();
            const auto key = TaskCycleService::occurrenceKey(version, start);
            auto ids = QJsonDocument::fromJson(q.value(8).toByteArray()).array().toVariantList();
            QStringList members;
            for (const auto &id : ids)
                members.append(id.toString());
            members.sort();
            auto &group = rows[q.value(0).toString()];
            if (group.contains(key))
                return {{}, failure(QStringLiteral("cycleConflict"))};
            group.insert(key, {{"occurrence_key", key}, {"rule_revision", QString::number(version)},
                               {"scheduled_start_utc", start},
                               {"scheduled_end_utc", nullable(q.value(3).toString())},
                               {"outcome", q.value(4).toString()},
                               {"performed_at_utc", q.value(5).toString()},
                               {"mutation_id", q.value(6).toString()},
                               {"completion_group_id", nullable(q.value(7).toString())},
                               {"members", QJsonArray::fromStringList(members)},
                               {"revoked_at_utc", nullable(q.value(9).toString())},
                               {"prior_reminder_utc", nullable(q.value(10).toString())}});
        }
        QHash<QString,QJsonArray> result;
        for (auto it = rows.cbegin(); it != rows.cend(); ++it) {
            QJsonArray history;
            for (const auto &entry : it.value())
                history.append(entry);
            result.insert(it.key(), history);
        }
        return {result, {}};
    }

    static QJsonObject encode(const TaskCycle &c, const QJsonArray &history) {
        auto date = [](const QDate &value) { return nullable(value.toString(Qt::ISODate)); };
        auto instant = [](const QDateTime &value) { return nullable(value.toUTC().toString(Qt::ISODateWithMs)); };
        return {{"frequency", c.rule.frequency}, {"interval_n", c.rule.interval},
                {"anchor_on", date(c.rule.anchor)}, {"weekdays_mask", c.rule.weekdays},
                {"month_day", c.rule.monthDay}, {"year_month", c.rule.yearMonth},
                {"until_on", date(c.rule.end)}, {"zone", c.zone}, {"active", c.rule.active},
                {"current_on", date(c.currentDate)}, {"current_start_utc", instant(c.currentStartUtc)},
                {"current_end_utc", instant(c.currentEndUtc)}, {"next_on", date(c.nextDate)},
                {"next_start_utc", instant(c.nextStartUtc)},
                {"rule_revision", QString::number(c.ruleRevision)}, {"completions", history}};
    }

    Result<void> apply(const QString &id, const QJsonObject &value,
                       const std::optional<TaskCycle> &previous) const {
        if (!previous && value.isEmpty())
            return {};
        QSqlQuery q(db);
        if (value.isEmpty()) {
            q.prepare("DELETE FROM task_cycles WHERE task_id=?");
            q.addBindValue(id);
            return q.exec() ? Result<void>{} : writeError();
        }
        const auto existing = history(id);
        if (!existing.ok())
            return {existing.error};
        if (previous && value == encode(*previous, existing.value.value(id)))
            return {};

        auto date = [&](const QString &field) {
            return QDate::fromString(value.value(field).toString(), Qt::ISODate);
        };
        auto instant = [&](const QString &field) {
            return QDateTime::fromString(value.value(field).toString(), Qt::ISODateWithMs);
        };
        TaskCycle c;
        c.rule.frequency = value.value("frequency").toString();
        c.rule.interval = value.value("interval_n").toInt();
        c.rule.anchor = date("anchor_on");
        c.rule.weekdays = value.value("weekdays_mask").toInt();
        c.rule.monthDay = value.value("month_day").toInt();
        c.rule.yearMonth = value.value("year_month").toInt();
        c.rule.end = date("until_on");
        c.zone = value.value("zone").toString();
        c.rule.active = value.value("active").toBool();
        c.currentDate = date("current_on");
        c.currentStartUtc = instant("current_start_utc");
        c.currentEndUtc = instant("current_end_utc");
        c.nextDate = date("next_on");
        c.nextStartUtc = instant("next_start_utc");
        c.ruleRevision = value.value("rule_revision").toString().toLongLong();
        if (!c.currentStartUtc.isValid() || !QTimeZone(c.zone.toUtf8()).isValid() ||
            (value.value("next_start_utc").isString() && !c.nextStartUtc.isValid()))
            return {failure(QStringLiteral("syncProtocolMismatch"))};
        const auto saved = TaskCycleRepository(db).save(id, c, previous ? previous->rowVersion : 0);
        if (!saved.ok())
            return saved;

        QHash<QString,QVariant> firedByMutation;
        q.prepare("SELECT mutation_id,prior_reminder_fired_utc FROM task_cycle_completions WHERE task_id=?");
        q.addBindValue(id);
        if (!q.exec())
            return writeError();
        while (q.next())
            firedByMutation.insert(q.value(0).toString(), q.value(1));
        q.prepare("DELETE FROM task_cycle_completions WHERE task_id=?");
        q.addBindValue(id);
        if (!q.exec())
            return writeError();
        q.prepare("INSERT INTO task_cycle_completions(task_id,occurrence_key,rule_revision,"
                  "scheduled_start_utc,scheduled_end_utc,outcome,performed_at_utc,mutation_id,"
                  "completion_group_id,members_json,revoked_at_utc,prior_reminder_utc,"
                  "prior_reminder_fired_utc) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)");
        for (const auto &entry : value.value("completions").toArray()) {
            const auto row = entry.toObject();
            int index = 0;
            q.bindValue(index++, id);
            for (const auto field : {"occurrence_key", "rule_revision", "scheduled_start_utc",
                                     "scheduled_end_utc", "outcome", "performed_at_utc", "mutation_id",
                                     "completion_group_id"})
                q.bindValue(index++, row.value(QLatin1String(field)).toVariant());
            const auto members = QJsonDocument(row.value("members").toArray()).toJson(QJsonDocument::Compact);
            q.bindValue(index++, QString::fromUtf8(members));
            q.bindValue(index++, row.value("revoked_at_utc").toVariant());
            q.bindValue(index++, row.value("prior_reminder_utc").toVariant());
            q.bindValue(index, firedByMutation.value(row.value("mutation_id").toString()));
            if (!q.exec())
                return writeError();
        }
        return {};
    }

  private:
    QSqlDatabase db;
    static Result<void> writeError() {
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("syncDatabaseWrite"));
    }
};
} // namespace daymark::sync
