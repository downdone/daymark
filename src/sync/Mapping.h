#pragma once
#include "Schema.h"
#include "Identity.h"
#include "persistence/TaskRepository.h"
#include "services/ReminderSchedule.h"
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTimeZone>
#include <QMap>

namespace daymark::sync {
inline QJsonObject entity(const QString &type, const QJsonObject &value) {
    return {{"type", type},
            {"id", value.value("id")},
            {"revision", QStringLiteral("1")},
            {"operation", QStringLiteral("replace")},
            {"value", value}};
}
inline QJsonObject legacy(const QJsonObject &v) {
    return {{"format_version", 1},
            {"source_export_version", 10},
            {"canonical_json", QString::fromUtf8(json(v))}};
}
inline QJsonObject capsule(const QJsonValue &v) {
    return QJsonDocument::fromJson(v.toObject().value("canonical_json").toString().toUtf8())
        .object();
}
inline QJsonObject event(const QString &type, const QString &at) {
    return {{"type", type}, {"event_at_utc", at}};
}
inline QJsonObject patchFields(const QJsonObject &value) {
    QJsonObject r;
    for (const auto &name : {"title", "notes", "planning_state", "category_id", "parent_task_id",
                             "start_on", "deadline_on", "today_focus", "evening", "pinned",
                             "important", "schedule", "reminder", "tag_ids", "links"})
        r.insert(QLatin1String(name), value.value(QLatin1String(name)));
    return r;
}

// Persistence adapter: only this layer translates the delivered desktop schema
// to protocol facts. Device delivery/history/preferences never enter the graph.
class Mapping final {
  public:
    explicit Mapping(QSqlDatabase db, QJsonArray overlay = {})
        : db(db), overlay(std::move(overlay)) {}
    Result<Graph> capture() const {
        Graph graph;
        const auto identity = Identity::load(db, overlay);
        if (!identity.ok())
            return {{}, identity.error};
        const auto taskResult = TaskRepository(db).all();
        if (!taskResult.ok())
            return {{}, taskResult.error};
        QHash<QString, Task> tasks;
        for (const auto &t : taskResult.value)
            tasks.insert(t.id, t);
        QHash<QString, QJsonObject> rawTasks;
        auto rows = read(QStringLiteral("tasks"));
        if (!rows.ok())
            return {{}, rows.error};
        for (const auto &r : rows.value)
            rawTasks.insert(r.value("id").toString(), r);
        auto add = [&](const QString &type, const QJsonObject &value, qint64 version) {
            auto e = entity(type, value);
            const auto n = normalize(e);
            if (n.ok())
                e = n.value.toObject();
            graph.entities.insert(key(e), e);
            graph.localVersions.insert(key(e), qMax<qint64>(1, version));
        };
        for (const auto &t : taskResult.value) {
            const auto raw = rawTasks.value(t.id);
            auto v = taskValue(t, raw);
            if (!t.recurrenceId.isEmpty()) {
                QString occurrence = raw.value("sync_occurrence_key").toString();
                if (occurrence.isEmpty()) {
                    if (t.repeat && t.repeat->frequency == "AFTER_DAYS") {
                        if (t.repeat->sourceTaskId == t.id || t.occurrenceOn == t.repeat->anchor)
                            occurrence =
                                QStringLiteral("D:") + t.occurrenceOn.toString(Qt::ISODate);
                        else {
                            QString predecessor;
                            QDateTime last;
                            bool ambiguous = false;
                            const auto created =
                                QDateTime::fromString(t.createdAtUtc, Qt::ISODateWithMs);
                            for (const auto &other : taskResult.value)
                                if (other.recurrenceId == t.recurrenceId && other.id != t.id &&
                                    other.status == "COMPLETED") {
                                    const auto completed = QDateTime::fromString(
                                        other.completedAtUtc, Qt::ISODateWithMs);
                                    if (completed.isValid() && completed <= created) {
                                        if (!last.isValid() || completed > last) {
                                            last = completed;
                                            predecessor = other.id;
                                            ambiguous = false;
                                        } else if (completed == last)
                                            ambiguous = true;
                                    }
                                }
                            if (predecessor.isEmpty() || ambiguous)
                                return {{},
                                        failure(QStringLiteral("syncLegacyRecurrenceUnsupported"))};
                            occurrence = QStringLiteral("A:") + predecessor;
                        }
                    } else
                        occurrence = QStringLiteral("D:") + t.occurrenceOn.toString(Qt::ISODate);
                }
                v.insert("occurrence_key", occurrence);
                QString actual = raw.value("sync_occurrence_on").toString();
                if (actual.isEmpty()) {
                    if (occurrence.startsWith("A:") && t.repeat) {
                        const auto predecessor = tasks.value(occurrence.mid(2));
                        const auto completed =
                            QDateTime::fromString(predecessor.completedAtUtc, Qt::ISODateWithMs);
                        const auto defaults =
                            QJsonDocument::fromJson(t.repeat->defaultsJson.toUtf8()).object();
                        const auto zone =
                            defaults.value("recurrence_time_zone")
                                .toString(QString::fromUtf8(QTimeZone::systemTimeZoneId()));
                        if (!completed.isValid())
                            return {{}, failure(QStringLiteral("syncLegacyRecurrenceUnsupported"))};
                        actual = completed.toTimeZone(QTimeZone(zone.toUtf8()))
                                     .date()
                                     .addDays(t.repeat->interval)
                                     .toString(Qt::ISODate);
                    } else
                        actual = t.occurrenceOn.toString(Qt::ISODate);
                }
                v.insert("occurrence_on", actual);

                const auto expected = stableId(t.recurrenceId + QLatin1Char(':') + occurrence);
                if (t.id != expected && (!t.repeat || t.repeat->sourceTaskId != t.id))
                    graph.aliases.append(
                        QJsonObject{{"alias_id", expected}, {"canonical_id", t.id}});
                if (occurrence.startsWith("A:"))
                    graph.edges.append(QJsonObject{{"rule_id", t.recurrenceId},
                                                   {"predecessor_id", occurrence.mid(2)},
                                                   {"successor_id", t.id}});
            }
            add("task", v, t.rowVersion);
        }
        auto areas = read("areas"), projects = read("projects"), tags = read("tags"),
             rules = read("recurrence_templates"), headings = read("headings");
        for (const auto *r : {&areas, &projects, &tags, &rules, &headings})
            if (!r->ok())
                return {{}, r->error};
        for (const auto &kind : {QStringLiteral("area"), QStringLiteral("project")})
            for (const auto &row : kind == "area" ? areas.value : projects.value) {
                const bool area = kind == "area";
                QJsonObject l{{"kind", kind}};
                if (!area) {
                    for (const auto &name : {"planning_state", "status", "start_on", "deadline_on",
                                             "completed_at_utc"})
                        l.insert(QLatin1String(name), row.value(QLatin1String(name)));
                    QJsonArray hs;
                    for (const auto &h : headings.value)
                        if (h.value("project_id") == row.value("id"))
                            hs.append(h);
                    if (!hs.isEmpty())
                        l.insert("headings", hs);
                }
                const bool historical =
                    area || l.contains("headings") ||
                    (!area &&
                     (row.value("planning_state") != "ACTIVE" || row.value("status") != "OPEN" ||
                      !row.value("start_on").isNull() || !row.value("deadline_on").isNull()));
                QJsonValue metadata =
                    historical ? QJsonValue(legacy(l)) : QJsonValue(QJsonValue::Null);
                if (!area && row.value("sync_legacy_json").isString())
                    metadata =
                        QJsonDocument::fromJson(row.value("sync_legacy_json").toString().toUtf8())
                            .object();
                QJsonObject v{{"id", row.value("id")},
                              {"parent_id",
                               area ? QJsonValue(QJsonValue::Null)
                                    : (row.value("parent_id").isNull() ? row.value("area_id")
                                                                       : row.value("parent_id"))},
                              {"title", row.value(area ? "name" : "title")},
                              {"description",
                               area ? QJsonValue(QStringLiteral("")) : row.value("description")},
                              {"color_key", row.value("color_key")},
                              {"archived", row.value("archived").toBool()},
                              {"created_at_utc", row.value("created_at_utc")},
                              {"updated_at_utc", row.value("updated_at_utc")},
                              {"deleted_at_utc", row.value("deleted_at_utc")},
                              {"legacy_metadata", metadata}};
                add("category", v, row.value("row_version").toInteger(1));
            }
        for (const auto &r : tags.value) {
            QJsonObject v{{"id", r.value("id")},
                          {"title", r.value("name")},
                          {"color_key", r.value("color_key")},
                          {"created_at_utc", r.value("created_at_utc")},
                          {"updated_at_utc", r.value("updated_at_utc")},
                          {"deleted_at_utc", r.value("deleted_at_utc")}};
            add("tag", v, r.value("row_version").toInteger(1));
        }
        for (const auto &r : rules.value) {
            const auto defaults =
                QJsonDocument::fromJson(r.value("task_defaults_json").toString().toUtf8()).object();
            const auto anchor = QDate::fromString(r.value("anchor_on").toString(), Qt::ISODate);
            QJsonObject spec{
                {"mode", r.value("mode")},
                {"frequency", r.value("frequency")},
                {"interval", r.value("interval_n")},
                {"anchor_on", r.value("anchor_on")},
                {"end_on", r.value("end_on")},
                {"weekdays_mask",
                 r.value("frequency") == "WEEKLY" ? r.value("weekdays_mask") : QJsonValue(0)},
                {"month_day", r.value("month_day").isNull() ? QJsonValue(1) : r.value("month_day")},
                {"year_month",
                 r.value("year_month").isNull() ? QJsonValue(1) : r.value("year_month")},
                {"time_zone", defaults.value("recurrence_time_zone")
                                  .toString(QString::fromUtf8(QTimeZone::systemTimeZoneId()))},
                {"defaults", templateValue(r, defaults, anchor)}};
            QJsonObject v{
                {"id", r.value("id")},
                {"source_task_id", r.value("source_task_id")},
                {"rule", spec},
                {"active", r.value("active").toBool() && r.value("deleted_at_utc").isNull()},
                {"created_at_utc", r.value("created_at_utc")},
                {"updated_at_utc", r.value("updated_at_utc")},
                {"deleted_at_utc", r.value("deleted_at_utc")}};
            graph.ruleCutoffs.insert(
                r.value("id").toString(),
                defaults.value("materialize_from").toString(r.value("anchor_on").toString()));
            add("recurrence_rule", v, r.value("row_version").toInteger(1));
        }
        // Sort scopes describe actual persisted ordering, not a query's filter.
        QMap<QString, QList<QPair<qint64, QString>>> scopes;
        QHash<QString, QPair<QString, QString>> scopeInfo;
        auto member = [&](const QString &kind, const QString &subject, qint64 order,
                          const QString &id) {
            const auto scope = orderingId(kind, subject);
            scopes[scope].append({order, id});
            scopeInfo.insert(scope, {kind, subject});
        };
        for (const auto &t : taskResult.value) {
            member("task_siblings", t.parentTaskId, t.sortOrder, t.id);
            if (t.todayOrder >= 0)
                member("today", {}, t.todayOrder, t.id);
        }
        for (const auto &r : areas.value)
            member("category_siblings", {}, r.value("sort_order").toInteger(),
                   r.value("id").toString());
        for (const auto &r : projects.value)
            member("category_siblings",
                   r.value("parent_id").isNull() ? r.value("area_id").toString()
                                                 : r.value("parent_id").toString(),
                   r.value("sort_order").toInteger(), r.value("id").toString());
        for (auto it = scopes.begin(); it != scopes.end(); ++it) {
            auto members = it.value();
            std::sort(members.begin(), members.end());
            QJsonArray ids;
            for (const auto &m : members)
                ids.append(m.second);
            const auto info = scopeInfo.value(it.key());
            add("ordering_scope",
                {{"id", it.key()},
                 {"kind", info.first},
                 {"subject_id", nullable(info.second)},
                 {"ordered_ids", ids}},
                1);
        }
        QSqlQuery remembered(db);
        if (!remembered.exec("SELECT b.canonical_snapshot FROM sync_base b JOIN sync_profiles p ON "
                             "p.profile_id=b.profile_id AND p.data_epoch=b.data_epoch WHERE "
                             "b.entity_type='ordering_scope'"))
            return {{}, {ErrorCode::PersistenceError, QStringLiteral("databaseRead")}};
        while (remembered.next()) {
            auto e = QJsonDocument::fromJson(remembered.value(0).toString().toUtf8()).object();
            if (graph.entities.contains(key(e)))
                continue;
            auto value = e.value("value").toObject();
            value.insert("ordered_ids", QJsonArray{});
            add(QStringLiteral("ordering_scope"), value, 1);
        }
        const auto normalized = normalize(QJsonObject{{"entities", graph.array()},
                                                      {"aliases", graph.aliases},
                                                      {"successor_edges", graph.edges}});
        if (!normalized.ok())
            return {{}, normalized.error};
        graph.aliases = normalized.value.toObject().value("aliases").toArray();
        graph.edges = normalized.value.toObject().value("successor_edges").toArray();
        return identity.value.canonicalize(std::move(graph));
    }
    Result<void> apply(const QJsonArray &incoming) const {
        QSqlQuery deferred(db);
        if (!deferred.exec(QStringLiteral("PRAGMA defer_foreign_keys=ON")))
            return writeFailure();
        for (const auto &x : incoming)
            if (!Schema::accepts("Change", x))
                return {failure(QStringLiteral("syncProtocolMismatch"))};
        const auto current = capture();
        if (!current.ok())
            return {current.error};
        QJsonArray changes;
        for (const auto &x : incoming)
            changes.append(Identity::entityTo(x.toObject(), current.value.taskLocalIds));
        // References are deferrable for this transaction, including source/rule
        // cycles. Order still minimizes transient inconsistency for readers.
        for (const auto &kind :
             {QStringLiteral("category"), QStringLiteral("tag"), QStringLiteral("recurrence_rule"),
              QStringLiteral("task"), QStringLiteral("ordering_scope")})
            for (const auto &x : changes) {
                const auto e = x.toObject();
                if (e.value("type") != kind)
                    continue;
                const auto v = e.value("value").toObject();
                Result<void> r;
                if (kind == "category")
                    r = applyCategory(v);
                else if (kind == "tag")
                    r = upsert("tags",
                               {{"id", v.value("id")},
                                {"name", v.value("title")},
                                {"normalized_name", v.value("title").toString().toCaseFolded()},
                                {"color_key", v.value("color_key")},
                                {"created_at_utc", v.value("created_at_utc")},
                                {"updated_at_utc", v.value("updated_at_utc")},
                                {"deleted_at_utc", v.value("deleted_at_utc")}});
                else if (kind == "task")
                    r = applyTask(v);
                else if (kind == "recurrence_rule")
                    r = applyRule(v);
                else
                    r = applyOrdering(v);
                if (!r.ok())
                    return r;
            }
        return {};
    }
    Result<QList<QJsonObject>> read(const QString &table) const {
        static const QStringList allowed{"tasks", "areas",    "projects",
                                         "tags",  "headings", "recurrence_templates"};
        if (!allowed.contains(table))
            return {{}, failure()};
        QSqlQuery q(db);
        if (!q.exec("SELECT * FROM " + table + " ORDER BY id"))
            return {{}, {ErrorCode::PersistenceError, QStringLiteral("databaseRead")}};
        QList<QJsonObject> rows;
        const auto record = q.record();
        while (q.next()) {
            QJsonObject row;
            for (int i = 0; i < record.count(); ++i) {
                const auto value = q.value(i);
                QJsonValue j =
                    value.isNull() ? QJsonValue(QJsonValue::Null) : QJsonValue::fromVariant(value);
                const auto name = record.fieldName(i);
                if (QStringList{"today_focus", "evening", "pinned", "important", "archived",
                                "active", "instance_overridden"}
                        .contains(name))
                    j = value.toBool();
                row.insert(name, j);
            }
            rows.append(row);
        }
        return {rows, {}};
    }

