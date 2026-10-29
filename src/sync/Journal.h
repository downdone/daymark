#pragma once
#include "Commands.h"
#include "persistence/ScopedTransaction.h"

namespace daymark::sync {
// All methods run on the database worker's connection. Callers own the outer
// transaction; network callbacks carry both binding and generation fences.
class Journal final {
  public:
    explicit Journal(QSqlDatabase db) : db(db) {}
    Result<QJsonObject> profile() const {
        QSqlQuery q(db);
        if (!q.exec("SELECT "
                    "profile_id,mode,server_origin,server_instance_id,user_id,dataset_id,data_"
                    "epoch,generation,auth_state,installation_id,login_label,phase FROM "
                    "sync_profiles LIMIT 1"))
            return error();
        if (!q.next())
            return {QJsonObject{{"mode", QStringLiteral("GUEST")},
                                {"phase", QStringLiteral("GUEST")},
                                {"auth_state", QStringLiteral("SIGNED_OUT")},
                                {"generation", 1}},
                    {}};
        const QStringList names{"profile_id",         "mode",        "server_origin",
                                "server_instance_id", "user_id",     "dataset_id",
                                "data_epoch",         "generation",  "auth_state",
                                "installation_id",    "login_label", "phase"};
        QJsonObject p;
        for (int i = 0; i < names.size(); ++i)
            p.insert(names[i], QJsonValue::fromVariant(q.value(i)));
        q.finish();
        q.prepare("SELECT count(*),coalesce(sum(state='BLOCKED_VALIDATION'),0) FROM sync_outbox "
                  "WHERE profile_id=? AND state NOT IN "
                  "('ACKNOWLEDGED','SUPERSEDED')");
        q.addBindValue(p.value("profile_id").toString());
        if (!q.exec() || !q.next())
            return error();
        p.insert("pending_count", q.value(0).toInt());
        p.insert("blocked_count", q.value(1).toInt());
        q.finish();
        q.prepare("SELECT "
                  "conflict_id,conflict_group_id,entity_type,entity_id,local_json,remote_json,"
                  "remote_revision "
                  "FROM sync_conflicts WHERE profile_id=? AND state='OPEN' ORDER BY conflict_id");
        q.addBindValue(p.value("profile_id").toString());
        if (!q.exec())
            return error();
        QJsonArray conflicts;
        QHash<QString, int> groups;
        while (q.next()) {
            const auto local = QJsonDocument::fromJson(q.value(4).toString().toUtf8()).object(),
                       remote = QJsonDocument::fromJson(q.value(5).toString().toUtf8()).object();
            const auto group = q.value(1).toString();
            QJsonObject item{{"id", q.value(0).toString()},
                             {"group", q.value(1).toString()},
                             {"type", q.value(2).toString()},
                             {"entity_id", q.value(3).toString()},
                             {"local_title", local.value("value").toObject().value("title")},
                             {"remote_title", remote.value("value").toObject().value("title")},
                             {"local_summary", preview(local.value("value").toObject())},
                             {"remote_summary", q.value(6).toLongLong() > 0
                                                    ? preview(remote.value("value").toObject())
                                                    : QStringLiteral("服务器中尚无此项。")},
                             {"remote_available", q.value(6).toLongLong() > 0},
                             {"related_count", 1}};
            if (!groups.contains(group)) {
                groups.insert(group, int(conflicts.size()));
                conflicts.append(item);
            } else {
                const auto i = groups.value(group);
                auto previous = conflicts.at(i).toObject();
                const int count = previous.value("related_count").toInt() + 1;
                const bool available = previous.value("remote_available").toBool() &&
                                       item.value("remote_available").toBool();
                if (previous.value("local_title").toString().isEmpty() &&
                    !item.value("local_title").toString().isEmpty())
                    previous = item;
                previous.insert("related_count", count);
                previous.insert("remote_available", available);
                conflicts[i] = previous;
            }
        }
        p.insert("conflicts", conflicts);
        return {p, {}};
    }
    Result<QJsonObject> bind(const QJsonObject &input) const {
        auto p = profile();
        if (!p.ok())
            return p;
        const auto me = input.value("me").toObject(), caps = input.value("capabilities").toObject();
        const auto datasets = me.value("datasets").toArray();
        if (datasets.isEmpty())
            return {{}, failure()};
        const auto dataset = datasets.first().toObject();
        if (!Schema::accepts("Me", me) || !Schema::accepts("Capabilities", caps))
            return {{}, failure()};
        if (p.value.value("mode") == "ACCOUNT") {
            if (p.value.value("server_origin") != input.value("origin") ||
                p.value.value("user_id") != me.value("user_id") ||
                p.value.value("server_instance_id") != caps.value("server_instance_id") ||
                p.value.value("dataset_id") != dataset.value("id"))
                return {{}, failure(QStringLiteral("syncProfileMismatch"))};
            if (p.value.value("data_epoch") != dataset.value("data_epoch"))
                return requireReconcile();
            if (!exec("UPDATE sync_profiles SET auth_state='AUTHENTICATED',generation=generation+1 "
                      "WHERE profile_id=?",
                      {p.value.value("profile_id").toVariant()}) ||
                !trackSession(p.value.value("profile_id").toString(),
                              input.value("session_id").toString()))
                return error();
            return profile();
        }
        const auto captured = Mapping(db).capture();
        if (!captured.ok())
            return {{}, captured.error};
        QJsonObject initial{{"dto_version", 1},
                            {"base_head_seq", QStringLiteral("0")},
                            {"entities", captured.value.array()},
                            {"aliases", captured.value.aliases},
                            {"successor_edges", captured.value.edges}};
        if (!Schema::accepts("ImportPayload", initial))
            return {{}, failure(QStringLiteral("syncLegacyUnsupported"))};
        if (!exec("INSERT INTO "
                  "sync_profiles(profile_id,mode,server_origin,server_instance_id,user_id,dataset_"
                  "id,data_epoch,generation,auth_state,installation_id,login_label,phase,initial_"
                  "payload_json) VALUES(?,'ACCOUNT',?,?,?,?,?,1,'AUTHENTICATED',?,?,'BOOTSTRAP',?)",
                  {uuid(), input.value("origin").toVariant(),
                   caps.value("server_instance_id").toVariant(), me.value("user_id").toVariant(),
                   dataset.value("id").toVariant(), dataset.value("data_epoch").toVariant(),
                   input.value("installation_id").toVariant(), input.value("label").toVariant(),
                   QString::fromUtf8(json(initial))}))
            return error();
        const auto bound = profile();
        if (!bound.ok())
            return bound;
        const auto initialized = bootstrap(bound.value);
        if (!initialized.ok())
            return initialized;
        if (!trackSession(bound.value.value("profile_id").toString(),
                          input.value("session_id").toString()))
            return error();
        return bound;
    }
    Result<void> record(const Graph &before, const Graph &after) const {
        auto p = profile();
        if (!p.ok())
            return {p.error};
        if (p.value.value("mode") != "ACCOUNT")
            return {};
        const auto delta = commandsBetween(before, after);
        if (delta.dirty.isEmpty())
            return {};
        const auto mid = uuid(), pid = p.value.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT next_local_seq FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return {error().error};
        const auto seq = q.value(0).toLongLong();
        q.finish();
        QJsonObject old, latest;
        for (const auto &id : delta.dirty) {
            if (before.entities.contains(id))
                old.insert(id, before.entities.value(id));
            if (after.entities.contains(id))
                latest.insert(id, after.entities.value(id));
        }
        const auto state = p.value.value("phase") == "RECONCILE_REQUIRED"
                               ? QStringLiteral("RECONCILE_REQUIRED")
                           : delta.unsupported ? QStringLiteral("BLOCKED_VALIDATION")
                                               : QStringLiteral("PENDING");
        if (!exec("INSERT INTO "
                  "sync_outbox(profile_id,mutation_id,origin_epoch,local_seq,state,kind,intent_"
                  "json,base_snapshot_json,after_snapshot_json,not_before_ms,created_ms,safe_error_"
                  "code) VALUES(?,?,?,?,?,'COMMANDS',?,?,?,?,?,?)",
                  {pid, mid, p.value.value("data_epoch").toVariant(), seq, state,
                   QString::fromUtf8(json({{"commands", delta.commands}})),
                   QString::fromUtf8(json(old)), QString::fromUtf8(json(latest)),
                   QDateTime::currentMSecsSinceEpoch(), QDateTime::currentMSecsSinceEpoch(),
                   delta.unsupported ? QVariant("legacy_format_unsupported") : QVariant()}))
            return {error().error};
        if (!exec("UPDATE sync_profiles SET next_local_seq=next_local_seq+1 WHERE profile_id=?",
                  {pid}))
            return {error().error};
        if (!exec("INSERT INTO sync_dependencies(profile_id,mutation_id,depends_on_id) SELECT "
                  "profile_id,?,mutation_id FROM sync_outbox WHERE profile_id=? AND local_seq<? "
                  "AND state<>'SUPERSEDED' AND echo_applied=0",
                  {mid, pid, seq}))
            return {error().error};
        for (const auto &id : delta.dirty) {
            const int split = id.indexOf(':');
            if (!exec("INSERT INTO "
                      "sync_dirty_entities(profile_id,mutation_id,entity_type,entity_id,captured_"
                      "row_version) VALUES(?,?,?,?,?)",
                      {pid, mid, id.left(split), id.mid(split + 1),
                       after.localVersions.value(id, before.localVersions.value(id, 1))}))
                return {error().error};
        }
        return {};
    }
    Result<void> recordImport(const Graph &before, const Graph &after) const {
        auto p = profile();
        if (!p.ok())
            return {p.error};
        if (p.value.value("mode") != "ACCOUNT")
            return {};
        QJsonArray added;
        Graph remainder = after;
        QStringList keys;
        QSet<QString> commandRules, commandTasks;
        for (auto it = after.entities.begin(); it != after.entities.end(); ++it)
            if (!before.entities.contains(it.key()) &&
                it.value().value("type") == "recurrence_rule") {
                const auto source =
                    it.value().value("value").toObject().value("source_task_id").toString();
                if (before.entities.contains("task:" + source))
                    commandRules.insert(it.value().value("id").toString());
            }
        for (const auto &entity : after.entities)
            if (entity.value("type") == "task" &&
                commandRules.contains(
                    entity.value("value").toObject().value("recurrence_id").toString()))
                commandTasks.insert(entity.value("id").toString());
        for (auto it = after.entities.begin(); it != after.entities.end(); ++it)
            if (!before.entities.contains(it.key())) {
                const auto entity = it.value(), value = entity.value("value").toObject();
                // Attaching a new generation to an existing source is a typed
                // command. Importing only its rule would lose that attachment.
                if ((entity.value("type") == "recurrence_rule" &&
                     commandRules.contains(entity.value("id").toString())) ||
                    (entity.value("type") == "task" &&
                     commandTasks.contains(entity.value("id").toString())) ||
                    (entity.value("type") == "ordering_scope" &&
                     commandTasks.contains(value.value("subject_id").toString())))
                    continue;
                added.append(it.value());
                keys.append(it.key());
                remainder.entities.remove(it.key());
            }
        if (!added.isEmpty()) {
            QJsonObject payload{{"dto_version", 1},
                                {"base_head_seq", QStringLiteral("0")},
                                {"entities", added},
                                {"aliases", after.aliases},
                                {"successor_edges", after.edges}};
            if (!Schema::accepts("ImportPayload", payload))
                return {failure(QStringLiteral("syncLegacyUnsupported"))};
            const auto pid = p.value.value("profile_id").toString(), mid = uuid();
            QSqlQuery q(db);
            q.prepare("SELECT next_local_seq FROM sync_profiles WHERE profile_id=?");
            q.addBindValue(pid);
            if (!q.exec() || !q.next())
                return {error().error};
            const auto seq = q.value(0).toLongLong();
            q.finish();
            if (!exec("INSERT INTO "
                      "sync_outbox(profile_id,mutation_id,origin_epoch,local_seq,state,kind,intent_"
                      "json,base_snapshot_json,after_snapshot_json,not_before_ms,created_ms) "
                      "VALUES(?,?,?,?,'PENDING','IMPORT',?,'{}',?,0,?)",
                      {pid, mid, p.value.value("data_epoch").toVariant(), seq,
                       QString::fromUtf8(json(payload)),
                       QString::fromUtf8(json({{"entities", added}})),
                       QDateTime::currentMSecsSinceEpoch()}) ||
                !exec("UPDATE sync_profiles SET next_local_seq=next_local_seq+1 WHERE profile_id=?",
                      {pid}) ||
                !exec("INSERT INTO sync_dependencies(profile_id,mutation_id,depends_on_id) SELECT "
                      "profile_id,?,mutation_id FROM sync_outbox WHERE profile_id=? AND "
                      "local_seq<? AND echo_applied=0 AND state<>'SUPERSEDED'",
                      {mid, pid, seq}))
                return {error().error};
            for (const auto &k : keys) {
                const int split = k.indexOf(':');
                if (!exec("INSERT INTO sync_dirty_entities VALUES(?,?,?,?,?)",
                          {pid, mid, k.left(split), k.mid(split + 1),
                           after.localVersions.value(k, 1)}))
                    return {error().error};
            }
        }
        return record(before, remainder);
    }
    Result<QJsonObject> execute(const QString &operation, const QJsonObject &input) const {
        auto current = profile();
        if (!current.ok())
            return current;
        const auto p = current.value;
        if (operation == "profile")
            return current;
        if (operation == "bind")
            return bind(input);
        if (p.value("mode") != "ACCOUNT")
            return {{}, failure(QStringLiteral("syncAccountRequired"))};
        if (input.contains("generation") && input.value("generation") != p.value("generation"))
            return {{}, {ErrorCode::Cancelled, QStringLiteral("syncStaleCallback")}};
        if (input.contains("profile_id") && input.value("profile_id") != p.value("profile_id"))
            return {{}, {ErrorCode::Cancelled, QStringLiteral("syncStaleCallback")}};
        const auto pid = p.value("profile_id").toString();
        if (operation == "revocations" || operation == "revoked")
            return revocations(p, input, operation);
        if (operation == "logout" || operation == "auth_required") {
            if (operation == "logout") {
                const auto pending = revocations(p, input, QStringLiteral("add"));
                if (!pending.ok())
                    return pending;
            }
            if (!exec("UPDATE sync_profiles SET auth_state=?,generation=generation+1 WHERE "
                      "profile_id=?",
                      {operation == "logout" ? "SIGNED_OUT" : "AUTH_REQUIRED", pid}) ||
                !exec("UPDATE sync_outbox SET state='RETRY_WAIT' WHERE profile_id=? AND "
                      "state='IN_FLIGHT'",
                      {pid}))
                return error();
            return profile();
        }
        if (operation == "reconcile")
            return requireReconcile();
        if (operation == "rebuild")
            return rebuild(p, input.value("dataset").toObject());
        if (operation == "restart_bootstrap") {
            if (p.value("phase") != "BOOTSTRAP")
                return {{}, failure()};
            auto next = p;
            next.insert("phase", QStringLiteral("RECONCILE_REQUIRED"));
            return rebuild(next, {{"id", p.value("dataset_id")},
                                  {"data_epoch", p.value("data_epoch")},
                                  {"head_seq", QStringLiteral("0")}});
        }
        if (operation == "prepare")
            return prepare(p);
        if (operation == "transport")
            return transport(p, input);
        if (operation == "rebase_import")
            return rebaseImport(p, input.value("mutation_id").toString());
        if (operation == "receipt")
            return receipt(p, input.value("receipt").toObject());
        if (operation == "retry" || operation == "reject") {
            const auto state = operation == "retry" ? QStringLiteral("RETRY_WAIT")
                                                    : QStringLiteral("BLOCKED_VALIDATION");
            if (!exec(
                    "UPDATE sync_outbox SET state=?,not_before_ms=?,safe_error_code=? WHERE "
                    "profile_id=? AND mutation_id=? AND state='IN_FLIGHT'",
                    {state,
                     QDateTime::currentMSecsSinceEpoch() + input.value("delay_ms").toInteger(1000),
                     input.value("code").toVariant(), pid, input.value("mutation_id").toVariant()}))
                return error();
            return profile();
        }
        if (operation == "checkpoint")
            return checkpoint(p);
        if (operation == "batch")
            return batch(p, input);
        if (operation == "tail") {
            qint64 head;
            if (!counter(input.value("head_seq"), &head))
                return {{}, failure()};
            if (!exec("UPDATE sync_cursor SET opaque_cursor=?,observed_head=max(observed_head,?) "
                      "WHERE profile_id=? AND opaque_cursor=?",
                      {input.value("cursor").toVariant(), head, pid,
                       input.value("expected_cursor").toVariant()}))
                return error();
            return checkpoint(p);
        }
        if (operation == "snapshot")
            return snapshot(p, input);
        if (operation == "initial")
            return initial(p);
        if (operation == "bootstrap")
            return bootstrap(p);
        if (operation == "freeze_import") {
            const auto meta = bootstrap(p);
            if (!meta.ok())
                return meta;
            const auto payload = input.value("payload").toObject();
            if (!Schema::accepts("ImportPayload", payload))
                return {{}, failure()};
            if (!exec("UPDATE sync_outbox SET "
                      "frozen_payload=?,canonical_hash=?,preconditions_json='{}' WHERE "
                      "profile_id=? AND mutation_id=? AND frozen_payload IS NULL",
                      {json(payload), digest(payload), pid,
                       meta.value.value("mutation_id").toVariant()}))
                return error();
            return meta;
        }
        if (operation == "initial_committed") {
            if (!exec("UPDATE sync_profiles SET phase='ACTIVE',initial_payload_json=NULL WHERE "
                      "profile_id=?",
                      {pid}))
                return error();
            return profile();
        }
        if (operation == "conflict")
            return block(p, input.value("mutation_id").toString());
        if (operation == "ensure_conflicts")
            return ensureConflicts(p);
        if (operation == "resolve")
            return resolve(p, input);
        return {{}, failure()};
    }

