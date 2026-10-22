#include "models/TaskWorkspace.h"
#include "persistence/SqliteDatabase.h"
#include "persistence/TaskRepository.h"
#include <QAbstractItemModelTester>
#include <QTemporaryDir>
#include <QtTest>

using namespace daymark;
class TaskWorkspaceTest : public QObject {
    Q_OBJECT
private slots:
    void commandsUpdateBothModelsOnlyAfterCommit()
    {
        QTemporaryDir dir; TaskWorkspace workspace;
        QAbstractItemModelTester tester(workspace.tasks(),QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester todayTester(workspace.todayTasks(),QAbstractItemModelTester::FailureReportingMode::QtTest);
        workspace.start(dir.filePath(QStringLiteral("tasks.sqlite")));
        QTRY_VERIFY_WITH_TIMEOUT(!workspace.busy(),10000); QVERIFY(workspace.ready());
        workspace.setView(QStringLiteral("today"));
        QVERIFY(workspace.createTask(QStringLiteral("   ")));
        QTRY_VERIFY(!workspace.busy()); QCOMPARE(workspace.tasks()->rowCount(),0); QVERIFY(!workspace.errorText().isEmpty());
        for(int i=0;i<20;++i) {QVERIFY(workspace.createTask(QStringLiteral("Today %1").arg(i))); QTRY_VERIFY(!workspace.busy());}
        QCOMPARE(workspace.tasks()->rowCount(),20); QCOMPARE(workspace.todayTasks()->rowCount(),20);
        const auto first=workspace.tasks()->index(0);
        const auto id=workspace.tasks()->data(first,TaskListModel::Id).toString();
        auto task=workspace.task(id);
        QVERIFY(workspace.saveTask(id,task.value("rowVersion").toLongLong(),QStringLiteral("edit"),QString(),QStringLiteral("ACTIVE"),QStringLiteral("2026-02-30"),QString(),true));
        QTRY_VERIFY(!workspace.busy()); QCOMPARE(workspace.task(id),task); QVERIFY(!workspace.errorText().isEmpty());
        QVERIFY(workspace.changeState(id,task.value("rowVersion").toLongLong(),QStringLiteral("complete")));
        QTRY_VERIFY(!workspace.busy()); QCOMPARE(workspace.tasks()->rowCount(),19); QCOMPARE(workspace.todayTasks()->rowCount(),19);
        workspace.setView(QStringLiteral("logbook")); QCOMPARE(workspace.tasks()->rowCount(),1);
        task=workspace.task(id);
        QVERIFY(workspace.changeState(id,task.value("rowVersion").toLongLong(),QStringLiteral("delete")));
        QTRY_VERIFY(!workspace.busy()); QCOMPARE(workspace.tasks()->rowCount(),0); QVERIFY(workspace.canUndo());
        QVERIFY(workspace.undoDelete()); QTRY_VERIFY(!workspace.busy()); QCOMPARE(workspace.tasks()->rowCount(),1);
        QVERIFY(workspace.createBackup());QTRY_VERIFY(!workspace.busy());QVERIFY(!workspace.infoText().isEmpty());
    }
    void shutdownDrainsAcceptedWriteAndReloadPersists()
    {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("tasks.sqlite"));
        {
            TaskWorkspace workspace;workspace.start(path);QTRY_VERIFY(!workspace.busy());QVERIFY(workspace.ready());
            QVERIFY(workspace.createTask(QStringLiteral("drain on exit")));
        }
        SqliteDatabase db;QVERIFY(db.open(path).ok());TaskRepository repo(db.connection());
        QCOMPARE(repo.all().value.size(),1);QCOMPARE(repo.all().value.first().title,QStringLiteral("drain on exit"));
    }
    void manualOrderAndEveningDropPersist()
    {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("order.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());w.setView(QStringLiteral("today"));
        for(const auto& title:{QStringLiteral("a"),QStringLiteral("b"),QStringLiteral("c")}){QVERIFY(w.createTask(title));QTRY_VERIFY(!w.busy());}
        const auto id=[&](int i){return w.tasks()->data(w.tasks()->index(i),TaskListModel::Id).toString();};
        const auto first=id(0),last=id(2);QVERIFY(w.moveTask(first,1));QTRY_VERIFY(!w.busy());QCOMPARE(id(1),first);
        auto map=w.task(last);QVERIFY(w.saveDetails(last,map.value(QStringLiteral("rowVersion")).toLongLong(),{{QStringLiteral("evening"),true}}));QTRY_VERIFY(!w.busy());
        QVERIFY(w.dropTask(first,last));QTRY_VERIFY(!w.busy());QVERIFY(w.task(first).value(QStringLiteral("evening")).toBool());
        w.reload();QTRY_VERIFY(!w.busy());QVERIFY(w.task(first).value(QStringLiteral("evening")).toBool());
        w.setView(QStringLiteral("anytime"));QCOMPARE(id(0),first); // Today order is independent of base order.
    }
};
QTEST_GUILESS_MAIN(TaskWorkspaceTest)
#include "TaskWorkspaceTest.moc"