  private:
    QSqlDatabase db;
    QJsonArray overlay;
    static Result<void> writeFailure() {
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite"));
    }
    QJsonObject taskValue(const Task &t, const QJsonObject &raw) const {
        QJsonValue schedule(QJsonValue::Null), reminder(QJsonValue::Null), old(QJsonValue::Null);
        if (t.schedule) {
            const auto &s = *t.schedule;
            if (s.allDay)
                schedule = QJsonObject{{"kind", QStringLiteral("ALL_DAY")},
                                       {"start_on", s.startOn.toString(Qt::ISODate)},
                                       {"end_on", s.endOn.toString(Qt::ISODate)}};
            else
                schedule =
                    QJsonObject{{"kind", QStringLiteral("TIMED")},
                                {"start_at_utc", s.startUtc.toUTC().toString(Qt::ISODateWithMs)},
                                {"end_at_utc", s.endUtc.toUTC().toString(Qt::ISODateWithMs)},
                                {"time_zone", s.timeZone}};
        }
        if (t.schedule && t.schedule->alertMinutes >= 0)
            reminder = QJsonObject{{"kind", QStringLiteral("RELATIVE")},
                                   {"minutes_before", t.schedule->alertMinutes},
                                   {"repeat", t.reminderRepeat},
                                   {"every", t.reminderEvery},
                                   {"time_zone", t.reminderZone}};
        else if (!t.reminderAnchorUtc.isEmpty() || !t.remindAtUtc.isEmpty())
            reminder =
                QJsonObject{{"kind", QStringLiteral("ABSOLUTE")},
                            {"anchor_at_utc",
                             t.reminderAnchorUtc.isEmpty() ? t.remindAtUtc : t.reminderAnchorUtc},
                            {"repeat", t.reminderRepeat},
                            {"every", t.reminderEvery},
                            {"time_zone", t.reminderZone}};
        if (!t.headingId.isEmpty())
            old = legacy({{"heading_id", t.headingId}});
        else if (!raw.value("sync_legacy_json").isNull())
            old =
                QJsonDocument::fromJson(raw.value("sync_legacy_json").toString().toUtf8()).object();
        QJsonArray links;
        for (const auto &l : t.links)
            links.append(QJsonObject{{"target_id", l.taskId}, {"role", l.role}});
        return {{"id", t.id},
                {"title", t.title},
                {"notes", t.notes.isNull() ? QStringLiteral("") : t.notes},
                {"planning_state", t.planningState},
                {"status", t.status},
                {"today_focus", t.todayFocus},
                {"evening", t.evening},
                {"pinned", t.pinned},
                {"important", t.important},
                {"category_id", nullable(t.projectId.isEmpty() ? t.areaId : t.projectId)},
                {"parent_task_id", nullable(t.parentTaskId)},
                {"start_on", nullable(t.startOn)},
                {"deadline_on", nullable(t.deadlineOn)},
                {"created_at_utc", t.createdAtUtc},
                {"updated_at_utc", t.updatedAtUtc},
                {"completed_at_utc", nullable(t.completedAtUtc)},
                {"cancelled_at_utc", nullable(t.cancelledAtUtc)},
                {"deleted_at_utc", nullable(t.deletedAtUtc)},
                {"deletion_group_id", nullable(t.deletionGroupId)},
                {"completion_group_id", nullable(t.completionGroupId)},
                {"schedule", schedule},
                {"reminder", reminder},
                {"recurrence_id", nullable(t.recurrenceId)},
                {"occurrence_on", t.occurrenceOn.isValid()
                                      ? QJsonValue(t.occurrenceOn.toString(Qt::ISODate))
                                      : QJsonValue(QJsonValue::Null)},
                {"occurrence_key", QJsonValue(QJsonValue::Null)},
                {"instance_overridden", t.instanceOverridden},
                {"legacy_metadata", old},
                {"tag_ids", QJsonArray::fromStringList(t.tagIds)},
                {"links", links}};
    }
    QJsonObject templateValue(const QJsonObject &row, const QJsonObject &seed,
                              const QDate &anchor) const {
        const auto original = QDate::fromString(seed.value("seed_on").toString(), Qt::ISODate);
        const auto base = original.isValid() ? original : anchor;
        const auto category = seed.value("project_id").toString().isEmpty()
                                  ? seed.value("area_id").toString()
                                  : seed.value("project_id").toString();
        QJsonArray links;
        for (const auto &x : seed.value("links").toArray()) {
            const auto l = x.toObject();
            links.append(QJsonObject{{"target_id", l.value("task_id")}, {"role", l.value("role")}});
        }
        QJsonValue schedule(QJsonValue::Null), reminder(QJsonValue::Null);
        const auto s = seed.value("schedule").toObject();
        if (!s.isEmpty()) {
            if (s.value("all_day").toBool())
                schedule = QJsonObject{
                    {"kind", QStringLiteral("ALL_DAY")},
                    {"start_day_offset",
                     base.daysTo(QDate::fromString(s.value("start_on").toString(), Qt::ISODate))},
                    {"end_day_offset",
                     base.daysTo(QDate::fromString(s.value("end_on").toString(), Qt::ISODate))}};
            else {
                const QTimeZone zone(s.value("zone").toString().toUtf8());
                const auto start = QDateTime::fromString(s.value("start_utc").toString(),
                                                         Qt::ISODateWithMs)
                                       .toTimeZone(zone),
                           end = QDateTime::fromString(s.value("end_utc").toString(),
                                                       Qt::ISODateWithMs)
                                     .toTimeZone(zone);
                schedule = QJsonObject{{"kind", QStringLiteral("TIMED")},
                                       {"start_day_offset", base.daysTo(start.date())},
                                       {"end_day_offset", base.daysTo(end.date())},
                                       {"start_local_time", start.time().toString("HH:mm:ss.zzz")},
                                       {"end_local_time", end.time().toString("HH:mm:ss.zzz")},
                                       {"time_zone", s.value("zone")}};
            }
        }
        const auto zoneId = seed.value("reminder_zone").toString(QStringLiteral("UTC"));
        QJsonObject common{
            {"repeat", seed.value("reminder_repeat").toString(QStringLiteral("NONE"))},
            {"every", seed.value("reminder_every").toInt(1)},
            {"time_zone", zoneId}};
        if (!s.isEmpty() && s.value("alert").toInt(-1) >= 0) {
            common.insert("kind", QStringLiteral("RELATIVE"));
            common.insert("minutes_before", s.value("alert"));
            reminder = common;
        } else {
            const auto at =
                QDateTime::fromString(seed.value("remind_at_utc").toString(), Qt::ISODateWithMs)
                    .toTimeZone(QTimeZone(zoneId.toUtf8()));
            if (at.isValid()) {
                common.insert("kind", QStringLiteral("ABSOLUTE"));
                common.insert("day_offset", base.daysTo(at.date()));
                common.insert("local_time", at.time().toString("HH:mm:ss.zzz"));
                reminder = common;
            }
        }
        return {
            {"template_version", 1},
            {"title", row.value("title")},
            {"notes", row.value("note_text")},
            {"planning_state", seed.value("planning_state").toString(QStringLiteral("ACTIVE"))},
            {"category_id", nullable(category)},
            {"parent_task_id", nullable(seed.value("parent_task_id").toString())},
            {"today_focus", seed.value("today_focus").toBool()},
            {"evening", seed.value("evening").toBool()},
            {"pinned", seed.value("pinned").toBool()},
            {"important", seed.value("important").toBool()},
            {"tag_ids",
             seed.value("tag_ids").isArray() ? seed.value("tag_ids") : QJsonValue(QJsonArray{})},
            {"links", links},
            {"start_day_offset",
             seed.contains("start_day_offset") ? seed.value("start_day_offset") : QJsonValue(0)},
            {"deadline_day_offset", seed.contains("deadline_days") ? seed.value("deadline_days")
                                                                   : QJsonValue(QJsonValue::Null)},
            {"schedule", schedule},
            {"reminder", reminder}};
    }
    bool exists(const QString &table, const QString &id) const {
        QSqlQuery q(db);
        q.prepare("SELECT 1 FROM " + table + " WHERE id=?");
        q.addBindValue(id);
        return q.exec() && q.next();
    }
    Result<void> upsert(const QString &table, QJsonObject row) const {
        QStringList columns, values, updates;
        QList<QVariant> binds;
        for (auto it = row.begin(); it != row.end(); ++it) {
            columns.append(it.key());
            values.append("?");
            binds.append(it.value().isNull() ? QVariant() : it.value().toVariant());
            // Incoming complete facts are authoritative, including the creation
            // time of a locally predicted recurrence successor. Dirty candidates
            // are protected by Journal before this adapter is called.
            if (it.key() != "id")
                updates.append(it.key() + "=excluded." + it.key());
        }
        QSqlQuery q(db);
        q.prepare("INSERT INTO " + table + "(" + columns.join(',') + ") VALUES(" +
                  values.join(',') + ") ON CONFLICT(id) DO UPDATE SET " + updates.join(',') +
                  ",row_version=" + table + ".row_version+1");
        for (const auto &b : binds)
            q.addBindValue(b);
        if (!q.exec())
            return writeFailure();
        return {};
    }
    Result<void> applyCategory(const QJsonObject &v) const {
        const auto l = capsule(v.value("legacy_metadata"));
        const auto id = v.value("id").toString();
        const bool area =
            exists("areas", id) || (l.value("kind") == "area" && v.value("parent_id").isNull());
        QJsonObject row{{"id", v.value("id")},
                        {area ? QStringLiteral("name") : QStringLiteral("title"), v.value("title")},
                        {"color_key", v.value("color_key")},
                        {"archived", v.value("archived")},
                        {"created_at_utc", v.value("created_at_utc")},
                        {"updated_at_utc", v.value("updated_at_utc")},
                        {"deleted_at_utc", v.value("deleted_at_utc")}};
        if (!area) {
            const auto parent = v.value("parent_id").toString();
            row.insert("parent_id", exists("areas", parent) ? QJsonValue(QJsonValue::Null)
                                                            : v.value("parent_id"));
            row.insert("area_id", exists("areas", parent) ? v.value("parent_id")
                                                          : QJsonValue(QJsonValue::Null));
            row.insert("description", v.value("description"));
            row.insert("sync_legacy_json", v.value("legacy_metadata").isNull()
                                               ? QJsonValue(QJsonValue::Null)
                                               : QJsonValue(QString::fromUtf8(
                                                     json(v.value("legacy_metadata").toObject()))));
            for (const auto &field :
                 {"planning_state", "status", "start_on", "deadline_on", "completed_at_utc"})
                if (l.contains(QLatin1String(field)))
                    row.insert(QLatin1String(field), l.value(QLatin1String(field)));
        }
        auto saved = upsert(area ? "areas" : "projects", row);
        if (!saved.ok())
            return saved;
        for (const auto &x : l.value("headings").toArray()) {
            auto h = x.toObject();
            if (h.value("project_id") != v.value("id") || !identifier(h.value("id").toString()))
                return {failure()};
            h.remove("row_version");
            auto r = upsert("headings", h);
            if (!r.ok())
                return r;
        }
        return {};
    }
    Result<void> applyTask(const QJsonObject &v) const {
        const auto id = v.value("id").toString();
        const auto before = TaskRepository(db).find(id);
        const auto r = v.value("reminder").toObject(), s = v.value("schedule").toObject();
        QString due, anchor, repeat = QStringLiteral("NONE"), zone = QStringLiteral("UTC"), fired;
        int every = 1, alert = -1;
        if (!r.isEmpty()) {
            repeat = r.value("repeat").toString();
            every = r.value("every").toInt();
            zone = r.value("time_zone").toString();
            if (r.value("kind") == "ABSOLUTE")
                anchor = r.value("anchor_at_utc").toString();
            else {
                alert = r.value("minutes_before").toInt();
                QDateTime start;
                if (s.value("kind") == "ALL_DAY")
                    start = QDateTime(
                        QDate::fromString(s.value("start_on").toString(), Qt::ISODate), QTime(9, 0),
                        QTimeZone(zone.toUtf8()), QDateTime::TransitionResolution::Reject);
                else
                    start = QDateTime::fromString(s.value("start_at_utc").toString(),
                                                  Qt::ISODateWithMs);
                if (!start.isValid())
                    return {failure()};
                anchor = start.addSecs(-60 * alert).toUTC().toString(Qt::ISODateWithMs);
            }
            due = anchor;
            if (before.ok()) {
                auto old = taskValue(before.value, {});
                if (old.value("reminder") == v.value("reminder") &&
                    old.value("schedule") == v.value("schedule")) {
                    due = before.value.remindAtUtc;
                    fired = before.value.reminderFiredAtUtc;
                }
            }
            if (fired.isEmpty() &&
                QDateTime::fromString(due, Qt::ISODateWithMs) < QDateTime::currentDateTimeUtc()) {
                if (repeat == "NONE")
                    fired = now();
                else {
                    const auto next = ReminderSchedule::nextAfter(
                        QDateTime::fromString(anchor, Qt::ISODateWithMs), repeat, every,
                        QTimeZone(zone.toUtf8()), QDateTime::currentDateTimeUtc());
                    if (next.isValid())
                        due = next.toUTC().toString(Qt::ISODateWithMs);
                }
            }
        }
        QJsonObject row{{"id", v.value("id")},
                        {"title", v.value("title")},
                        {"note_text", v.value("notes")},
                        {"project_id", QJsonValue(QJsonValue::Null)},
                        {"direct_area_id", QJsonValue(QJsonValue::Null)},
                        {"heading_id", QJsonValue(QJsonValue::Null)}};
        const auto category = v.value("category_id").toString();
        if (!category.isEmpty())
            row.insert(exists("areas", category) ? "direct_area_id" : "project_id", category);
        const auto heading = capsule(v.value("legacy_metadata")).value("heading_id").toString();
        if (exists("headings", heading) && !row.value("project_id").isNull())
            row.insert("heading_id", heading);
        for (const auto &name :
             {"parent_task_id", "planning_state", "status", "today_focus", "evening", "pinned",
              "important", "start_on", "deadline_on", "created_at_utc", "updated_at_utc",
              "completed_at_utc", "cancelled_at_utc", "deleted_at_utc", "deletion_group_id",
              "completion_group_id", "occurrence_on", "instance_overridden"})
            row.insert(QLatin1String(name), v.value(QLatin1String(name)));
        if (!v.value("recurrence_id").isNull()) {
            const auto actual = QDate::fromString(v.value("occurrence_on").toString(), Qt::ISODate);
            QDate slot = actual;
            if (before.ok() && before.value.recurrenceId == v.value("recurrence_id").toString() &&
                (before.value.syncOccurrenceKey == v.value("occurrence_key").toString() ||
                 before.value.syncOccurrenceKey.isEmpty()))
                slot = before.value.occurrenceOn;
            QSqlQuery occupied(db);
            occupied.prepare("SELECT 1 FROM tasks WHERE recurrence_template_id=? AND "
                             "occurrence_on=? AND id<>?");
            occupied.addBindValue(v.value("recurrence_id").toString());
            occupied.addBindValue(slot.toString(Qt::ISODate));
            occupied.addBindValue(id);
            if (!occupied.exec())
                return writeFailure();
            const bool occupiedSlot = occupied.next();
            occupied.finish();
            if (occupiedSlot) {
                occupied.prepare(
                    "SELECT max(occurrence_on) FROM tasks WHERE recurrence_template_id=?");
                occupied.addBindValue(v.value("recurrence_id").toString());
                if (!occupied.exec() || !occupied.next())
                    return writeFailure();
                slot = qMax(actual, QDate::fromString(occupied.value(0).toString(), Qt::ISODate))
                           .addDays(1);
                if (!slot.isValid() || slot.year() > 9999)
                    return {failure()};
            }
            row.insert("occurrence_on", slot.toString(Qt::ISODate));
            row.insert("sync_occurrence_on", actual.toString(Qt::ISODate));
        } else
            row.insert("sync_occurrence_on", QJsonValue(QJsonValue::Null));
        row.insert("recurrence_template_id", v.value("recurrence_id"));
        row.insert("sync_occurrence_key", v.value("occurrence_key"));
        row.insert("sync_legacy_json", v.value("legacy_metadata").isNull()
                                           ? QJsonValue(QJsonValue::Null)
                                           : QJsonValue(QString::fromUtf8(
                                                 json(v.value("legacy_metadata").toObject()))));
        row.insert("remind_at_utc", nullable(due));
        row.insert("reminder_anchor_utc", nullable(anchor));
        row.insert("reminder_fired_at_utc", nullable(fired));
        row.insert("reminder_repeat", repeat);
        row.insert("reminder_every", every);
        row.insert("reminder_zone", zone);
        auto saved = upsert("tasks", row);
        if (!saved.ok())
            return saved;
        QSqlQuery q(db);
        for (const auto &table : {QStringLiteral("task_tags"), QStringLiteral("task_schedules")}) {
            q.prepare("DELETE FROM " + table + " WHERE task_id=?");
            q.addBindValue(id);
            if (!q.exec())
                return writeFailure();
        }
        q.prepare("DELETE FROM task_links WHERE source_id=?");
        q.addBindValue(id);
        if (!q.exec())
            return writeFailure();
        for (const auto &tag : v.value("tag_ids").toArray()) {
            q.prepare("INSERT INTO task_tags(task_id,tag_id) VALUES(?,?)");
            q.addBindValue(id);
            q.addBindValue(tag.toString());
            if (!q.exec())
                return writeFailure();
        }
        for (const auto &link : v.value("links").toArray()) {
            const auto l = link.toObject();
            q.prepare("INSERT INTO task_links(source_id,target_id,role) VALUES(?,?,?)");
            q.addBindValue(id);
            q.addBindValue(l.value("target_id").toString());
            q.addBindValue(l.value("role").toString());
            if (!q.exec())
                return writeFailure();
        }
        if (!s.isEmpty()) {
            const bool all = s.value("kind") == "ALL_DAY";
            q.prepare("INSERT INTO "
                      "task_schedules(task_id,all_day,start_on,end_on,start_at_utc,end_at_utc,time_"
                      "zone,alert_minutes) VALUES(?,?,?,?,?,?,?,?)");
            for (const auto &b :
                 QList<QVariant>{id, all, all ? s.value("start_on").toVariant() : QVariant(),
                                 all ? s.value("end_on").toVariant() : QVariant(),
                                 all ? QVariant() : s.value("start_at_utc").toVariant(),
                                 all ? QVariant() : s.value("end_at_utc").toVariant(),
                                 all ? QVariant() : s.value("time_zone").toVariant(), alert})
                q.addBindValue(b);
            if (!q.exec())
                return writeFailure();
        }
        return {};
    }
    Result<void> applyRule(const QJsonObject &v) const {
        const auto r = v.value("rule").toObject(), t = r.value("defaults").toObject();
        const auto date = QDate::fromString(r.value("anchor_on").toString(), Qt::ISODate);
        const auto category = t.value("category_id").toString();
        QJsonObject seed{{"seed_on", date.toString(Qt::ISODate)},
                         {"recurrence_time_zone", r.value("time_zone")},
                         {"planning_state", t.value("planning_state")},
                         {"project_id", exists("areas", category) ? QStringLiteral("") : category},
                         {"area_id", exists("areas", category) ? category : QStringLiteral("")},
                         {"parent_task_id", t.value("parent_task_id").toString()},
                         {"important", t.value("important")},
                         {"tag_ids", t.value("tag_ids")}};
        for (const auto &f : {"today_focus", "evening", "pinned", "start_day_offset"})
            seed.insert(QLatin1String(f), t.value(QLatin1String(f)));
        if (!t.value("deadline_day_offset").isNull())
            seed.insert("deadline_days", t.value("deadline_day_offset"));
        QJsonArray links;
        for (const auto &x : t.value("links").toArray()) {
            const auto l = x.toObject();
            links.append(QJsonObject{{"task_id", l.value("target_id")}, {"role", l.value("role")}});
        }
        seed.insert("links", links);
        const auto s = t.value("schedule").toObject(), reminder = t.value("reminder").toObject();
        const int alert =
            reminder.value("kind") == "RELATIVE" ? reminder.value("minutes_before").toInt() : -1;
        if (!s.isEmpty()) {
            const auto start = date.addDays(s.value("start_day_offset").toInt()),
                       end = date.addDays(s.value("end_day_offset").toInt());
            const bool all = s.value("kind") == "ALL_DAY";
            QJsonObject local{{"all_day", all}, {"alert", alert}};
            if (all) {
                local.insert("start_on", start.toString(Qt::ISODate));
                local.insert("end_on", end.toString(Qt::ISODate));
            } else {
                const auto zone = s.value("time_zone").toString();
                const auto a = QDateTime(start,
                                         QTime::fromString(s.value("start_local_time").toString(),
                                                           "HH:mm:ss.zzz"),
                                         QTimeZone(zone.toUtf8()),
                                         QDateTime::TransitionResolution::Reject),
                           b = QDateTime(end,
                                         QTime::fromString(s.value("end_local_time").toString(),
                                                           "HH:mm:ss.zzz"),
                                         QTimeZone(zone.toUtf8()),
                                         QDateTime::TransitionResolution::Reject);
                if (!a.isValid() || !b.isValid())
                    return {failure()};
                local.insert("zone", zone);
                local.insert("start_utc", a.toUTC().toString(Qt::ISODateWithMs));
                local.insert("end_utc", b.toUTC().toString(Qt::ISODateWithMs));
            }
            seed.insert("schedule", local);
        }
        if (!reminder.isEmpty()) {
            seed.insert("reminder_repeat", reminder.value("repeat"));
            seed.insert("reminder_every", reminder.value("every"));
            seed.insert("reminder_zone", reminder.value("time_zone"));
            if (reminder.value("kind") == "ABSOLUTE") {
                const auto at = QDateTime(
                    date.addDays(reminder.value("day_offset").toInt()),
                    QTime::fromString(reminder.value("local_time").toString(), "HH:mm:ss.zzz"),
                    QTimeZone(reminder.value("time_zone").toString().toUtf8()),
                    QDateTime::TransitionResolution::Reject);
                if (!at.isValid())
                    return {failure()};
                seed.insert("remind_at_utc", at.toUTC().toString(Qt::ISODateWithMs));
            }
        }
        return upsert("recurrence_templates",
                      {{"id", v.value("id")},
                       {"source_task_id", v.value("source_task_id")},
                       {"title", t.value("title")},
                       {"note_text", t.value("notes")},
                       {"task_defaults_json", QString::fromUtf8(json(seed))},
                       {"mode", r.value("mode")},
                       {"frequency", r.value("frequency")},
                       {"interval_n", r.value("interval")},
                       {"anchor_on", r.value("anchor_on")},
                       {"end_on", r.value("end_on")},
                       {"weekdays_mask", r.value("weekdays_mask")},
                       {"month_day", r.value("month_day")},
                       {"year_month", r.value("year_month")},
                       {"active", v.value("active")},
                       {"created_at_utc", v.value("created_at_utc")},
                       {"updated_at_utc", v.value("updated_at_utc")},
                       {"deleted_at_utc", v.value("deleted_at_utc")}});
    }
    Result<void> applyOrdering(const QJsonObject &v) const {
        const auto kind = v.value("kind").toString();
        QSqlQuery q(db);
        qint64 order = 0;
        for (const auto &x : v.value("ordered_ids").toArray()) {
            if (kind == "today") {
                q.prepare(
                    "INSERT INTO task_view_order(task_id,view_code,sort_order) VALUES(?,'today',?) "
                    "ON CONFLICT(task_id,view_code) DO UPDATE SET sort_order=excluded.sort_order");
                q.addBindValue(x.toString());
                q.addBindValue(order++);
            } else {
                const auto table = kind == "task_siblings"         ? QStringLiteral("tasks")
                                   : exists("areas", x.toString()) ? QStringLiteral("areas")
                                                                   : QStringLiteral("projects");
                q.prepare("UPDATE " + table +
                          " SET sort_order=?,row_version=row_version+1 WHERE id=?");
                q.addBindValue(order++);
                q.addBindValue(x.toString());
            }
            if (!q.exec())
                return writeFailure();
        }
        return {};
    }
};
} // namespace daymark::sync
