// Real Qt coordinator <-> Go TLS integration, included by TaskWorkspaceTest.
// The Go harness supplies an isolated CA and synthetic accounts.
#include "sync/Coordinator.h"
#include "persistence/DataTransfer.h"
#include "persistence/ScopedTransaction.h"
#include "sync/Mapping.h"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QThread>
#include <QSqlQuery>
#include <iostream>
namespace daymark::test::sync_account {
class MemoryCredentials final : public SyncCredentialStore {
  public:
    QHash<QString, QByteArray> values;
    Result<QByteArray> read(const QString &id) override {
        return {values.value(id), {}};
    }
    Result<void> write(const QString &id, const QByteArray &bytes) override {
        values.insert(id, bytes);
        return {};
    }
    Result<void> remove(const QString &id) override {
        values.remove(id);
        return {};
    }
};
static bool until(const std::function<bool()> &condition, int timeout = 30000) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeout) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        if (condition())
            return true;
        QThread::msleep(5);
    }
    return false;
}
static bool idle(TaskWorkspace &w) {
    return until([&] { return w.ready() && !w.busy(); });
}
static bool synced(TaskWorkspace &w) {
    return until([&] {
        const auto s = w.syncState();
        return s.value("authenticated").toBool() && !s.value("busy").toBool() &&
               s.value("phase") == "ACTIVE" && s.value("pending_count").toInt() == 0;
    });
}
static bool settle(TaskWorkspace &w) {
    w.syncNow();
    return synced(w);
}
static void inspect(TaskWorkspace *w) {
    if (!w)
        return;
    const auto name = sync::uuid();
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(w->databasePath());
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec(
                    "SELECT local_seq,state,echo_applied,(SELECT count(*) FROM sync_dirty_entities "
                    "d WHERE d.mutation_id=o.mutation_id),safe_error_code FROM sync_outbox o ORDER "
                    "BY local_seq"))
                while (q.next())
                    std::cerr << " outbox=" << q.value(0).toLongLong() << ","
                              << q.value(1).toString().toStdString() << "," << q.value(2).toInt()
                              << ",dirty=" << q.value(3).toInt()
                              << ",safe=" << q.value(4).toString().toStdString();
            if (q.exec("SELECT applied_seq,observed_head FROM sync_cursor"))
                while (q.next())
                    std::cerr << " cursor=" << q.value(0).toLongLong() << ","
                              << q.value(1).toLongLong();
            if (q.exec("SELECT intent_json,base_snapshot_json,after_snapshot_json FROM sync_outbox "
                       "WHERE state='BLOCKED_VALIDATION'"))
                while (q.next())
                    std::cerr << " synthetic-blocked=" << q.value(0).toString().toStdString()
                              << " before=" << q.value(1).toString().toStdString()
                              << " after=" << q.value(2).toString().toStdString();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(name);
}
static int failed(const char *step, TaskWorkspace *a = nullptr, TaskWorkspace *b = nullptr) {
    std::cerr << "FAIL " << step;
    if (a)
        std::cerr << " A=" << a->syncState().value("status").toString().toStdString()
                  << " code=" << a->syncState().value("safe_error_code").toString().toStdString()
                  << " local=" << a->errorText().toStdString();
    inspect(a);
    if (b)
        std::cerr << " B=" << b->syncState().value("status").toString().toStdString()
                  << " code=" << b->syncState().value("safe_error_code").toString().toStdString();
    inspect(b);
    std::cerr << std::endl;
    return 1;
}
int run() {
    const auto url = qEnvironmentVariable("DAYMARK_SYNC_E2E_URL"),
               password = qEnvironmentVariable("DAYMARK_SYNC_E2E_PASSWORD");
    if (url.isEmpty() || password.isEmpty())
        return 2;
    const auto login=qEnvironmentVariable("DAYMARK_SYNC_E2E_LOGIN",QStringLiteral("synthetic@example.invalid"));
    const bool external=qEnvironmentVariable("DAYMARK_SYNC_E2E_MODE")==QStringLiteral("external");
    const auto caPath=qEnvironmentVariable("DAYMARK_SYNC_E2E_CA");
    QList<QSslCertificate> certs;
    if(!caPath.isEmpty()) {
        QFile ca(caPath);
        if(!ca.open(QIODevice::ReadOnly))return failed("CA file");
        certs=QSslCertificate::fromData(ca.readAll());
        if(certs.size()!=1)return failed("CA parse");
    } else if(!external) {
        return failed("CA file");
    }
    auto tls = QSslConfiguration::defaultConfiguration();
    tls.addCaCertificates(certs);
    tls.setPeerVerifyMode(QSslSocket::VerifyPeer);
    // This exercises the actual OS store with a unique synthetic credential,
    // separately from the deterministic coordinator credential test double.
    SystemSyncCredentials system;
    const auto credentialId = QStringLiteral("test-") + sync::uuid();
    const auto synthetic = QByteArrayLiteral("synthetic-test-only");
    if (!system.write(credentialId, synthetic).ok() ||
        system.read(credentialId).value != synthetic || !system.remove(credentialId).ok() ||
        !system.read(credentialId).value.isEmpty())
        return failed("native credential write/read/delete");
    QTemporaryDir temporary;
    if (!temporary.isValid())
        return failed("temporary directory");
    const auto dirA = temporary.filePath("a"), dirB = temporary.filePath("b");
    QDir().mkpath(dirA);
    QDir().mkpath(dirB);
    MemoryCredentials credentialsA, credentialsB;
    TaskWorkspace a, b;
    a.setInteractionActive(true);
    b.setInteractionActive(true);
    a.start(dirA + "/daymark.sqlite");
    b.start(dirB + "/daymark.sqlite");
    if (!idle(a) || !idle(b))
        return failed("workspace load");
    if (!a.createTask("合成离线任务") || !idle(a))
        return failed("offline task", &a);
    const auto id = a.tasks()->taskIdAt(0);
    if (id.isEmpty())
        return failed("offline identity");
    sync::Coordinator coordinatorA(a, dirA, credentialsA), coordinatorB(b, dirB, credentialsB);
    coordinatorA.setTlsConfiguration(tls);
    coordinatorB.setTlsConfiguration(tls);
    if (!until([&] { return a.syncState().contains("mode") && b.syncState().contains("mode"); }))
        return failed("coordinator init", &a, &b);
    a.syncRegister(url, login, password);
    if (!settle(a))
        return failed("first import and own echo", &a, &b);
    b.syncRegister(url, login, password);
    if (!settle(b) || b.task(id).value("title") != "合成离线任务")
        return failed("second device snapshot", &a, &b);
    auto t = a.task(id);
    if (!a.saveTask(id, t.value("rowVersion").toLongLong(), "第一设备修改", "", "ACTIVE", "", "",
                    false) ||
        !idle(a) || !settle(a))
        return failed("capture and push", &a, &b);
    b.syncNow();
    if (!settle(b) || b.task(id).value("title") != "第一设备修改")
        return failed("incremental pull", &a, &b);
    int requests = 0;
    QObject::connect(coordinatorA.findChild<QNetworkAccessManager *>(),
                     &QNetworkAccessManager::finished, QCoreApplication::instance(),
                     [&](QNetworkReply *) { ++requests; });
    a.syncLogout();
    if (!until([&] {
            return !a.syncState().value("busy").toBool() &&
                   !a.syncState().value("authenticated").toBool();
        }))
        return failed("logout", &a);
    const auto count = requests;
    t = a.task(id);
    if (!a.saveTask(id, t.value("rowVersion").toLongLong(), "退出后离线修改", "", "ACTIVE", "", "",
                    false) ||
        !idle(a))
        return failed("signed out local edit", &a);
    a.syncNow();
    until([] { return false; }, 100);
    if (requests != count || !credentialsA.values.isEmpty())
        return failed("signed out must make zero network requests", &a);
    a.syncLogin(url, login, password);
    if (!settle(a))
        return failed("same account offline queue resume", &a);
    b.syncNow();
    if (!settle(b) || b.task(id).value("title") != "退出后离线修改")
        return failed("offline edit replicated", &a, &b);
    a.syncLogout();
    b.syncLogout();
    if (!until([&] {
            return !a.syncState().value("busy").toBool() && !b.syncState().value("busy").toBool();
        }))
        return failed("offline conflict preparation");
    t = a.task(id);
    if (!a.saveTask(id, t.value("rowVersion").toLongLong(), "服务器先提交", "", "ACTIVE", "", "",
                    false) ||
        !idle(a))
        return failed("first conflict candidate");
    t = b.task(id);
    if (!b.saveTask(id, t.value("rowVersion").toLongLong(), "第二设备候选", "", "ACTIVE", "", "",
                    false) ||
        !idle(b))
        return failed("second conflict candidate");
    a.syncLogin(url, login, password);
    if (!settle(a))
        return failed("first conflict commit", &a);
    b.syncLogin(url, login, password);
    if (!until([&] {
            return !b.syncState().value("busy").toBool() &&
                   !b.syncState().value("conflicts").toList().isEmpty();
        }))
        return failed("durable conflict", &a, &b);
    const auto conflicts = b.syncState().value("conflicts").toList();
    b.syncResolveConflict(conflicts.first().toMap().value("id").toString(), "remote");
    if (!settle(b) || b.task(id).value("title") != "服务器先提交")
        return failed("conflict remote choice", &a, &b);
    t = a.task(id);
    if (!a.saveDetails(id, t.value("rowVersion").toLongLong(),
                       {{"repeat", QVariantMap{{"frequency", "AFTER_DAYS"}, {"interval", 1}}}}) ||
        !idle(a) || !settle(a) || !settle(b))
        return failed("after completion rule replication", &a, &b);
    QString predecessor = id;
    for (int cycle = 0; cycle < 2; ++cycle) {
        t = a.task(predecessor);
        const bool accepted =
            cycle == 0 ? a.changeState(predecessor, t.value("rowVersion").toLongLong(), "complete")
                       : a.batchTasks({QVariantMap{{"taskId", predecessor},
                                                   {"rowVersion", t.value("rowVersion")}}},
                                      "complete", "");
        if (!accepted || !idle(a) || !settle(a) || !settle(b))
            return failed("rapid completion and atomic successor", &a, &b);
        QString successor;
        const auto name = sync::uuid();
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", name);
            db.setDatabaseName(a.databasePath());
            db.setConnectOptions("QSQLITE_OPEN_READONLY");
            if (!db.open())
                return failed("read successor");
            QSqlQuery query(db);
            query.prepare("SELECT id FROM tasks WHERE sync_occurrence_key=? AND status='OPEN'");
            query.addBindValue("A:" + predecessor);
            if (query.exec() && query.next())
                successor = query.value(0).toString();
            query.finish();
            db.close();
        }
        QSqlDatabase::removeDatabase(name);
        if (successor.isEmpty() || b.task(successor).isEmpty())
            return failed("successor identity on both devices", &a, &b);
        predecessor = successor;
    }
    if(external) {
        const auto title=QStringLiteral("External device B ")+sync::uuid();
        if(!b.createTask(title) || !idle(b))return failed("external second device create",&a,&b);
        QString created;
        for(int row=0;row<b.tasks()->rowCount();++row){const auto candidate=b.tasks()->taskIdAt(row);if(b.task(candidate).value("title")==title)created=candidate;}
        if(created.isEmpty() || !settle(b))return failed("external second device push",&a,&b);
        a.syncNow();
        if(!settle(a) || a.task(created).value("title")!=title)return failed("external second-to-first replication",&a,&b);
        a.syncLogout();b.syncLogout();
        if(!until([&]{return !a.syncState().value("busy").toBool() && !b.syncState().value("busy").toBool();}))
            return failed("external service logout",&a,&b);
        std::cout << "PASS external server: native credentials; registration/login; two Qt workspaces; "
                     "offline import; bidirectional state; logout and resume; conflict resolution; "
                     "atomic recurrence successors; verified TLS; temporary local data; "
                  << (certs.isEmpty() ? "default system trust" : "explicit test CA") << std::endl;
        return 0;
    }
    const auto importPath = temporary.filePath("bulk.json");
    QString lastBulk;
    {
        SqliteDatabase source;
        if (!source.open(temporary.filePath("bulk-source.sqlite")).ok())
            return failed("bulk source");
        ScopedTransaction tx(source.connection());
        QSqlQuery q(source.connection());
        for (int i = 0; i < 1000; ++i) {
            lastBulk = sync::stableId("bulk-task-" + QString::number(i));
            q.prepare("INSERT INTO tasks(id,title,sort_order,created_at_utc,updated_at_utc) "
                      "VALUES(?,?,?,?,?)");
            q.addBindValue(lastBulk);
            q.addBindValue("Synthetic bulk " + QString::number(i));
            q.addBindValue(i);
            q.addBindValue(sync::now());
            q.addBindValue(sync::now());
            if (!q.exec())
                return failed("bulk seed");
        }
        if (!tx.commit() || !DataTransfer(source).exportJson(importPath).ok())
            return failed("bulk export");
    }
    if (!a.previewImport(importPath) || !idle(a) || !a.applyImport(false) || !idle(a))
        return failed("atomic bulk import", &a, &b);
    a.syncNow();
    if (!until(
            [&] {
                return !a.syncState().value("busy").toBool() &&
                       a.syncState().value("phase") == "RECONCILE_REQUIRED";
            },
            45000) ||
        !a.syncState().value("authenticated").toBool() || a.task(lastBulk).isEmpty() ||
        a.syncState().value("pending_count").toInt() == 0)
        return failed("expired upload retains facts and exposes explicit recovery", &a, &b);
    a.syncRebuild();
    if (!settle(a))
        return failed("backup and explicit remerge after expired staged job", &a, &b);
    b.syncNow();
    if (!settle(b) || b.task(lastBulk).isEmpty())
        return failed("large batch reference download", &a, &b);
    QString category, tag;
    QObject::connect(&a, &TaskWorkspace::catalogSaved, &a,
                     [&](const QString &created, const QString &kind) {
                         if (kind == "project")
                             category = created;
                         if (kind == "tag")
                             tag = created;
                     });
    if (!a.saveCatalog({{"kind", "project"}, {"title", "Synthetic category"}}) || !idle(a) ||
        !settle(a) || !a.saveCatalog({{"kind", "tag"}, {"title", "Synthetic tag"}}) || !idle(a) ||
        !settle(a))
        return failed("category and tag commands", &a, &b);
    t = a.task(lastBulk);
    const auto future = QDate::currentDate().addDays(2).toString(Qt::ISODate);
    if (!a.saveDetails(lastBulk, t.value("rowVersion").toLongLong(),
                       {{"projectId", category},
                        {"tagIds", QStringList{tag}},
                        {"schedule", QVariantMap{{"allDay", true},
                                                 {"startDate", future},
                                                 {"endDate", future},
                                                 {"alertMinutes", 10}}}}) ||
        !idle(a) || !settle(a) || !settle(b))
        return failed("move tree, schedule and reminder configuration", &a, &b);
    QString child;
    QObject::connect(&a, &TaskWorkspace::subtaskCreated, &a,
                     [&](const QString &created) { child = created; });
    if (!a.createSubtask("Synthetic child", lastBulk) || !idle(a) || !settle(a) || !settle(b) ||
        child.isEmpty() || b.task(child).value("parentTaskId") != lastBulk)
        return failed("hierarchy create replication", &a, &b);
    t = a.task(lastBulk);
    if (!a.changeState(lastBulk, t.value("rowVersion").toLongLong(), "delete") || !idle(a) ||
        !settle(a) || !settle(b))
        return failed("whole tree soft deletion", &a, &b);
    a.setView("trash");
    if (!idle(a))
        return failed("trash load");
    t.clear();
    for (const auto &deleted : a.tasks()->items())
        if (deleted.id == lastBulk)
            t = TaskListModel::taskMap(deleted);
    if (!a.changeState(lastBulk, t.value("rowVersion").toLongLong(), "restore") || !idle(a) ||
        !settle(a) || !settle(b) || b.task(child).isEmpty())
        return failed("same cohort restore", &a, &b);
    auto categoryVersion = qint64(0), tagVersion = qint64(0);
    for (const auto &entry : a.catalog()) {
        const auto item = entry.toMap();
        if (item.value("id") == category)
            categoryVersion = item.value("rowVersion").toLongLong();
        if (item.value("id") == tag)
            tagVersion = item.value("rowVersion").toLongLong();
    }
    if (!a.deleteCatalog("tag", tag, tagVersion) || !idle(a) || !settle(a) ||
        !a.deleteCatalog("project", category, categoryVersion) || !idle(a) || !settle(a) ||
        !settle(b))
        return failed("catalog removal keeps tasks", &a, &b);
    a.setView("all");
    if (!idle(a) || a.task(lastBulk).isEmpty() || b.task(lastBulk).isEmpty())
        return failed("task preserved after category removal", &a, &b);
    a.syncLogout();
    if (!until([&] { return !a.syncState().value("busy").toBool(); }))
        return failed("before account switch");
    a.syncLogin(url, "second@example.invalid", password);
    if (!settle(a) || !a.task(id).isEmpty() || !a.task(lastBulk).isEmpty())
        return failed("account ownership isolation", &a);
    const auto profiles = a.syncState().value("profiles").toList();
    QString original;
    for (const auto &x : profiles)
        if (x.toMap().value("label") == login)
            original = x.toMap().value("id").toString();
    if (original.isEmpty())
        return failed("local profile recovery entry", &a);
    a.syncOpenProfile(original);
    if (!until([&] {
            return !a.syncState().value("busy").toBool() &&
                   a.task(id).value("title") == "服务器先提交";
        }))
        return failed("offline prior profile reopening", &a);
    if (a.syncState().value("authenticated").toBool())
        return failed("prior profile must remain offline", &a);
    const auto connection = sync::uuid();
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
        db.setDatabaseName(a.databasePath());
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open())
            return failed("read final graph");
        const auto graph = sync::Mapping(db).capture();
        if (!graph.ok())
            return failed("capture final graph");
        QFile report(qEnvironmentVariable("DAYMARK_SYNC_E2E_GRAPH_PATH"));
        if (!report.open(QIODevice::WriteOnly) ||
            report.write(sync::json({{"entities", graph.value.array()}})) < 0)
            return failed("write graph oracle");
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    std::cout << "PASS native credentials; Qt/Go trusted TLS; guest import; snapshot; incremental "
                 "push/pull; logout zero-network; durable offline queue; conflict resolution; "
                 "atomic 1000-task import; staged mutation; large batch reference; account "
                 "isolation and offline recovery"
              << std::endl;
    return 0;
}

} // namespace daymark::test::sync_account
#ifndef DAYMARK_ACCOUNT_SYNC_EMBEDDED
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    return daymark::test::sync_account::run();
}
#endif
