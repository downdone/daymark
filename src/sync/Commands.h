#pragma once
#include "Mapping.h"

namespace daymark::sync {
struct CommandDelta {
    QJsonArray commands;
    QStringList dirty;
    bool unsupported = false;
};
inline QJsonObject facts(QJsonObject value) {
    value.remove("updated_at_utc");
    return value;
}
// Derive only the closed command union. A non-representable legacy change is
// retained in a blocked journal entry; it never becomes a generic remote upsert.
inline CommandDelta commandsBetween(const Graph &before, const Graph &after) {
    CommandDelta out;
    QJsonArray creates, patches, states, rules, orders, categories, tags;
    QSet<QString> consumedState;
    for (auto it = after.entities.begin(); it != after.entities.end(); ++it) {
        const auto e = it.value(), old = before.entities.value(it.key());
        const auto type = e.value("type").toString(), id = e.value("id").toString();
        const auto v = e.value("value").toObject(), was = old.value("value").toObject();
        if (!old.isEmpty() && facts(v) == facts(was))
            continue;
        out.dirty.append(it.key());
        const auto at = v.value("updated_at_utc").toString(now());
        if (type == "category") {
            if (!old.isEmpty() && v.value("legacy_metadata") != was.value("legacy_metadata"))
                out.unsupported = true;
            auto c =
                event(v.value("deleted_at_utc").isNull() ? "category.save" : "category.remove", at);
            c.insert("category_id", id);
            if (v.value("deleted_at_utc").isNull()) {
                for (const auto &f : {"title", "description", "color_key", "archived"})
                    c.insert(QLatin1String(f), v.value(QLatin1String(f)));
                if (old.isEmpty())
                    c.insert("parent_id", v.value("parent_id"));
                else if (v.value("parent_id") != was.value("parent_id") &&
                         (!after.entities.contains("category:" +
                                                   was.value("parent_id").toString()) ||
                          after.entities.value("category:" + was.value("parent_id").toString())
                              .value("value")
                              .toObject()
                              .value("deleted_at_utc")
                              .isNull())) {
                    auto relocate = event("category.relocate", at);
                    relocate.insert("category_id", id);
                    if (v.value("parent_id").isNull()) {
                        const auto scope =
                            after.entities
                                .value("ordering_scope:" + orderingId("category_siblings"))
                                .value("value")
                                .toObject()
                                .value("ordered_ids")
                                .toArray();
                        QString target;
                        int here = -1, there = -1;
                        for (int n = 0; n < scope.size(); ++n) {
                            const auto candidate = scope[n].toString();
                            if (candidate == id) {
                                here = n;
                                continue;
                            }
                            const auto cat = after.entities.value("category:" + candidate)
                                                 .value("value")
                                                 .toObject();
                            if (target.isEmpty() && cat.value("deleted_at_utc").isNull()) {
                                target = candidate;
                                there = n;
                            }
                        }
                        if (target.isEmpty())
                            out.unsupported = true;
                        else {
                            relocate.insert("target_id", target);
                            relocate.insert("placement", here > there ? QStringLiteral("after")
                                                                      : QStringLiteral("before"));
                            categories.append(relocate);
                        }
                    } else {
                        relocate.insert("target_id", v.value("parent_id"));
                        relocate.insert("placement", QStringLiteral("inside"));
                        categories.append(relocate);
                    }
                }
            }
            categories.append(c);
        } else if (type == "tag") {
            auto c = event(v.value("deleted_at_utc").isNull() ? "tag.save" : "tag.remove", at);
            c.insert("tag_id", id);
            if (v.value("deleted_at_utc").isNull()) {
                c.insert("title", v.value("title"));
                c.insert("color_key", v.value("color_key"));
            }
            tags.append(c);
        } else if (type == "recurrence_rule") {
            if (old.isEmpty()) {
                if (v.value("source_task_id").isNull()) {
                    out.unsupported = true;
                    continue;
                }
                auto c = event("recurrence.replace", at);
                c.insert("source_task_id", v.value("source_task_id"));
                c.insert("new_rule_id", id);
                c.insert("rule", v.value("rule"));
                c.insert("cutoff_on",
                         after.ruleCutoffs.value(
                             id, v.value("rule").toObject().value("anchor_on").toString()));
                rules.append(c);
            } else if (was.value("rule") != v.value("rule")) {
                out.unsupported = true;
            } else if (!was.value("active").toBool() && v.value("active").toBool()) {
                if (v.value("source_task_id").isNull())
                    out.unsupported = true;
                else {
                    auto c = event("recurrence.replace", at);
                    c.insert("source_task_id", v.value("source_task_id"));
                    c.insert("new_rule_id", id);
                    c.insert("rule", v.value("rule"));
                    c.insert("cutoff_on", QDate::currentDate().addDays(1).toString(Qt::ISODate));
                    rules.append(c);
                }
            } else if (was.value("active").toBool() && !v.value("active").toBool()) {
                auto c = event("recurrence.stop", at);
                c.insert("rule_id", id);
                c.insert("remove_future", true);
                c.insert("cutoff_on", QDate::currentDate().addDays(1).toString(Qt::ISODate));
                rules.append(c);
            }
        } else if (type == "ordering_scope") {
            auto c = event("ordering.replace", now());
            c.insert("scope_id", id);
            c.insert("kind", v.value("kind"));
            c.insert("subject_id", v.value("subject_id"));
            c.insert("member_ids", v.value("ordered_ids"));
            c.insert("ordered_ids", v.value("ordered_ids"));
            orders.append(c);
        } else if (type == "task") {
            if (old.isEmpty()) {
                if (!v.value("recurrence_id").isNull()) {
                    const auto rule =
                        after.entities
                            .value("recurrence_rule:" + v.value("recurrence_id").toString())
                            .value("value")
                            .toObject();
                    if (rule.value("source_task_id") != id) {
                        if (v.value("occurrence_key").toString().startsWith("D:")) {
                            auto c = event("recurrence.materialize", at);
                            c.insert("rule_id", v.value("recurrence_id"));
                            c.insert("from_on", v.value("occurrence_on"));
                            c.insert("through_on", v.value("occurrence_on"));
                            rules.append(c);
                        } else
                            out.dirty.removeAll(it.key());
                        continue;
                    }
                }
                auto c = event("task.create", v.value("created_at_utc").toString(at));
                c.insert("task_id", id);
                c.insert("title", v.value("title"));
                auto initial = patchFields(v);
                initial.remove("title");
                c.insert("initial", initial);
                creates.append(c);
                if (v.value("status") != "OPEN" || !v.value("deleted_at_utc").isNull())
                    out.unsupported = true;
            } else {
                if (v.value("legacy_metadata") != was.value("legacy_metadata"))
                    out.unsupported = true;
                if (v.value("category_id") != was.value("category_id") ||
                    v.value("parent_task_id") != was.value("parent_task_id")) {
                    auto c = event("task.move_tree", at);
                    c.insert("task_id", id);
                    c.insert("category_id", v.value("category_id"));
                    c.insert("parent_task_id", v.value("parent_task_id"));
                    patches.append(c);
                }
                QJsonObject changed;
                const auto patch = patchFields(v), oldPatch = patchFields(was);
                for (auto p = patch.begin(); p != patch.end(); ++p)
                    if (p.key() != "category_id" && p.key() != "parent_task_id" &&
                        oldPatch.value(p.key()) != p.value())
                        changed.insert(p.key(), p.value());
                if (!changed.isEmpty()) {
                    auto c = event("task.patch", at);
                    c.insert("task_id", id);
                    c.insert("patch", changed);
                    patches.append(c);
                }
                QString action, group;
                if (was.value("deleted_at_utc").isNull() && !v.value("deleted_at_utc").isNull()) {
                    action = "delete";
                    group = v.value("deletion_group_id").toString();
                } else if (!was.value("deleted_at_utc").isNull() &&
                           v.value("deleted_at_utc").isNull()) {
                    action = "restore";
                    group = was.value("deletion_group_id").toString();
                } else if (v.value("status") != was.value("status")) {
                    action = v.value("status") == "COMPLETED"   ? "complete"
                             : v.value("status") == "CANCELLED" ? "cancel"
                                                                : "reopen";
                    group = (action == "reopen" ? was : v).value("completion_group_id").toString();
                }
                if (!action.isEmpty()) {
                    const auto groupKey = action + ":" + group;
                    if (!group.isEmpty() && consumedState.contains(groupKey))
                        continue;
                    // Roots precede their descendants so the persisted cohort is
                    // reproduced by one server operation, not multiple groups.
                    bool inherited = false;
                    const auto parent = v.value("parent_task_id").toString();
                    if (!parent.isEmpty()) {
                        const auto pv =
                            after.entities.value("task:" + parent).value("value").toObject();
                        const auto field = (action == "delete" || action == "restore")
                                               ? QStringLiteral("deletion_group_id")
                                               : QStringLiteral("completion_group_id");
                        const auto pp =
                            action == "restore"
                                ? before.entities.value("task:" + parent).value("value").toObject()
                                : pv;
                        if (!group.isEmpty() && pp.value(field) == group)
                            inherited = true;
                    }
                    if (!inherited) {
                        auto c = event("task.change_state", at);
                        c.insert("task_ids", QJsonArray{id});
                        c.insert("action", action);
                        c.insert("group_id", group.isEmpty() ? uuid() : group);
                        states.append(c);
                        if (!group.isEmpty())
                            consumedState.insert(groupKey);
                    }
                }
            }
        }
    }
    for (auto it = before.entities.begin(); it != before.entities.end(); ++it)
        if (!after.entities.contains(it.key())) {
            out.dirty.append(it.key());
            out.unsupported = true;
        }
    // Place parent creations first; links to other newly created tasks are
    // patched after all creates, so command order never relies on UUID sorting.
    QList<QJsonObject> waiting;
    for (const auto &c : creates)
        waiting.append(c.toObject());
    QSet<QString> created;
    while (!waiting.isEmpty()) {
        bool progress = false;
        for (int i = 0; i < waiting.size();) {
            auto c = waiting[i], initial = c.value("initial").toObject();
            const auto parent = initial.value("parent_task_id").toString();
            if (!parent.isEmpty() && !before.entities.contains("task:" + parent) &&
                !created.contains(parent)) {
                ++i;
                continue;
            }
            const auto links = initial.value("links").toArray();
            if (!links.isEmpty()) {
                initial.insert("links", QJsonArray{});
                auto p = event("task.patch", c.value("event_at_utc").toString());
                p.insert("task_id", c.value("task_id"));
                p.insert("patch", QJsonObject{{"links", links}});
                patches.append(p);
                c.insert("initial", initial);
            }
            out.commands.append(c);
            created.insert(c.value("task_id").toString());
            waiting.removeAt(i);
            progress = true;
        }
        if (!progress) {
            out.unsupported = true;
            break;
        }
    }
    QJsonArray earlyStates, lateStates, categorySaves, categoryMoves, categoryRemoves,
        sourcePatches, otherPatches;
    QSet<QString> sources;
    for (const auto &r : rules)
        if (r.toObject().value("type") == "recurrence.replace")
            sources.insert(r.toObject().value("source_task_id").toString());
    for (const auto &p : patches) {
        if (sources.contains(p.toObject().value("task_id").toString()) &&
            p.toObject().value("type") == "task.patch")
            sourcePatches.append(p);
        else
            otherPatches.append(p);
    }
    for (const auto &c : categories) {
        const auto t = c.toObject().value("type");
        if (t == "category.save")
            categorySaves.append(c);
        else if (t == "category.relocate")
            categoryMoves.append(c);
        else
            categoryRemoves.append(c);
    }
    for (const auto &c : states) {
        const auto action = c.toObject().value("action");
        if (action == "restore" || action == "reopen")
            earlyStates.append(c);
        else
            lateStates.append(c);
    }
    QJsonArray result;
    for (const auto &a : {categorySaves, categoryMoves, categoryRemoves, tags, out.commands,
                          earlyStates, sourcePatches, rules, otherPatches, lateStates, orders})
        for (const auto &c : a)
            result.append(c);
    out.commands = result;
    for (const auto &c : out.commands)
        if (!Schema::accepts("Command", c))
            out.unsupported = true;
    return out;
}
} // namespace daymark::sync
