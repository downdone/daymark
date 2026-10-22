#include "services/CatalogService.h"
#include "services/TaskService.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QtTest>
using namespace daymark;
class CatalogServiceTest:public QObject {
    Q_OBJECT
private slots:
    void archiveRestoresAreaTasksWithoutMutatingThem()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("archive.sqlite"))).ok());
        CatalogRepository catalog(db.connection());SystemClock clock;CatalogService service(catalog,clock);TaskRepository tasks(db.connection());TaskService taskService(tasks,clock,&service);
        CatalogItem area;area.kind=QStringLiteral("area");area.title=QStringLiteral("archive me");auto saved=service.save(area);QVERIFY(saved.ok());
        const auto task=taskService.create(QStringLiteral("preserve me"),QStringLiteral("area:")+saved.value.id);QVERIFY(task.ok());
        const auto original=task.value;
        saved.value.archived=true;saved=service.save(saved.value);QVERIFY(saved.ok());
        QVERIFY(QueryService::select(tasks.all().value,QStringLiteral("anytime"),clock.localToday()).isEmpty());
        QCOMPARE(tasks.find(original.id).value.rowVersion,original.rowVersion);
        saved.value.archived=false;QVERIFY(service.save(saved.value).ok());
        QCOMPARE(QueryService::select(tasks.all().value,QStringLiteral("anytime"),clock.localToday()).size(),1);
    }
    void siblingOrderPersistsAndRejectsStaleVersions()
    {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("order.sqlite"));SqliteDatabase db;QVERIFY(db.open(path).ok());
        CatalogRepository repo(db.connection());SystemClock clock;CatalogService service(repo,clock);
        CatalogItem item;item.kind=QStringLiteral("area");item.title=QStringLiteral("A");const auto a=service.save(item);QVERIFY(a.ok());
        item.title=QStringLiteral("B");const auto b=service.save(item);QVERIFY(b.ok());
        const auto original=repo.all().value;const auto last=original.last();
        QVERIFY(repo.move(last.kind,last.id,last.rowVersion,-1,clock.nowUtc().toString(Qt::ISODateWithMs)).ok());
        QCOMPARE(repo.all().value.first().id,last.id);
        QVERIFY(!repo.move(last.kind,last.id,last.rowVersion,1,clock.nowUtc().toString(Qt::ISODateWithMs)).ok());
        SqliteDatabase reopened;QVERIFY(reopened.open(path).ok());CatalogRepository second(reopened.connection());QCOMPARE(second.all().value.first().id,last.id);
    }
    void ownershipChecklistTagsAndSafeDeletion()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("test.sqlite"))).ok());
        SystemClock clock;CatalogRepository catalog(db.connection());CatalogService service(catalog,clock);
        TaskRepository repository(db.connection());TaskService tasks(repository,clock,&service);
        CatalogItem area;area.kind=QStringLiteral("area");area.title=QStringLiteral("工作");auto a=service.save(area);QVERIFY(a.ok());
        CatalogItem project;project.kind=QStringLiteral("project");project.title=QStringLiteral("发布");project.parentId=a.value.id;auto p=service.save(project);QVERIFY(p.ok());
        auto badProject=p.value;badProject.startOn=QDate(2024,3,2);badProject.deadlineOn=QDate(2024,3,1);QVERIFY(!service.save(badProject).ok());QCOMPARE(catalog.find(QStringLiteral("project"),p.value.id).value.rowVersion,p.value.rowVersion);
        CatalogItem heading;heading.kind=QStringLiteral("heading");heading.title=QStringLiteral("准备");heading.parentId=p.value.id;auto h=service.save(heading);QVERIFY(h.ok());
        auto inProject=tasks.create(QStringLiteral("项目内新增"),QStringLiteral("project:")+p.value.id);QVERIFY(inProject.ok());
        QCOMPARE(inProject.value.projectId,p.value.id);
        QVERIFY(QueryService::inView(inProject.value,QStringLiteral("area:")+a.value.id,clock.localToday()));
        CatalogItem tag;tag.kind=QStringLiteral("tag");tag.title=QStringLiteral("Review");auto t=service.save(tag);QVERIFY(t.ok());
        tag.title=QStringLiteral("review");QVERIFY(!service.save(tag).ok());
        auto task=tasks.create(QStringLiteral("task"),QStringLiteral("today"));QVERIFY(task.ok());
        TaskPatch patch;patch.projectId=FieldPatch<QString>::set(p.value.id);patch.headingId=FieldPatch<QString>::set(h.value.id);
        patch.tagIds=FieldPatch<QStringList>::set({t.value.id});patch.checklist=FieldPatch<QList<ChecklistItem>>::set({{QString(),QStringLiteral("check"),false,0}});
        auto saved=tasks.edit(task.value.id,task.value.rowVersion,patch);QVERIFY(saved.ok());QCOMPARE(saved.value.checklist.size(),1);QCOMPARE(saved.value.tagNames,QStringList{QStringLiteral("Review")});
        auto completed=tasks.changeState(saved.value.id,saved.value.rowVersion,QStringLiteral("complete"));QVERIFY(completed.ok());QVERIFY(!completed.value.checklist.first().done);
        auto active=tasks.changeState(completed.value.id,completed.value.rowVersion,QStringLiteral("reopen"));QVERIFY(active.ok());
        auto projectItem=p.value;projectItem.status=QStringLiteral("COMPLETED");auto finished=service.save(projectItem);QVERIFY(finished.ok());
        QVERIFY(!QueryService::inView(repository.find(active.value.id).value,QStringLiteral("today"),clock.localToday()));
        auto restore=finished.value;restore.status=QStringLiteral("OPEN");auto reopened=service.save(restore);QVERIFY(reopened.ok());
        QVERIFY(QueryService::inView(repository.find(active.value.id).value,QStringLiteral("today"),clock.localToday()));
        QSqlQuery query(db.connection());QVERIFY(query.exec(QStringLiteral("CREATE TRIGGER block_migration BEFORE UPDATE OF project_id ON tasks BEGIN SELECT RAISE(ABORT,'test'); END")));
        QVERIFY(!service.remove(QStringLiteral("project"),p.value.id,reopened.value.rowVersion).ok());QVERIFY(catalog.find(QStringLiteral("project"),p.value.id).ok());
        QVERIFY(query.exec(QStringLiteral("DROP TRIGGER block_migration")));
        QVERIFY(service.remove(QStringLiteral("project"),p.value.id,reopened.value.rowVersion).ok());
        auto migrated=repository.find(active.value.id);QVERIFY(migrated.ok());QVERIFY(migrated.value.projectId.isEmpty());QVERIFY(migrated.value.headingId.isEmpty());QVERIFY(migrated.value.tagIds.contains(t.value.id));
        QVERIFY(service.remove(QStringLiteral("area"),a.value.id,a.value.rowVersion).ok());
        QVERIFY(service.remove(QStringLiteral("tag"),t.value.id,t.value.rowVersion).ok());
        QVERIFY(repository.find(active.value.id).value.tagIds.isEmpty());
    }
};
QTEST_GUILESS_MAIN(CatalogServiceTest)
#include "CatalogServiceTest.moc"
