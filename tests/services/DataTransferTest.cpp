#include "persistence/DataTransfer.h"
#include "services/TaskService.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QSettings>
#include <QtTest>
using namespace daymark;
class DataTransferTest:public QObject {
    Q_OBJECT
private slots:
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