  private:
    QSqlDatabase db;
    static Result<QJsonObject> error() {
        return {{}, {ErrorCode::PersistenceError, QStringLiteral("syncDatabaseWrite")}};
    }
    Result<void> adoptIdentities(const QJsonObject &p, const QJsonArray &changes,
                                 const QJsonArray &aliases) const {
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare(
            "SELECT graph_meta_json,initial_payload_json FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return {error().error};
        auto root = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
        auto initial = QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object();
        q.finish();
        QMap<QString, QString> merged;
        for (const auto &x : root.value("identity_aliases").toArray()) {
            const auto a = x.toObject();
            merged.insert(a.value("alias_id").toString(), a.value("canonical_id").toString());
        }
        for (const auto &x : aliases) {
            if (!Schema::accepts("Alias", x))
                return {failure()};
            const auto a = x.toObject();
            const auto from = a.value("alias_id").toString(),
                       to = a.value("canonical_id").toString();
            if (from == to || (merged.contains(from) && merged.value(from) != to))
                return {failure(QStringLiteral("syncProtocolMismatch"))};
            merged.insert(from, to);
        }
        QJsonArray all;
        for (auto it = merged.begin(); it != merged.end(); ++it)
            all.append(QJsonObject{{"alias_id", it.key()}, {"canonical_id", it.value()}});
        root.insert("identity_aliases", all);
        if (!exec("UPDATE sync_profiles SET graph_meta_json=? WHERE profile_id=?",
                  {QString::fromUtf8(json(root)), pid}))
            return {error().error};
        const auto mapped = Mapping(db, changes).capture();
        if (!mapped.ok())
            return {mapped.error};
        auto bindings = root.value("physical_task_ids").toObject();
        for (auto it = mapped.value.taskWireIds.begin(); it != mapped.value.taskWireIds.end(); ++it)
            if (it.key() != it.value())
                bindings.insert(it.key(), it.value());
        root.insert("physical_task_ids", bindings);
        if (!exec("UPDATE sync_profiles SET graph_meta_json=? WHERE profile_id=?",
                  {QString::fromUtf8(json(root)), pid}))
            return {error().error};
        auto scopes = mapped.value.scopeWireIds;
        for (auto it = mapped.value.taskWireIds.begin(); it != mapped.value.taskWireIds.end(); ++it)
            scopes.insert(orderingId("task_siblings", it.key()),
                          orderingId("task_siblings", it.value()));
        if (!initial.isEmpty()) {
            initial = Identity::documentTo(initial, mapped.value.taskWireIds, scopes);
            if (!exec("UPDATE sync_profiles SET initial_payload_json=? WHERE profile_id=?",
                      {QString::fromUtf8(json(initial)), pid}))
                return {error().error};
        }
        q.prepare(
            "SELECT "
            "mutation_id,intent_json,base_snapshot_json,after_snapshot_json,frozen_payload,state "
            "FROM sync_outbox WHERE profile_id=? AND state<>'SUPERSEDED' AND echo_applied=0");
        q.addBindValue(pid);
        if (!q.exec())
            return {error().error};
        QList<QList<QVariant>> rows;
        while (q.next()) {
            QList<QVariant> row;
            for (int i = 0; i < 6; ++i)
                row.append(q.value(i));
            rows.append(row);
        }
        q.finish();
        for (const auto &row : rows) {
            const auto mid = row[0].toString();
            const auto intent = QJsonDocument::fromJson(row[1].toString().toUtf8()).object();
            const auto converted = Identity::documentTo(intent, mapped.value.taskWireIds, scopes);
            const auto before = Identity::documentTo(
                           QJsonDocument::fromJson(row[2].toString().toUtf8()).object(),
                           mapped.value.taskWireIds, scopes),
                       after = Identity::documentTo(
                           QJsonDocument::fromJson(row[3].toString().toUtf8()).object(),
                           mapped.value.taskWireIds, scopes);
            if (row[4].isNull()) {
                if (!exec("UPDATE sync_outbox SET "
                          "intent_json=?,base_snapshot_json=?,after_snapshot_json=? WHERE "
                          "profile_id=? AND mutation_id=?",
                          {QString::fromUtf8(json(converted)), QString::fromUtf8(json(before)),
                           QString::fromUtf8(json(after)), pid, mid}))
                    return {error().error};
            } else {
                if (!exec("UPDATE sync_outbox SET base_snapshot_json=?,after_snapshot_json=? WHERE "
                          "profile_id=? AND mutation_id=?",
                          {QString::fromUtf8(json(before)), QString::fromUtf8(json(after)), pid,
                           mid}))
                    return {error().error};
                if (converted != intent && row[5].toString() != "ACKNOWLEDGED" &&
                    !exec("UPDATE sync_outbox SET "
                          "state='BLOCKED_CONFLICT',safe_error_code='identity_rebase' WHERE "
                          "profile_id=? AND mutation_id=?",
                          {pid, mid}))
                    return {error().error};
            }
            q.prepare("SELECT entity_type,entity_id,captured_row_version FROM sync_dirty_entities "
                      "WHERE profile_id=? AND mutation_id=?");
            q.addBindValue(pid);
            q.addBindValue(mid);
            if (!q.exec())
                return {error().error};
            QList<QList<QVariant>> dirty;
            while (q.next()) {
                const auto type = q.value(0).toString(), id = q.value(1).toString();
                dirty.append({type,
                              type == "task"             ? mapped.value.taskWireIds.value(id, id)
                              : type == "ordering_scope" ? scopes.value(id, id)
                                                         : id,
                              q.value(2)});
            }
            q.finish();
            if (!exec("DELETE FROM sync_dirty_entities WHERE profile_id=? AND mutation_id=?",
                      {pid, mid}))
                return {error().error};
            for (const auto &d : dirty)
                if (!exec("INSERT INTO sync_dirty_entities VALUES(?,?,?,?,?)",
                          {pid, mid, d[0], d[1], d[2]}))
                    return {error().error};
        }
        return {};
    }
    static QString preview(const QJsonObject &value) {
        if (value.isEmpty())
            return QStringLiteral("此设备中尚无此项。");
        QStringList lines;
        auto title = value.value("title").toString();
        const auto defaults = value.value("rule").toObject().value("defaults").toObject();
        if (title.isEmpty())
            title = defaults.value("title").toString();
        if (!title.isEmpty())
            lines.append(title);
        if (!value.value("deleted_at_utc").isNull() && value.contains("deleted_at_utc"))
            lines.append(QStringLiteral("已删除"));
        else if (value.value("status") == "COMPLETED")
            lines.append(QStringLiteral("已完成"));
        else if (value.value("status") == "CANCELLED")
            lines.append(QStringLiteral("已取消"));
        if (value.value("archived").toBool())
            lines.append(QStringLiteral("已归档"));
        QString notes = value.value("notes").toString(
            value.value("description").toString(defaults.value("notes").toString()));
        if (notes.size() > 1000) {
            notes.truncate(1000);
            if (notes.back().isHighSurrogate())
                notes.chop(1);
            notes += QStringLiteral("…");
        }
        if (!notes.isEmpty())
            lines.append(notes);
        const auto start = value.value("start_on").toString(),
                   end = value.value("deadline_on").toString();
        if (!start.isEmpty() || !end.isEmpty())
            lines.append(QStringLiteral("安排：") + start +
                         (end.isEmpty() ? QString() : QStringLiteral(" — ") + end));
        const auto schedule = value.value("schedule").toObject();
        if (schedule.value("kind") == "TIMED")
            lines.append(schedule.value("start_at_utc").toString() + QStringLiteral(" — ") +
                         schedule.value("end_at_utc").toString() + QStringLiteral("（") +
                         schedule.value("time_zone").toString() + QStringLiteral("）"));
        if (value.value("important").toBool())
            lines.append(QStringLiteral("重要"));
        if (value.value("pinned").toBool())
            lines.append(QStringLiteral("置顶"));
        if (value.contains("ordered_ids"))
            lines.append(
                QStringLiteral("排列顺序：%1 项").arg(value.value("ordered_ids").toArray().size()));
        return lines.join(QLatin1Char('\n'));
    }
    bool exec(const QString &sql, const QList<QVariant> &binds = {}) const {
        QSqlQuery q(db);
        q.prepare(sql);
        for (const auto &b : binds)
            q.addBindValue(b);
        return q.exec();
    }
    bool trackSession(const QString &pid, const QString &session) const {
        return session.isEmpty() ||
               (identifier(session) && exec("UPDATE sync_profiles SET "
                                            "graph_meta_json=json_set(graph_meta_json,'$.recent_"
                                            "session_id',?) WHERE profile_id=?",
                                            {session, pid}));
    }
    Result<QJsonObject> revocations(const QJsonObject &p, const QJsonObject &input,
                                    const QString &op) const {
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT graph_meta_json FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        auto root = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
        q.finish();
        auto pending = root.value("pending_revocations").toArray();
        const auto session =
            input.value("session_id").toString(root.value("recent_session_id").toString());
        if (op == "add" && identifier(session) && !pending.contains(session))
            pending.append(session);
        if (op == "revoked") {
            if (!identifier(session))
                return {{}, failure()};
            QJsonArray filtered;
            for (const auto &id : pending)
                if (id != session)
                    filtered.append(id);
            pending = filtered;
        }
        if (op != "revocations") {
            root.insert("pending_revocations", pending);
            if (!exec("UPDATE sync_profiles SET graph_meta_json=? WHERE profile_id=?",
                      {QString::fromUtf8(json(root)), pid}))
                return error();
        }
        return {QJsonObject{{"session_ids", pending}}, {}};
    }
    Result<QJsonObject> requireReconcile() const {
        if (!exec(
                "UPDATE sync_profiles SET "
                "phase='RECONCILE_REQUIRED',auth_state='AUTH_REQUIRED',generation=generation+1") ||
            !exec("UPDATE sync_outbox SET state='RECONCILE_REQUIRED' WHERE state NOT IN "
                  "('ACKNOWLEDGED','SUPERSEDED')"))
            return error();
        return profile();
    }
    Result<QJsonObject> checkpoint(const QJsonObject &p) const {
        QSqlQuery q(db);
        q.prepare(
            "SELECT opaque_cursor,applied_seq,observed_head FROM sync_cursor WHERE profile_id=?");
        q.addBindValue(p.value("profile_id").toString());
        if (!q.exec())
            return error();
        if (!q.next())
            return {QJsonObject{{"cursor", QStringLiteral("")},
                                {"applied_seq", QStringLiteral("0")},
                                {"head_seq", QStringLiteral("0")}},
                    {}};
        return {QJsonObject{{"cursor", q.value(0).toString()},
                            {"applied_seq", QString::number(q.value(1).toLongLong())},
                            {"head_seq", QString::number(q.value(2).toLongLong())}},
                {}};
    }
    Result<QJsonObject> transport(const QJsonObject &p, const QJsonObject &input) const {
        const auto pid = p.value("profile_id").toString(),
                   mid = input.value("mutation_id").toString();
        if (!identifier(mid))
            return {{}, failure()};
        QSqlQuery q(db);
        q.prepare("SELECT canonical_hash FROM sync_outbox WHERE profile_id=? AND mutation_id=? AND "
                  "frozen_payload IS NOT NULL");
        q.addBindValue(pid);
        q.addBindValue(mid);
        if (!q.exec() || !q.next())
            return {{}, failure()};
        const auto hash = q.value(0).toByteArray();
        q.finish();
        q.prepare("SELECT graph_meta_json FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        auto root = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object(),
             all = root.value("transport").toObject(), item = all.value(mid).toObject();
        q.finish();
        if (item.isEmpty() || input.value("reset").toBool())
            item = {{"operation_id", uuid()}, {"payload_ref", QJsonValue(QJsonValue::Null)}};
        if (input.contains("payload_ref")) {
            const auto ref = input.value("payload_ref");
            if (!Schema::accepts("PayloadRef", ref) ||
                ref.toObject().value("canonical_sha256").toString().toLatin1() != hash.toHex())
                return {{}, failure(QStringLiteral("syncHashMismatch"))};
            item.insert("payload_ref", ref);
        }
        all.insert(mid, item);
        root.insert("transport", all);
        if (!exec("UPDATE sync_profiles SET graph_meta_json=? WHERE profile_id=?",
                  {QString::fromUtf8(json(root)), pid}))
            return error();
        return {item, {}};
    }
    Result<QJsonObject> rebaseImport(const QJsonObject &p, const QString &mutation) const {
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT local_seq FROM sync_outbox WHERE profile_id=? AND mutation_id=? AND "
                  "kind='IMPORT' AND state='IN_FLIGHT'");
        q.addBindValue(pid);
        q.addBindValue(mutation);
        if (!q.exec() || !q.next())
            return {{}, failure()};
        const auto first = q.value(0).toLongLong();
        q.finish();
        q.prepare(
            "SELECT "
            "mutation_id,kind,intent_json,base_snapshot_json,after_snapshot_json,state,safe_error_"
            "code FROM sync_outbox WHERE profile_id=? AND local_seq>=? AND echo_applied=0 AND "
            "state NOT IN ('ACKNOWLEDGED','SUPERSEDED') ORDER BY local_seq");
        q.addBindValue(pid);
        q.addBindValue(first);
        if (!q.exec())
            return error();
        QList<QList<QVariant>> rows;
        while (q.next()) {
            QList<QVariant> r;
            for (int i = 0; i < 7; ++i)
                r.append(q.value(i));
            rows.append(r);
        }
        q.finish();
        if (!exec("UPDATE sync_outbox SET state='SUPERSEDED',echo_applied=1 WHERE profile_id=? AND "
                  "local_seq>=? AND echo_applied=0 AND state NOT IN ('ACKNOWLEDGED','SUPERSEDED')",
                  {pid, first}))
            return error();
        for (const auto &row : rows) {
            q.prepare("SELECT next_local_seq FROM sync_profiles WHERE profile_id=?");
            q.addBindValue(pid);
            if (!q.exec() || !q.next())
                return error();
            const auto seq = q.value(0).toLongLong();
            q.finish();
            const auto mid = uuid();
            const auto state = row[5].toString() == "BLOCKED_VALIDATION"
                                   ? QStringLiteral("BLOCKED_VALIDATION")
                                   : QStringLiteral("PENDING");
            if (!exec("INSERT INTO "
                      "sync_outbox(profile_id,mutation_id,origin_epoch,local_seq,state,kind,intent_"
                      "json,base_snapshot_json,after_snapshot_json,not_before_ms,created_ms,safe_"
                      "error_code) VALUES(?,?,?,?,?,?,?,?,?,0,?,?)",
                      {pid, mid, p.value("data_epoch").toVariant(), seq, state, row[1], row[2],
                       row[3], row[4], QDateTime::currentMSecsSinceEpoch(), row[6]}) ||
                !exec("INSERT INTO sync_dirty_entities SELECT "
                      "profile_id,?,entity_type,entity_id,captured_row_version FROM "
                      "sync_dirty_entities WHERE profile_id=? AND mutation_id=?",
                      {mid, pid, row[0]}) ||
                !exec("INSERT INTO sync_dependencies SELECT profile_id,?,mutation_id FROM "
                      "sync_outbox WHERE profile_id=? AND local_seq<? AND echo_applied=0 AND "
                      "state<>'SUPERSEDED'",
                      {mid, pid, seq}) ||
                !exec("UPDATE sync_profiles SET next_local_seq=next_local_seq+1 WHERE profile_id=?",
                      {pid}) ||
                !exec(
                    "UPDATE sync_conflicts SET mutation_id=? WHERE profile_id=? AND mutation_id=?",
                    {mid, pid, row[0]}))
                return error();
        }
        return profile();
    }
    Result<QJsonObject> prepare(const QJsonObject &p) const {
        if (p.value("phase") != "ACTIVE" || p.value("auth_state") != "AUTHENTICATED")
            return {QJsonObject{}, {}};
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare(
            "SELECT mutation_id,local_seq,intent_json,frozen_payload,kind FROM sync_outbox o WHERE "
            "profile_id=? AND state IN ('PENDING','IN_FLIGHT','RETRY_WAIT') AND not_before_ms<=? "
            "AND NOT EXISTS(SELECT 1 FROM sync_dependencies d JOIN sync_outbox parent ON "
            "parent.profile_id=d.profile_id AND parent.mutation_id=d.depends_on_id WHERE "
            "d.profile_id=o.profile_id AND d.mutation_id=o.mutation_id AND parent.echo_applied=0 "
            "AND parent.state<>'SUPERSEDED') ORDER BY local_seq LIMIT 1");
        q.addBindValue(pid);
        q.addBindValue(QDateTime::currentMSecsSinceEpoch());
        if (!q.exec())
            return error();
        if (!q.next())
            return {QJsonObject{}, {}};
        const auto mid = q.value(0).toString();
        const auto seq = q.value(1).toLongLong();
        const auto intent = QJsonDocument::fromJson(q.value(2).toString().toUtf8()).object();
        QByteArray frozen = q.value(3).toByteArray();
        const auto kind = q.value(4).toString();
        q.finish();
        if (kind == "IMPORT") {
            if (frozen.isEmpty()) {
                auto payload = intent;
                const auto cp = checkpoint(p);
                if (!cp.ok())
                    return cp;
                payload.insert("base_head_seq", cp.value.value("applied_seq"));
                if (!Schema::accepts("ImportPayload", payload))
                    return {{}, failure()};
                frozen = json(payload);
                if (!exec("UPDATE sync_outbox SET "
                          "frozen_payload=?,canonical_hash=?,preconditions_json='{}' WHERE "
                          "profile_id=? AND mutation_id=? AND frozen_payload IS NULL",
                          {frozen, digest(payload), pid, mid}))
                    return error();
            }
            if (!exec("UPDATE sync_outbox SET state='IN_FLIGHT',attempt=attempt+1 WHERE "
                      "profile_id=? AND mutation_id=?",
                      {pid, mid}))
                return error();
            return {QJsonObject{{"kind", QStringLiteral("IMPORT")},
                                {"mutation_id", mid},
                                {"client_seq", QString::number(seq)},
                                {"payload", QString::fromUtf8(frozen)}},
                    {}};
        }
        if (frozen.isEmpty()) {
            QMap<QString, QJsonObject> entities, structures;
            q.prepare("SELECT entity_type,entity_id,server_revision FROM sync_base WHERE "
                      "profile_id=? AND data_epoch=?");
            q.addBindValue(pid);
            q.addBindValue(p.value("data_epoch").toString());
            if (!q.exec())
                return error();
            while (q.next()) {
                QJsonObject v{{"type", q.value(0).toString()},
                              {"id", q.value(1).toString()},
                              {"revision", QString::number(q.value(2).toLongLong())}};
                entities.insert(key(v), v);
            }
            q.finish();
            q.prepare("SELECT kind,entity_id,server_revision FROM sync_structures WHERE "
                      "profile_id=? AND data_epoch=?");
            q.addBindValue(pid);
            q.addBindValue(p.value("data_epoch").toString());
            if (!q.exec())
                return error();
            while (q.next()) {
                QJsonObject v{{"kind", q.value(0).toString()},
                              {"id", q.value(1).toString()},
                              {"revision", QString::number(q.value(2).toLongLong())}};
                structures.insert(v.value("kind").toString() + ":" + v.value("id").toString(), v);
            }
            q.finish();
            auto entityRead = [&](const QString &type, const QString &id) {
                if (id.isEmpty())
                    return;
                const auto k = type + ":" + id;
                if (!entities.contains(k))
                    entities.insert(
                        k, {{"type", type}, {"id", id}, {"revision", QStringLiteral("0")}});
            };
            auto structure = [&](const QString &kind, const QString &id) {
                const auto k = kind + ":" + id;
                if (!structures.contains(k))
                    structures.insert(
                        k, {{"kind", kind}, {"id", id}, {"revision", QStringLiteral("0")}});
            };
            const auto local = Mapping(db).capture();
            if (!local.ok())
                return {{}, local.error};
            const auto nil = QStringLiteral("00000000-0000-0000-0000-000000000000");
            structure("category_tree", p.value("dataset_id").toString());
            structure("category_members", nil);
            for (const auto &en : local.value.entities) {
                const auto type = en.value("type").toString(), id = en.value("id").toString();
                entityRead(type, id);
                if (type == "task")
                    structure("task_tree", id);
                else if (type == "category")
                    structure("category_members", id);
                else if (type == "tag")
                    structure("tag_members", id);
                else if (type == "recurrence_rule")
                    structure("recurrence_members", id);
                else if (type == "ordering_scope")
                    structure("ordering_scope", id);
            }
            // Server may derive successors not materialized on this device.
            for (const auto &cmd : intent.value("commands").toArray()) {
                const auto c = cmd.toObject();
                if (c.value("type") == "task.change_state" && c.value("action") == "complete")
                    for (const auto &id : c.value("task_ids").toArray()) {
                        const auto task = local.value.entities.value("task:" + id.toString())
                                              .value("value")
                                              .toObject();
                        const auto ruleId = task.value("recurrence_id").toString();
                        if (!ruleId.isEmpty()) {
                            const auto derived = stableId(ruleId + ":A:" + id.toString());
                            entityRead("task", derived);
                            structure("task_tree", derived);
                        }
                    }
            }
            QJsonArray es, ss;
            for (const auto &e : entities)
                es.append(e);
            for (const auto &s : structures)
                ss.append(s);
            QJsonObject pre{{"entities", es}, {"structures", ss}};
            QJsonObject payload{{"protocol_version", 1},
                                {"data_epoch", p.value("data_epoch")},
                                {"mutation_id", mid},
                                {"installation_id", p.value("installation_id")},
                                {"client_seq", QString::number(seq)},
                                {"preconditions", pre},
                                {"commands", intent.value("commands")}};
            if (!Schema::accepts("PushRequest", payload)) {
                if (!exec("UPDATE sync_outbox SET "
                          "state='BLOCKED_VALIDATION',safe_error_code='payload_too_large' WHERE "
                          "profile_id=? AND mutation_id=?",
                          {pid, mid}))
                    return error();
                return {QJsonObject{}, {}};
            }
            auto semantic = payload;
            semantic.insert("dataset_id", p.value("dataset_id"));
            const auto n = normalize(semantic);
            if (!n.ok())
                return {{}, n.error};
            frozen = json(payload);
            if (!exec(
                    "UPDATE sync_outbox SET preconditions_json=?,frozen_payload=?,canonical_hash=? "
                    "WHERE profile_id=? AND mutation_id=? AND frozen_payload IS NULL",
                    {QString::fromUtf8(json(pre)), frozen, digest(n.value), pid, mid}))
                return error();
        }
        if (!exec("UPDATE sync_outbox SET state='IN_FLIGHT',attempt=attempt+1 WHERE profile_id=? "
                  "AND mutation_id=?",
                  {pid, mid}))
            return error();
        return {QJsonObject{{"mutation_id", mid}, {"payload", QString::fromUtf8(frozen)}}, {}};
    }
    Result<QJsonObject> receipt(const QJsonObject &p, const QJsonObject &r) const {
        if (!Schema::accepts("Receipt", r) || r.value("data_epoch") != p.value("data_epoch"))
            return {{}, failure()};
        const auto pid = p.value("profile_id").toString(), mid = r.value("mutation_id").toString();
        qint64 seq = 0;
        if (!r.value("batch_seq").isNull() && !counter(r.value("batch_seq"), &seq))
            return {{}, failure()};
        if (!exec("INSERT INTO "
                  "sync_ack(profile_id,mutation_id,origin_epoch,batch_seq,receipt_json,confirmed_"
                  "revisions_json,confirmed_ms) VALUES(?,?,?,?,?,?,?) ON "
                  "CONFLICT(profile_id,mutation_id) DO NOTHING",
                  {pid, mid, p.value("data_epoch").toVariant(), seq ? QVariant(seq) : QVariant(),
                   QString::fromUtf8(json(r)),
                   QString::fromUtf8(QJsonDocument(r.value("entity_revisions").toArray())
                                         .toJson(QJsonDocument::Compact)),
                   QDateTime::currentMSecsSinceEpoch()}) ||
            !exec("UPDATE sync_outbox SET state='ACKNOWLEDGED',echo_applied=CASE WHEN ?=0 THEN 1 "
                  "ELSE echo_applied END WHERE profile_id=? AND mutation_id=? AND frozen_payload "
                  "IS NOT NULL",
                  {seq, pid, mid}))
            return error();
        return profile();
    }
    Result<void> saveBase(const QJsonObject &p, const QJsonArray &changes,
                          const QJsonArray &structures, const QString &mid) const {
        for (const auto &x : changes) {
            const auto e = x.toObject();
            qint64 revision;
            if (!counter(e.value("revision"), &revision) || revision < 1)
                return {failure()};
            if (!exec("INSERT INTO "
                      "sync_base(profile_id,data_epoch,entity_type,entity_id,server_revision,"
                      "canonical_snapshot,last_origin_mutation) VALUES(?,?,?,?,?,?,?) ON "
                      "CONFLICT(profile_id,data_epoch,entity_type,entity_id) DO UPDATE SET "
                      "server_revision=excluded.server_revision,canonical_snapshot=excluded."
                      "canonical_snapshot,last_origin_mutation=excluded.last_origin_mutation",
                      {p.value("profile_id").toVariant(), p.value("data_epoch").toVariant(),
                       e.value("type").toVariant(), e.value("id").toVariant(), revision,
                       QString::fromUtf8(json(e)), mid.isEmpty() ? QVariant() : QVariant(mid)}))
                return {error().error};
        }
        for (const auto &x : structures) {
            const auto s = x.toObject();
            qint64 revision;
            if (!counter(s.value("revision"), &revision) || revision < 1)
                return {failure()};
            if (!exec("INSERT INTO "
                      "sync_structures(profile_id,data_epoch,kind,entity_id,server_revision) "
                      "VALUES(?,?,?,?,?) ON CONFLICT(profile_id,data_epoch,kind,entity_id) DO "
                      "UPDATE SET server_revision=excluded.server_revision",
                      {p.value("profile_id").toVariant(), p.value("data_epoch").toVariant(),
                       s.value("kind").toVariant(), s.value("id").toVariant(), revision}))
                return {error().error};
        }
        return {};
    }
    Result<QJsonObject> batch(const QJsonObject &p, const QJsonObject &input) const {
        const auto b = input.value("batch").toObject();
        if (!Schema::accepts("ChangeBatch", b))
            return {{}, failure()};
        auto cp = checkpoint(p);
        if (!cp.ok())
            return cp;
        qint64 seq, applied;
        if (!counter(b.value("seq"), &seq) || !counter(cp.value.value("applied_seq"), &applied))
            return {{}, failure()};
        if (seq <= applied)
            return cp;
        if (seq != applied + 1 || input.value("expected_cursor") != cp.value.value("cursor"))
            return {{}, {ErrorCode::Conflict, QStringLiteral("syncCursorConflict")}};
        const auto pid = p.value("profile_id").toString(),
                   mid = b.value("origin_mutation_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT mutation_id FROM sync_outbox WHERE profile_id=? AND mutation_id=?");
        q.addBindValue(pid);
        q.addBindValue(mid);
        if (!q.exec())
            return error();
        const bool own = q.next();
        q.finish();
        const auto adopted =
            adoptIdentities(p, b.value("changes").toArray(), b.value("aliases").toArray());
        if (!adopted.ok())
            return {{}, adopted.error};
        QSet<QString> protectedKeys;
        QHash<QString, QString> mutationFor;
        q.prepare("SELECT d.entity_type,d.entity_id,d.mutation_id FROM sync_dirty_entities d JOIN "
                  "sync_outbox o ON o.profile_id=d.profile_id AND o.mutation_id=d.mutation_id "
                  "WHERE d.profile_id=? AND o.state<>'SUPERSEDED' AND o.echo_applied=0 AND (?=0 OR "
                  "o.mutation_id<>?) ORDER BY o.local_seq");
        q.addBindValue(pid);
        q.addBindValue(own ? 1 : 0);
        q.addBindValue(mid);
        if (!q.exec())
            return error();
        while (q.next()) {
            const auto k = q.value(0).toString() + ":" + q.value(1).toString();
            protectedKeys.insert(k);
            if (!mutationFor.contains(k))
                mutationFor.insert(k, q.value(2).toString());
        }
        q.finish();
        q.prepare("SELECT entity_type,entity_id,mutation_id FROM sync_conflicts WHERE profile_id=? "
                  "AND state='OPEN'");
        q.addBindValue(pid);
        if (!q.exec())
            return error();
        while (q.next()) {
            const auto k = q.value(0).toString() + ":" + q.value(1).toString();
            protectedKeys.insert(k);
            if (!mutationFor.contains(k))
                mutationFor.insert(k, q.value(2).toString());
        }
        q.finish();
        QJsonArray changes = b.value("changes").toArray();
        QSet<QString> collisionKeys = protectedKeys;
        if (!own && !protectedKeys.isEmpty()) {
            const auto current = Mapping(db).capture();
            if (!current.ok())
                return {{}, current.error};
            for (const auto &x : changes) {
                const auto e = x.toObject();
                const auto k = key(e);
                if (!protectedKeys.contains(k))
                    continue;
                const auto local = current.value.entities.value(k).value("value").toObject(),
                           remote = e.value("value").toObject();
                if (e.value("type") == "task") {
                    auto l = facts(local), r = facts(remote);
                    if (!l.value("recurrence_id").isNull() &&
                        l.value("recurrence_id") == r.value("recurrence_id") &&
                        l.value("occurrence_key") == r.value("occurrence_key")) {
                        l.remove("created_at_utc");
                        r.remove("created_at_utc");
                    }
                    if (l == r) {
                        collisionKeys.remove(k);
                        protectedKeys.remove(k);
                    }
                } else if (e.value("type") == "ordering_scope") {
                    QSqlQuery prior(db);
                    prior.prepare("SELECT canonical_snapshot FROM sync_base WHERE profile_id=? AND "
                                  "entity_type='ordering_scope' AND entity_id=?");
                    prior.addBindValue(pid);
                    prior.addBindValue(e.value("id").toString());
                    if (!prior.exec())
                        return error();
                    if (!prior.next())
                        continue;
                    const auto before = QJsonDocument::fromJson(prior.value(0).toString().toUtf8())
                                            .object()
                                            .value("value")
                                            .toObject()
                                            .value("ordered_ids")
                                            .toArray(),
                               after = remote.value("ordered_ids").toArray();
                    QJsonArray beforeCommon, afterCommon;
                    for (const auto &id : before)
                        if (after.contains(id))
                            beforeCommon.append(id);
                    for (const auto &id : after)
                        if (before.contains(id))
                            afterCommon.append(id);
                    if (beforeCommon == afterCommon)
                        collisionKeys.remove(k);
                }
            }
        }
        bool collision = false;
        for (const auto &x : changes)
            if (collisionKeys.contains(key(x.toObject())))
                collision = true;
        if (collision && !own) {
            const auto local = Mapping(db).capture();
            if (!local.ok())
                return {{}, local.error};
            const auto group = uuid();
            QString ownerMutation;
            for (const auto &x : changes)
                if (mutationFor.contains(key(x.toObject()))) {
                    ownerMutation = mutationFor.value(key(x.toObject()));
                    break;
                }
            QMap<QString, QJsonObject> candidates;
            for (const auto &x : changes)
                candidates.insert(key(x.toObject()), x.toObject());
            QSet<QString> priorGroups;
            for (const auto &x : changes) {
                const auto e = x.toObject();
                q.prepare("SELECT conflict_group_id FROM sync_conflicts WHERE profile_id=? AND "
                          "entity_type=? AND entity_id=? AND state='OPEN'");
                q.addBindValue(pid);
                q.addBindValue(e.value("type").toString());
                q.addBindValue(e.value("id").toString());
                if (!q.exec())
                    return error();
                while (q.next())
                    priorGroups.insert(q.value(0).toString());
                q.finish();
            }
            for (const auto &oldGroup : priorGroups) {
                q.prepare(
                    "SELECT c.entity_type,c.entity_id,coalesce(b.canonical_snapshot,c.remote_json) "
                    "FROM sync_conflicts c LEFT JOIN sync_base b ON b.profile_id=c.profile_id AND "
                    "b.data_epoch=c.source_epoch AND b.entity_type=c.entity_type AND "
                    "b.entity_id=c.entity_id WHERE c.profile_id=? AND c.conflict_group_id=? AND "
                    "c.state='OPEN'");
                q.addBindValue(pid);
                q.addBindValue(oldGroup);
                if (!q.exec())
                    return error();
                while (q.next()) {
                    const auto k = q.value(0).toString() + ":" + q.value(1).toString();
                    if (!candidates.contains(k))
                        candidates.insert(
                            k, QJsonDocument::fromJson(q.value(2).toString().toUtf8()).object());
                }
                q.finish();
                if (!exec("UPDATE sync_conflicts SET state='SUPERSEDED' WHERE profile_id=? AND "
                          "conflict_group_id=? AND state='OPEN'",
                          {pid, oldGroup}))
                    return error();
            }
            for (const auto &x : candidates) {
                const auto e = x;
                const auto k = key(e);
                const auto l = local.value.entities.value(k);
                QSqlQuery base(db);
                base.prepare("SELECT canonical_snapshot FROM sync_base WHERE profile_id=? AND "
                             "entity_type=? AND entity_id=?");
                base.addBindValue(pid);
                base.addBindValue(e.value("type").toString());
                base.addBindValue(e.value("id").toString());
                if (!base.exec())
                    return error();
                QVariant prior;
                if (base.next())
                    prior = base.value(0);
                qint64 revision = 0;
                counter(e.value("revision"), &revision);
                if (!exec("INSERT INTO "
                          "sync_conflicts(profile_id,conflict_id,mutation_id,source_epoch,conflict_"
                          "group_id,entity_type,entity_id,base_json,local_json,remote_json,remote_"
                          "revision,state) VALUES(?,?,?,?,?,?,?,?,?,?,?,'OPEN')",
                          {pid, uuid(), ownerMutation, p.value("data_epoch").toVariant(), group,
                           e.value("type").toVariant(), e.value("id").toVariant(), prior,
                           QString::fromUtf8(json(l)), QString::fromUtf8(json(e)), revision}))
                    return error();
            }
            if (!exec("UPDATE sync_outbox SET "
                      "state='BLOCKED_CONFLICT',safe_error_code='revision_conflict' WHERE "
                      "profile_id=? AND echo_applied=0 AND state NOT IN "
                      "('SUPERSEDED','ACKNOWLEDGED')",
                      {pid}))
                return error();
        } else {
            QJsonArray apply;
            for (const auto &x : changes)
                if (!protectedKeys.contains(key(x.toObject())))
                    apply.append(x);
            auto r = Mapping(db).apply(apply);
            if (!r.ok())
                return {{}, r.error};
        }
        auto saved = saveBase(p, changes, b.value("structure_revisions").toArray(), mid);
        if (!saved.ok())
            return {{}, saved.error};
        if (own &&
            !exec("UPDATE sync_outbox SET echo_applied=1 WHERE profile_id=? AND mutation_id=?",
                  {pid, mid}))
            return error();
        if (!exec("UPDATE sync_cursor SET "
                  "opaque_cursor=?,applied_seq=?,observed_head=max(observed_head,?) WHERE "
                  "profile_id=?",
                  {input.value("after_cursor").toVariant(), seq, seq, pid}))
            return error();
        return checkpoint(p);
    }
    Result<QJsonObject> snapshot(const QJsonObject &p, const QJsonObject &input) const {
        const auto s = input.value("snapshot").toObject();
        if (!Schema::accepts("SnapshotPayload", s) ||
            s.value("data_epoch") != p.value("data_epoch") ||
            s.value("dataset_id") != p.value("dataset_id") || p.value("phase") != "BOOTSTRAP")
            return {{}, failure()};
        const auto adopted =
            adoptIdentities(p, s.value("entities").toArray(), s.value("aliases").toArray());
        if (!adopted.ok())
            return {{}, adopted.error};
        const auto local = Mapping(db).capture();
        if (!local.ok())
            return {{}, local.error};
        QJsonArray apply;
        for (const auto &x : s.value("entities").toArray())
            if (!local.value.entities.contains(key(x.toObject())))
                apply.append(x);
        auto a = Mapping(db).apply(apply);
        if (!a.ok())
            return {{}, a.error};
        auto b = saveBase(p, s.value("entities").toArray(),
                          s.value("structure_revisions").toArray(), {});
        if (!b.ok())
            return {{}, b.error};
        qint64 cutoff;
        if (!counter(s.value("cutoff_seq"), &cutoff))
            return {{}, failure()};
        if (!exec("INSERT INTO "
                  "sync_cursor(profile_id,data_epoch,opaque_cursor,applied_seq,observed_head) "
                  "VALUES(?,?,?,?,?) ON CONFLICT(profile_id) DO UPDATE SET "
                  "opaque_cursor=excluded.opaque_cursor,applied_seq=excluded.applied_seq,observed_"
                  "head=excluded.observed_head",
                  {p.value("profile_id").toVariant(), p.value("data_epoch").toVariant(),
                   input.value("resume_cursor").toVariant(), cutoff, cutoff}))
            return error();
        return initial(p);
    }
    Result<QJsonObject> initial(const QJsonObject &p) const {
        const auto pid = p.value("profile_id").toString();
        const auto meta = bootstrap(p);
        if (!meta.ok())
            return meta;
        QSqlQuery q(db);
        q.prepare("SELECT frozen_payload FROM sync_outbox WHERE profile_id=? AND mutation_id=?");
        q.addBindValue(pid);
        q.addBindValue(meta.value.value("mutation_id").toString());
        if (!q.exec() || !q.next())
            return error();
        const auto frozen = q.value(0).toByteArray();
        q.finish();
        if (!frozen.isEmpty())
            return Schema::decode("ImportPayload", frozen);
        q.prepare("SELECT initial_payload_json FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        auto initial = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
        q.finish();
        auto cp = checkpoint(p);
        if (!cp.ok())
            return cp;
        initial.insert("base_head_seq", cp.value.value("applied_seq"));
        QJsonArray filtered, collisions;
        Graph orderBefore, orderAfter;
        for (const auto &x : initial.value("entities").toArray()) {
            const auto en = x.toObject();
            q.prepare("SELECT canonical_snapshot FROM sync_base WHERE profile_id=? AND "
                      "entity_type=? AND entity_id=?");
            q.addBindValue(pid);
            q.addBindValue(en.value("type").toString());
            q.addBindValue(en.value("id").toString());
            if (!q.exec())
                return error();
            if (!q.next()) {
                filtered.append(en);
                q.finish();
                continue;
            }
            const auto remote = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            q.finish();
            if (facts(remote.value("value").toObject()) == facts(en.value("value").toObject()) ||
                meta.value.value("resolved_initial").toBool())
                continue;
            if (en.value("type") == "ordering_scope") {
                orderBefore.entities.insert(key(remote), remote);
                orderAfter.entities.insert(key(en), en);
                orderAfter.localVersions.insert(key(en), 1);
                continue;
            }
            collisions.append(QJsonObject{{"local", en}, {"remote", remote}});
        }
        if (!collisions.isEmpty()) {
            q.prepare("SELECT count(*) FROM sync_conflicts WHERE profile_id=? AND state='OPEN'");
            q.addBindValue(pid);
            if (!q.exec() || !q.next())
                return error();
            const bool exists = q.value(0).toInt() > 0;
            q.finish();
            if (!exists) {
                const auto group = uuid();
                for (const auto &x : collisions) {
                    const auto pair = x.toObject(), local = pair.value("local").toObject(),
                               remote = pair.value("remote").toObject();
                    qint64 revision = 0;
                    counter(remote.value("revision"), &revision);
                    if (!exec("INSERT INTO "
                              "sync_conflicts(profile_id,conflict_id,mutation_id,source_epoch,"
                              "conflict_group_id,entity_type,entity_id,local_json,remote_json,"
                              "remote_revision,state) VALUES(?,?,?,?,?,?,?,?,?,?,'OPEN')",
                              {pid, uuid(), meta.value.value("mutation_id").toVariant(),
                               p.value("data_epoch").toVariant(), group,
                               remote.value("type").toVariant(), remote.value("id").toVariant(),
                               QString::fromUtf8(json(local)), QString::fromUtf8(json(remote)),
                               revision}))
                        return error();
                }
            }
            return {QJsonObject{{"needs_resolution", true}}, {}};
        }
        if (!orderAfter.entities.isEmpty() && !meta.value.value("ordering_seeded").toBool()) {
            const auto recorded = record(orderBefore, orderAfter);
            if (!recorded.ok())
                return {{}, recorded.error};
            if (!exec("UPDATE sync_profiles SET "
                      "graph_meta_json=json_set(graph_meta_json,'$.bootstrap.ordering_seeded',json("
                      "'true')) WHERE profile_id=?",
                      {pid}))
                return error();
        }
        initial.insert("entities", filtered);
        return {initial, {}};
    }
    Result<QJsonObject> bootstrap(const QJsonObject &p) const {
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT graph_meta_json FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        auto root = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
        auto meta = root.value("bootstrap").toObject();
        q.finish();
        if (!meta.isEmpty())
            return {meta, {}};
        q.prepare("SELECT next_local_seq FROM sync_profiles WHERE profile_id=?");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        const auto seq = q.value(0).toLongLong();
        q.finish();
        meta = {{"snapshot_operation_id", uuid()},
                {"import_operation_id", uuid()},
                {"mutation_id", uuid()},
                {"client_seq", QString::number(seq)}};
        root.insert("bootstrap", meta);
        if (!exec("UPDATE sync_profiles SET graph_meta_json=? WHERE profile_id=?",
                  {QString::fromUtf8(json(root)), pid}) ||
            !exec("INSERT INTO "
                  "sync_outbox(profile_id,mutation_id,origin_epoch,local_seq,state,kind,intent_"
                  "json,base_snapshot_json,not_before_ms,created_ms) "
                  "VALUES(?,?,?,?,'PENDING','IMPORT','{}','{}',0,?)",
                  {pid, meta.value("mutation_id").toVariant(), p.value("data_epoch").toVariant(),
                   seq, QDateTime::currentMSecsSinceEpoch()}) ||
            !exec("UPDATE sync_profiles SET next_local_seq=next_local_seq+1 WHERE profile_id=?",
                  {pid}))
            return error();
        return {meta, {}};
    }
    Result<QJsonObject> rebuild(const QJsonObject &p, const QJsonObject &dataset) const {
        if (!Schema::accepts("Dataset", dataset) || dataset.value("id") != p.value("dataset_id") ||
            p.value("phase") != "RECONCILE_REQUIRED")
            return {{}, failure()};
        const auto pid = p.value("profile_id").toString();
        const auto graph = Mapping(db).capture();
        if (!graph.ok())
            return {{}, graph.error};
        const QJsonObject payload{{"dto_version", 1},
                                  {"base_head_seq", QStringLiteral("0")},
                                  {"entities", graph.value.array()},
                                  {"aliases", graph.value.aliases},
                                  {"successor_edges", graph.value.edges}};
        if (!Schema::accepts("ImportPayload", payload))
            return {{}, failure(QStringLiteral("syncLegacyUnsupported"))};
        for (const auto &table :
             {QStringLiteral("sync_incoming"), QStringLiteral("sync_incoming_manifests"),
              QStringLiteral("sync_cursor"), QStringLiteral("sync_base"),
              QStringLiteral("sync_structures")})
            if (!exec("DELETE FROM " + table + " WHERE profile_id=?", {pid}))
                return error();
        if (!exec("UPDATE sync_outbox SET state='SUPERSEDED',echo_applied=1 WHERE profile_id=? AND "
                  "state<>'SUPERSEDED'",
                  {pid}) ||
            !exec(
                "UPDATE sync_conflicts SET state='SUPERSEDED' WHERE profile_id=? AND state='OPEN'",
                {pid}) ||
            !exec("UPDATE sync_profiles SET "
                  "data_epoch=?,generation=generation+1,auth_state='AUTHENTICATED',phase='"
                  "BOOTSTRAP',initial_payload_json=?,graph_meta_json=json_remove(graph_meta_json,'$"
                  ".bootstrap','$.transport','$.identity_aliases') WHERE profile_id=?",
                  {dataset.value("data_epoch").toVariant(), QString::fromUtf8(json(payload)), pid}))
            return error();
        const auto current = profile();
        if (!current.ok())
            return current;
        const auto meta = bootstrap(current.value);
        return meta.ok() ? current : meta;
    }
    Result<QJsonObject> block(const QJsonObject &p, const QString &mid) const {
        if (!exec("UPDATE sync_outbox SET "
                  "state='BLOCKED_CONFLICT',safe_error_code='revision_conflict' WHERE profile_id=? "
                  "AND mutation_id=?",
                  {p.value("profile_id").toVariant(), mid}))
            return error();
        return profile();
    }
    Result<QJsonObject> ensureConflicts(const QJsonObject &p) const {
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT count(*) FROM sync_conflicts WHERE profile_id=? AND state='OPEN'");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        if (q.value(0).toInt() > 0)
            return profile();
        q.finish();
        q.prepare("SELECT d.entity_type,d.entity_id,d.mutation_id FROM sync_dirty_entities d JOIN "
                  "sync_outbox o ON o.profile_id=d.profile_id AND o.mutation_id=d.mutation_id "
                  "WHERE d.profile_id=? AND o.state='BLOCKED_CONFLICT' ORDER BY o.local_seq");
        q.addBindValue(pid);
        if (!q.exec())
            return error();
        QList<QJsonObject> keys;
        QSet<QString> seen;
        while (q.next()) {
            const auto k = q.value(0).toString() + ":" + q.value(1).toString();
            if (seen.contains(k))
                continue;
            seen.insert(k);
            keys.append({{"type", q.value(0).toString()},
                         {"id", q.value(1).toString()},
                         {"mutation_id", q.value(2).toString()}});
        }
        q.finish();
        if (keys.isEmpty())
            return profile();
        const auto local = Mapping(db).capture();
        if (!local.ok())
            return {{}, local.error};
        const auto group = uuid();
        for (const auto &k : keys) {
            const auto l = local.value.entities.value(key(k));
            if (l.isEmpty())
                continue;
            q.prepare("SELECT canonical_snapshot,server_revision FROM sync_base WHERE profile_id=? "
                      "AND entity_type=? AND entity_id=?");
            q.addBindValue(pid);
            q.addBindValue(k.value("type").toString());
            q.addBindValue(k.value("id").toString());
            if (!q.exec())
                return error();
            QJsonObject remote = l;
            qint64 revision = 0;
            if (q.next()) {
                remote = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
                revision = q.value(1).toLongLong();
            } else
                remote.insert("revision", QStringLiteral("0"));
            q.finish();
            if (!exec("INSERT INTO "
                      "sync_conflicts(profile_id,conflict_id,mutation_id,source_epoch,conflict_"
                      "group_id,entity_type,entity_id,local_json,remote_json,remote_revision,state)"
                      " VALUES(?,?,?,?,?,?,?,?,?,?,'OPEN')",
                      {pid, uuid(), k.value("mutation_id").toVariant(),
                       p.value("data_epoch").toVariant(), group, k.value("type").toVariant(),
                       k.value("id").toVariant(), QString::fromUtf8(json(l)),
                       QString::fromUtf8(json(remote)), revision}))
                return error();
        }
        return profile();
    }
    Result<QJsonObject> resolve(const QJsonObject &p, const QJsonObject &input) const {
        if (p.value("phase") == "BOOTSTRAP")
            return resolveInitial(p, input);
        const auto decision = input.value("decision").toString();
        if (decision != "remote" && decision != "local")
            return {{}, failure()};
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT conflict_group_id FROM sync_conflicts WHERE profile_id=? AND "
                  "conflict_id=? AND state='OPEN'");
        q.addBindValue(pid);
        q.addBindValue(input.value("conflict_id").toString());
        if (!q.exec() || !q.next())
            return {{}, {ErrorCode::NotFound, QStringLiteral("syncConflictMissing")}};
        const auto group = q.value(0).toString();
        q.finish();
        q.prepare("SELECT remote_json,entity_type,entity_id,remote_revision FROM sync_conflicts "
                  "WHERE profile_id=? AND conflict_group_id=? AND state='OPEN'");
        q.addBindValue(pid);
        q.addBindValue(group);
        if (!q.exec())
            return error();
        QJsonArray remote;
        while (q.next()) {
            const auto r = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            QSqlQuery current(db);
            current.prepare("SELECT server_revision FROM sync_base WHERE profile_id=? AND "
                            "entity_type=? AND entity_id=?");
            current.addBindValue(pid);
            current.addBindValue(q.value(1));
            current.addBindValue(q.value(2));
            if (!current.exec())
                return error();
            const bool exists = current.next();
            const auto expected = q.value(3).toLongLong();
            if ((expected > 0 && (!exists || current.value(0).toLongLong() != expected)) ||
                (expected == 0 && exists))
                return {{}, {ErrorCode::Conflict, QStringLiteral("syncConflictChanged")}};
            if (expected == 0 && decision == "remote")
                return {{}, failure(QStringLiteral("syncRemoteVersionMissing"))};
            remote.append(r);
        }
        q.finish();
        auto local = Mapping(db).capture();
        if (!local.ok())
            return {{}, local.error};
        QJsonArray apply;
        for (const auto &x : remote)
            if (decision == "remote" || !local.value.entities.contains(key(x.toObject())))
                apply.append(x);
        const auto applied = Mapping(db).apply(apply);
        if (!applied.ok())
            return {{}, applied.error};
        const auto desired = Mapping(db).capture();
        if (!desired.ok())
            return {{}, desired.error};
        Graph before;
        q.prepare("SELECT canonical_snapshot FROM sync_base WHERE profile_id=? AND data_epoch=?");
        q.addBindValue(pid);
        q.addBindValue(p.value("data_epoch").toString());
        if (!q.exec())
            return error();
        while (q.next()) {
            const auto e = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            before.entities.insert(key(e), e);
            before.localVersions.insert(key(e), 1);
        }
        q.finish();
        if (!exec("UPDATE sync_outbox SET state='SUPERSEDED',echo_applied=1 WHERE profile_id=? AND "
                  "echo_applied=0 AND state NOT IN ('ACKNOWLEDGED','SUPERSEDED')",
                  {pid}) ||
            !exec("UPDATE sync_conflicts SET state='RESOLVED',decision_json=? WHERE profile_id=? "
                  "AND conflict_group_id=? AND state='OPEN'",
                  {QString::fromUtf8(json({{"choice", decision}})), pid, group}))
            return error();
        const auto recorded = recordImport(before, desired.value);
        if (!recorded.ok())
            return {{}, recorded.error};
        q.prepare("SELECT count(*) FROM sync_conflicts WHERE profile_id=? AND state='OPEN'");
        q.addBindValue(pid);
        if (!q.exec() || !q.next())
            return error();
        const bool unresolved = q.value(0).toInt() > 0;
        q.finish();
        if (unresolved && !exec("UPDATE sync_outbox SET state='BLOCKED_CONFLICT' WHERE "
                                "profile_id=? AND state='PENDING'",
                                {pid}))
            return error();
        return profile();
    }
    Result<QJsonObject> resolveInitial(const QJsonObject &p, const QJsonObject &input) const {
        const auto decision = input.value("decision").toString();
        if (decision != "local" && decision != "remote")
            return {{}, failure()};
        const auto pid = p.value("profile_id").toString();
        QSqlQuery q(db);
        q.prepare("SELECT remote_json FROM sync_conflicts WHERE profile_id=? AND state='OPEN'");
        q.addBindValue(pid);
        if (!q.exec())
            return error();
        QJsonArray remote;
        while (q.next())
            remote.append(QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object());
        q.finish();
        if (remote.isEmpty())
            return {{}, failure()};
        if (decision == "remote") {
            const auto applied = Mapping(db).apply(remote);
            if (!applied.ok())
                return {{}, applied.error};
        }
        const auto desired = Mapping(db).capture();
        if (!desired.ok())
            return {{}, desired.error};
        Graph before = desired.value;
        q.prepare("SELECT canonical_snapshot FROM sync_base WHERE profile_id=? AND data_epoch=?");
        q.addBindValue(pid);
        q.addBindValue(p.value("data_epoch").toString());
        if (!q.exec())
            return error();
        while (q.next()) {
            const auto e = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            before.entities.insert(key(e), e);
        }
        q.finish();
        if (!exec("UPDATE sync_outbox SET state='SUPERSEDED',echo_applied=1 WHERE profile_id=? AND "
                  "kind='COMMANDS' AND state<>'SUPERSEDED'",
                  {pid}))
            return error();
        const auto recorded = record(before, desired.value);
        if (!recorded.ok())
            return {{}, recorded.error};
        QJsonObject payload{{"dto_version", 1},
                            {"base_head_seq", QStringLiteral("0")},
                            {"entities", desired.value.array()},
                            {"aliases", desired.value.aliases},
                            {"successor_edges", desired.value.edges}};
        if (!exec("UPDATE sync_profiles SET "
                  "initial_payload_json=?,graph_meta_json=json_set(graph_meta_json,'$.bootstrap."
                  "resolved_initial',json('true')) WHERE profile_id=?",
                  {QString::fromUtf8(json(payload)), pid}) ||
            !exec("UPDATE sync_conflicts SET state='RESOLVED',decision_json=? WHERE profile_id=? "
                  "AND state='OPEN'",
                  {QString::fromUtf8(json({{"choice", decision}})), pid}))
            return error();
        return profile();
    }
};
} // namespace daymark::sync
