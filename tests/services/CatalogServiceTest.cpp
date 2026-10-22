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
