#pragma once
#include "Graph.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QQueue>
namespace daymark::sync {
// Physical SQLite identities remain stable for the editor and local history.
// This adapter translates recurrence aliases at the replication boundary.
class Identity final {
  public:
    QHash<QString, QString> aliases, bindings, occurrences, sources, knownKeys;
    QSet<QString> known;
    static Result<Identity> load(QSqlDatabase db, const QJsonArray &overlay = {}) {
        Identity index;
        QSqlQuery q(db);
        if (!q.exec("SELECT graph_meta_json FROM sync_profiles LIMIT 1"))
            return {{}, {ErrorCode::PersistenceError, QStringLiteral("databaseRead")}};
        if (q.next()) {
            const auto root = QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object();
            for (const auto &x : root.value("identity_aliases").toArray()) {
                const auto a = x.toObject();
                index.aliases.insert(a.value("alias_id").toString(),
                                     a.value("canonical_id").toString());
            }
            const auto b = root.value("physical_task_ids").toObject();
            for (auto it = b.begin(); it != b.end(); ++it)
                index.bindings.insert(it.key(), it.value().toString());
        }
        q.finish();
        if (!q.exec("SELECT b.canonical_snapshot FROM sync_base b JOIN sync_profiles p ON "
                    "p.profile_id=b.profile_id AND p.data_epoch=b.data_epoch WHERE b.entity_type "
                    "IN ('task','recurrence_rule')"))
            return {{}, {ErrorCode::PersistenceError, QStringLiteral("databaseRead")}};
        while (q.next())
            index.add(QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object());
        for (const auto &x : overlay)
            index.add(x.toObject());
        return {index, {}};
    }
    QString resolve(QString id) const {
        QSet<QString> seen;
        for (int i = 0; i < 64 && aliases.contains(id); ++i) {
            if (seen.contains(id))
                return {};
            seen.insert(id);
            id = aliases.value(id);
        }
        return aliases.contains(id) ? QString() : id;
    }
    static QJsonValue mapId(const QJsonValue &value, const QHash<QString, QString> &ids) {
        if (value.isNull() || value.isUndefined())
            return value;
        const auto s = value.toString();
        return ids.value(s, s);
    }
    static QJsonArray mapLinks(QJsonArray links, const QHash<QString, QString> &ids) {
        for (int i = 0; i < links.size(); ++i) {
            auto l = links[i].toObject();
            l.insert("target_id", mapId(l.value("target_id"), ids));
            links[i] = l;
        }
        return links;
    }
    static QJsonObject entityTo(const QJsonObject &input, const QHash<QString, QString> &ids) {
        auto e = input, v = e.value("value").toObject();
        const auto type = e.value("type").toString();
        if (type == "task") {
            v.insert("id", mapId(v.value("id"), ids));
            v.insert("parent_task_id", mapId(v.value("parent_task_id"), ids));
            v.insert("links", mapLinks(v.value("links").toArray(), ids));
            const auto occurrence = v.value("occurrence_key").toString();
            if (occurrence.startsWith("A:"))
                v.insert("occurrence_key",
                         QStringLiteral("A:") + ids.value(occurrence.mid(2), occurrence.mid(2)));
            e.insert("id", v.value("id"));
        } else if (type == "recurrence_rule") {
            v.insert("source_task_id", mapId(v.value("source_task_id"), ids));
            auto rule = v.value("rule").toObject(), t = rule.value("defaults").toObject();
            t.insert("parent_task_id", mapId(t.value("parent_task_id"), ids));
            t.insert("links", mapLinks(t.value("links").toArray(), ids));
            rule.insert("defaults", t);
            v.insert("rule", rule);
        } else if (type == "ordering_scope") {
            const auto kind = v.value("kind").toString();
            if (kind != "category_siblings") {
                v.insert("subject_id", mapId(v.value("subject_id"), ids));
                auto members = v.value("ordered_ids").toArray();
                for (int i = 0; i < members.size(); ++i)
                    members[i] = mapId(members[i], ids);
                v.insert("ordered_ids", members);
                const auto scope = orderingId(kind, v.value("subject_id").toString());
                v.insert("id", scope);
                e.insert("id", scope);
            }
        }
        e.insert("value", v);
        return e;
    }
    static QJsonObject documentTo(QJsonObject doc, const QHash<QString, QString> &ids,
                                  const QHash<QString, QString> &scopes) {
        auto taskFields = [&](QJsonObject value) {
            if (value.contains("parent_task_id"))
                value.insert("parent_task_id", mapId(value.value("parent_task_id"), ids));
            if (value.contains("links"))
                value.insert("links", mapLinks(value.value("links").toArray(), ids));
            return value;
        };
        if (doc.value("type") == "task" || doc.value("type") == "ordering_scope" ||
            doc.value("type") == "recurrence_rule")
            return entityTo(doc, ids);
        if (doc.contains("entities")) {
            QJsonArray result;
            for (const auto &x : doc.value("entities").toArray())
                result.append(entityTo(x.toObject(), ids));
            doc.insert("entities", result);
        }
        if (doc.contains("aliases")) {
            auto values = doc.value("aliases").toArray();
            for (int i = 0; i < values.size(); ++i) {
                auto a = values[i].toObject();
                a.insert("canonical_id", mapId(a.value("canonical_id"), ids));
                values[i] = a;
            }
            doc.insert("aliases", values);
        }
        if (doc.contains("successor_edges")) {
            auto values = doc.value("successor_edges").toArray();
            for (int i = 0; i < values.size(); ++i) {
                auto edge = values[i].toObject();
                for (const auto &f : {"predecessor_id", "successor_id"})
                    edge.insert(QLatin1String(f), mapId(edge.value(QLatin1String(f)), ids));
                values[i] = edge;
            }
            doc.insert("successor_edges", values);
        }
        if (doc.contains("commands")) {
            auto values = doc.value("commands").toArray();
            for (int i = 0; i < values.size(); ++i) {
                auto c = values[i].toObject();
                const auto type = c.value("type").toString();
                if (type.startsWith("task.")) {
                    if (c.contains("task_id"))
                        c.insert("task_id", mapId(c.value("task_id"), ids));
                    if (c.contains("task_ids")) {
                        auto a = c.value("task_ids").toArray();
                        for (int j = 0; j < a.size(); ++j)
                            a[j] = mapId(a[j], ids);
                        c.insert("task_ids", a);
                    }
                    if (c.contains("parent_task_id"))
                        c.insert("parent_task_id", mapId(c.value("parent_task_id"), ids));
                    for (const auto &f : {"patch", "initial"})
                        if (c.contains(QLatin1String(f)))
                            c.insert(QLatin1String(f),
                                     taskFields(c.value(QLatin1String(f)).toObject()));
                } else if (type == "recurrence.replace") {
                    c.insert("source_task_id", mapId(c.value("source_task_id"), ids));
                    auto rule = c.value("rule").toObject();
                    rule.insert("defaults", taskFields(rule.value("defaults").toObject()));
                    c.insert("rule", rule);
                } else if (type == "ordering.replace" && c.value("kind") != "category_siblings") {
                    c.insert("subject_id", mapId(c.value("subject_id"), ids));
                    for (const auto &f : {"member_ids", "ordered_ids"}) {
                        auto a = c.value(QLatin1String(f)).toArray();
                        for (int j = 0; j < a.size(); ++j)
                            a[j] = mapId(a[j], ids);
                        c.insert(QLatin1String(f), a);
                    }
                    c.insert("scope_id", orderingId(c.value("kind").toString(),
                                                    c.value("subject_id").toString()));
                }
                values[i] = c;
            }
            doc.insert("commands", values);
        }
        QJsonObject result;
        for (auto it = doc.begin(); it != doc.end(); ++it) {
            auto name = it.key();
            auto value = it.value();
            if (name.startsWith("task:")) {
                name = QStringLiteral("task:") + ids.value(name.mid(5), name.mid(5));
                value = entityTo(value.toObject(), ids);
            } else if (name.startsWith("ordering_scope:")) {
                name = QStringLiteral("ordering_scope:") + scopes.value(name.mid(15), name.mid(15));
                value = entityTo(value.toObject(), ids);
            }
            result.insert(name, value);
        }
        return result;
    }
    Result<Graph> canonicalize(Graph graph) const {
        QHash<QString, QJsonObject> nodes;
        QSet<QString> sourceIds;
        for (const auto &e : graph.entities)
            if (e.value("type") == "recurrence_rule") {
                const auto s = e.value("value").toObject().value("source_task_id").toString();
                if (!s.isEmpty())
                    sourceIds.insert(s);
            } else if (e.value("type") == "task")
                nodes.insert(e.value("id").toString(), e.value("value").toObject());
        QHash<QString, QString> mapping, ready;
        QHash<QString, QStringList> children;
        QQueue<QString> queue;
        QSet<QString> queued;
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            const auto occurrence = it.value().value("occurrence_key").toString();
            if (!occurrence.startsWith("A:")) {
                queue.enqueue(it.key());
                queued.insert(it.key());
            } else {
                const auto raw = occurrence.mid(2);
                const auto dependency = nodes.contains(raw) ? raw : resolve(raw);
                children[dependency].append(it.key());
                if (!nodes.contains(raw) && known.contains(dependency)) {
                    ready.insert(dependency, dependency);
                    queue.enqueue(it.key());
                    queued.insert(it.key());
                }
            }
        }
        QHash<QString, QString> inverse;
        while (!queue.isEmpty()) {
            const auto native = queue.dequeue();
            const auto task = nodes.value(native);
            const auto rule = task.value("recurrence_id").toString();
            const auto rawKey = task.value("occurrence_key").toString();
            QString canonicalKey = rawKey;
            if (rawKey.startsWith("A:")) {
                const auto raw = rawKey.mid(2);
                const auto dependency = nodes.contains(raw) ? raw : resolve(raw);
                if (!ready.contains(dependency)) {
                    queued.remove(native);
                    continue;
                }
                canonicalKey = QStringLiteral("A:") + ready.value(dependency);
            }
            QString canonical = resolve(bindings.value(native, native));
            if (canonical.isEmpty())
                return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
            if (!rule.isEmpty() && !canonicalKey.isEmpty()) {
                const auto logical = rule + QLatin1Char('/') + canonicalKey;
                const auto match = occurrences.value(logical);
                const bool source = sourceIds.contains(native);
                if (source) {
                    const auto expectedSource = sources.value(rule);
                    if (!expectedSource.isEmpty() && canonical != expectedSource)
                        return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
                } else if (!match.isEmpty()) {
                    if (known.contains(native) && knownKeys.value(native) != logical)
                        return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
                    canonical = match;
                } else if (native == stableId(rule + QLatin1Char(':') + rawKey))
                    canonical = resolve(stableId(rule + QLatin1Char(':') + canonicalKey));
            }
            if (canonical.isEmpty() ||
                (inverse.contains(canonical) && inverse.value(canonical) != native))
                return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
            mapping.insert(native, canonical);
            inverse.insert(canonical, native);
            ready.insert(native, canonical);
            ready.insert(canonical, canonical);
            for (const auto &dependency : {native, canonical})
                for (const auto &child : children.value(dependency))
                    if (!queued.contains(child)) {
                        queued.insert(child);
                        queue.enqueue(child);
                    }
        }
        if (mapping.size() != nodes.size())
            return {{}, failure(QStringLiteral("syncLegacyRecurrenceUnsupported"))};
        Graph result;
        result.ruleCutoffs = graph.ruleCutoffs;
        result.taskWireIds = mapping;
        result.taskLocalIds = inverse;
        for (auto it = graph.entities.begin(); it != graph.entities.end(); ++it) {
            const auto e = entityTo(it.value(), mapping);
            const auto k = key(e);
            if (result.entities.contains(k)) {
                if (e.value("type") != "ordering_scope")
                    return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
                if (e.value("value").toObject().value("ordered_ids").toArray().isEmpty())
                    continue;
            }
            result.entities.insert(k, e);
            result.localVersions.insert(k, graph.localVersions.value(it.key(), 1));
            if (e.value("type") == "ordering_scope")
                result.scopeWireIds.insert(it.value().value("id").toString(),
                                           e.value("id").toString());
        }
        QMap<QString, QString> outgoing;
        for (auto it = aliases.begin(); it != aliases.end(); ++it) {
            const auto target = resolve(it.value());
            if (target != it.key() && result.entities.contains("task:" + target))
                outgoing.insert(it.key(), target);
        }
        for (const auto &e : result.entities)
            if (e.value("type") == "task") {
                const auto t = e.value("value").toObject();
                const auto rule = t.value("recurrence_id").toString(),
                           occurrence = t.value("occurrence_key").toString();
                if (rule.isEmpty() || occurrence.isEmpty())
                    continue;
                const auto source = result.entities.value("recurrence_rule:" + rule)
                                        .value("value")
                                        .toObject()
                                        .value("source_task_id");
                const auto expected = stableId(rule + QLatin1Char(':') + occurrence);
                if (expected != t.value("id").toString() && source != t.value("id"))
                    outgoing.insert(expected, t.value("id").toString());
            }
        for (auto it = outgoing.begin(); it != outgoing.end(); ++it) {
            if (result.entities.contains("task:" + it.key()))
                return {{}, failure(QStringLiteral("syncLegacyIdentityAmbiguous"))};
            result.aliases.append(
                QJsonObject{{"alias_id", it.key()}, {"canonical_id", it.value()}});
        }
        for (const auto &x : graph.edges) {
            auto edge = x.toObject();
            edge.insert("predecessor_id", mapId(edge.value("predecessor_id"), mapping));
            edge.insert("successor_id", mapId(edge.value("successor_id"), mapping));
            result.edges.append(edge);
        }
        return {result, {}};
    }

  private:
    void add(const QJsonObject &e) {
        const auto v = e.value("value").toObject();
        const auto id = e.value("id").toString();
        if (e.value("type") == "task") {
            known.insert(id);
            const auto rule = v.value("recurrence_id").toString(),
                       occurrence = v.value("occurrence_key").toString();
            if (!rule.isEmpty() && !occurrence.isEmpty()) {
                const auto logical = rule + QLatin1Char('/') + occurrence;
                occurrences.insert(logical, id);
                knownKeys.insert(id, logical);
            }
        } else if (e.value("type") == "recurrence_rule")
            sources.insert(id, v.value("source_task_id").toString());
    }
};
} // namespace daymark::sync
