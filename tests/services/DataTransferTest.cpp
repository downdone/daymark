#include "persistence/DataTransfer.h"
#include "services/TaskService.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QSettings>
#include <QSqlQuery>
#include <QtTest>
using namespace daymark;
class DataTransferTest:public QObject {
    Q_OBJECT
private slots:
    void weeklyPolicySkipsRecentSnapshotButDailyPolicyRefreshes()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("policy.sqlite"))).ok());
        TaskRepository repo(db.connection());SystemClock clock;TaskService service(repo,clock);QVERIFY(service.create(QStringLiteral("snapshot data")).ok());
        QVERIFY(db.ensureDailySnapshot().ok());QDir backups(dir.filePath(QStringLiteral("backups/daily")));
        const auto original=backups.entryList({QStringLiteral("daily-*.sqlite")},QDir::Files).first();
        auto past=original;past.replace(6,10,QDate::currentDate().addDays(-3).toString(Qt::ISODate));QVERIFY(backups.rename(original,past));
        QSettings settings(dir.filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);settings.setValue(QStringLiteral("preferences/backupDays"),7);settings.sync();
        QVERIFY(db.ensureDailySnapshot().ok());QCOMPARE(backups.entryList({QStringLiteral("daily-*.sqlite")},QDir::Files).size(),1);
        settings.setValue(QStringLiteral("preferences/backupDays"),1);settings.sync();QVERIFY(db.ensureDailySnapshot().ok());
        QCOMPARE(backups.entryList({QStringLiteral("daily-*.sqlite")},QDir::Files).size(),2);
        QCOMPARE(repo.all().value.size(),1);
    }
    void legacyAreaJsonUpgradesWithoutLosingData()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("legacy.sqlite"))).ok());
        CatalogRepository repository(db.connection());SystemClock clock;CatalogService service(repository,clock);
        CatalogItem area;area.kind=QStringLiteral("area");area.title=QStringLiteral("legacy");area.archived=true;QVERIFY(service.save(area).ok());
        DataTransfer transfer(db);const auto path=dir.filePath(QStringLiteral("export.json"));QVERIFY(transfer.exportJson(path).ok());
        auto plan=DataTransfer::preview(path);QVERIFY(plan.ok());
        QVERIFY(transfer.apply(plan.value,true).ok());QVERIFY(repository.all().value.first().archived);
        auto legacy=plan.value.document;legacy.insert(QStringLiteral("schemaVersion"),1);auto tables=legacy.value(QStringLiteral("tables")).toObject();auto areas=tables.value(QStringLiteral("areas")).toArray();auto row=areas[0].toObject();row.remove(QStringLiteral("archived"));areas[0]=row;tables.insert(QStringLiteral("areas"),areas);legacy.insert(QStringLiteral("tables"),tables);
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(legacy).toJson());file.close();
        plan=DataTransfer::preview(path);QVERIFY(plan.ok());QVERIFY(transfer.apply(plan.value,true).ok());QVERIFY(!repository.all().value.first().archived);
    }
    void migrationBacksUpWalAndBackupFailureBlocksUpgrade()
    {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("upgrade.sqlite"));
        {
            SqliteDatabase db;QVERIFY(db.open(path).ok());TaskRepository repo(db.connection());SystemClock clock;TaskService service(repo,clock);QVERIFY(service.create(QStringLiteral("retained" )).ok());
            QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("DROP TABLE event_reminder_history")));QVERIFY(q.exec(QStringLiteral("DROP TABLE event_tasks")));QVERIFY(q.exec(QStringLiteral("DROP TABLE events")));QVERIFY(q.exec(QStringLiteral("ALTER TABLE areas DROP COLUMN archived")));QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=1")));
        }
        QFile blocker(dir.filePath(QStringLiteral("backups")));QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();
        {
            SqliteDatabase blocked;QCOMPARE(blocked.open(path).error.key,QStringLiteral("backupFailed"));
            auto raw=blocked.connection();QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);q.finish();raw.close();
        }
        QVERIFY(blocker.remove());
        SqliteDatabase upgraded;QVERIFY(upgraded.open(path).ok());
        QCOMPARE(TaskRepository(upgraded.connection()).all().value.size(),1);
        const auto backups=QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files);QCOMPARE(backups.size(),1);
        QSqlQuery q(upgraded.connection());QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),4);
    }
    void roundtripPreviewMergeAndRestore()
    {
        QTemporaryDir dir;SqliteDatabase source,target;QVERIFY(source.open(dir.filePath(QStringLiteral("source.sqlite"))).ok());QVERIFY(target.open(dir.filePath(QStringLiteral("target.sqlite"))).ok());
        SystemClock clock;TaskRepository repo(source.connection());TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("中文 \"quote\"\n=SUM(A1)"),QStringLiteral("today"));QVERIFY(task.ok());
        TaskPatch patch;patch.notes=FieldPatch<QString>::set(QStringLiteral("多行\n备注"));patch.checklist=FieldPatch<QList<ChecklistItem>>::set({{QString(),QStringLiteral("item"),true,0}});QVERIFY(service.edit(task.value.id,1,patch).ok());
        DataTransfer exporter(source),importer(target);const auto json=dir.filePath(QStringLiteral("tasks.json"));QVERIFY(exporter.exportJson(json).ok());QVERIFY(exporter.exportCsv(dir.filePath(QStringLiteral("tasks.csv"))).ok());
        QVERIFY(!exporter.exportJson(source.connection().databaseName()).ok());
        auto plan=DataTransfer::preview(json);QVERIFY(plan.ok());QCOMPARE(plan.value.rows,2);QCOMPARE(TaskRepository(target.connection()).all().value.size(),0);
        auto imported=importer.apply(plan.value,false);QVERIFY(imported.ok());QCOMPARE(imported.value.added,2);QCOMPARE(TaskRepository(target.connection()).find(task.value.id).value,repo.find(task.value.id).value);
        auto merged=importer.apply(plan.value,false);QVERIFY(merged.ok());QCOMPARE(merged.value.skipped,2);
        const auto backup=source.createSnapshot(dir.filePath(QStringLiteral("snapshots")));QVERIFY(backup.ok());auto snapshot=DataTransfer::preview(backup.value,true);QVERIFY(snapshot.ok());
        auto restored=importer.apply(snapshot.value,true);QVERIFY(restored.ok());QVERIFY(QDir(dir.filePath(QStringLiteral("backups"))).entryList({QStringLiteral("*.sqlite")},QDir::Files).size()>0);
        auto withSettings=plan.value;withSettings.document.insert(QStringLiteral("settings"),QJsonObject{{QStringLiteral("sundayFirst"),true},{QStringLiteral("themeMode"),2},{QStringLiteral("textScale"),1.5}});
        auto configured=importer.apply(withSettings,true);QVERIFY(configured.ok());QVERIFY(configured.value.settingsApplied);
        QSettings settings(dir.filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);QCOMPARE(settings.value(QStringLiteral("appearance/themeMode")).toInt(),2);
        QVERIFY(target.ensureDailySnapshot().ok());QVERIFY(target.ensureDailySnapshot().ok());
        QCOMPARE(QDir(dir.filePath(QStringLiteral("backups/daily"))).entryList({QStringLiteral("daily-*.sqlite")},QDir::Files).size(),1);
    }
    void malformedImportsHaveZeroChanges()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("data.sqlite"))).ok());SystemClock clock;TaskRepository repo(db.connection());TaskService service(repo,clock);QVERIFY(service.create(QStringLiteral("preserve")).ok());
        DataTransfer transfer(db);const auto json=dir.filePath(QStringLiteral("tasks.json"));QVERIFY(transfer.exportJson(json).ok());const auto original=repo.all().value;auto plan=DataTransfer::preview(json);QVERIFY(plan.ok());
        auto bad=plan.value;auto tables=bad.document.value(QStringLiteral("tables")).toObject();auto tasks=tables.value(QStringLiteral("tasks")).toArray();auto task=tasks[0].toObject();task.insert(QStringLiteral("start_on"),QStringLiteral("2024-02-30"));tasks[0]=task;tables.insert(QStringLiteral("tasks"),tasks);bad.document.insert(QStringLiteral("tables"),tables);
        QVERIFY(!transfer.apply(bad,true).ok());QCOMPARE(repo.all().value,original);
        tasks=plan.value.document.value(QStringLiteral("tables")).toObject().value(QStringLiteral("tasks")).toArray();tasks.append(tasks[0]);tables.insert(QStringLiteral("tasks"),tasks);bad.document.insert(QStringLiteral("tables"),tables);
        QVERIFY(!transfer.apply(bad,false).ok());QCOMPARE(repo.all().value,original);
        bad.document={};QVERIFY(!transfer.apply(bad,true).ok());QCOMPARE(repo.all().value,original);
        bad=plan.value;bad.document.insert(QStringLiteral("formatVersion"),1.5);QVERIFY(!transfer.apply(bad,true).ok());QCOMPARE(repo.all().value,original);
        QFile file(json);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{broken");file.close();QVERIFY(!DataTransfer::preview(json).ok());QCOMPARE(repo.all().value,original);
    }
};
QTEST_GUILESS_MAIN(DataTransferTest)
#include "DataTransferTest.moc"
