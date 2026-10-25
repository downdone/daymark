#include "services/CatalogService.h"
#include "services/TaskService.h"
#include "persistence/SqliteDatabase.h"
#include <QTemporaryDir>
#include <QSqlQuery>
#include <QFile>
#include <QtTest>
using namespace daymark;
class CatalogServiceTest:public QObject {
    Q_OBJECT
private slots:
    void schemaFourCategoriesUpgradeWithBackupAndCollisionRollback() {
        QTemporaryDir dir;{SqliteDatabase init;QVERIFY(init.open(dir.filePath(QStringLiteral("resources.sqlite"))).ok());}
        for(const bool collision:{false,true}) {
            const auto path=dir.filePath(collision?QStringLiteral("collision.sqlite"):QStringLiteral("old.sqlite"));const auto name=path;
            {auto raw=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);raw.setDatabaseName(path);QVERIFY(raw.open());QSqlQuery q(raw);
                for(const auto& resource:{"001_init.sql","002_area_archive.sql","003_calendar_events.sql","004_event_series.sql"}){QFile file(QStringLiteral(":/daymark/src/persistence/migrations/")+QLatin1String(resource));QVERIFY(file.open(QIODevice::ReadOnly));for(const auto& sql:QString::fromUtf8(file.readAll()).split(';',Qt::SkipEmptyParts))if(!sql.trimmed().isEmpty())QVERIFY(q.exec(sql));}
                QVERIFY(q.exec(QStringLiteral("INSERT INTO areas(id,name,created_at_utc,updated_at_utc) VALUES('root','Work','2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO projects(id,area_id,title,created_at_utc,updated_at_utc) VALUES('child','root','Release','2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO tasks(id,title,direct_area_id,created_at_utc,updated_at_utc) VALUES('direct','Keep','root','2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO checklist_items(id,task_id,title,is_done,sort_order,created_at_utc,updated_at_utc) VALUES('step','direct','Migrated step',1,2,'2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));
                if(collision)QVERIFY(q.exec(QStringLiteral("INSERT INTO projects(id,title,created_at_utc,updated_at_utc) VALUES('root','Collision','2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));
                QVERIFY(q.exec(QStringLiteral("PRAGMA user_version=4")));raw.close();}
            QSqlDatabase::removeDatabase(name);SqliteDatabase upgraded;const auto opened=upgraded.open(path);
            if(collision){QCOMPARE(opened.error.key,QStringLiteral("migrationFailed"));auto raw=upgraded.connection();QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),4);q.finish();QVERIFY(q.exec(QStringLiteral("SELECT direct_area_id FROM tasks WHERE id='direct'")));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("root"));raw.close();}
            else {QVERIFY(opened.ok());CatalogRepository repo(upgraded.connection());QCOMPARE(repo.find(QStringLiteral("project"),QStringLiteral("child")).value.parentId,QStringLiteral("root"));QCOMPARE(repo.find(QStringLiteral("project"),QStringLiteral("root")).value.title,QStringLiteral("Work"));const auto task=TaskRepository(upgraded.connection()).find(QStringLiteral("direct"));QVERIFY(task.ok());QCOMPARE(task.value.projectId,QStringLiteral("root"));QVERIFY(task.value.areaId.isEmpty());const auto child=TaskRepository(upgraded.connection()).find(QStringLiteral("step"));QVERIFY(child.ok());QCOMPARE(child.value.parentTaskId,QStringLiteral("direct"));QCOMPARE(child.value.status,QStringLiteral("COMPLETED"));QCOMPARE(child.value.projectId,QStringLiteral("root"));QCOMPARE(child.value.title,QStringLiteral("Migrated step"));QCOMPARE(child.value.sortOrder,qint64(2));}
        }
        QVERIFY(!QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files).isEmpty());
    }
    void categoryDragOrdersReparentsAndRollsBack() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("drag-categories.sqlite"))).ok());SystemClock clock;CatalogRepository repo(db.connection());CatalogService service(repo,clock);TaskRepository tasks(db.connection());TaskService actions(tasks,clock,&service);
        auto make=[&](const QString& title,const QString& parent=QString()){CatalogItem item;item.kind=QStringLiteral("project");item.title=title;item.parentId=parent;return service.save(item);};
        const auto a=make(QStringLiteral("A")),b=make(QStringLiteral("B")),c=make(QStringLiteral("C"));QVERIFY(a.ok() && b.ok() && c.ok());
        auto current=[&](const QString& id){return repo.find(QStringLiteral("project"),id).value;};
        auto drop=[&](const QString& id,const QString& target,const QString& placement){return service.drop(id,current(id).rowVersion,target,current(target).rowVersion,placement);};
        const auto task=actions.create(QStringLiteral("keep"),QStringLiteral("project:")+c.value.id);QVERIFY(task.ok());
        QVERIFY(drop(c.value.id,a.value.id,QStringLiteral("before")).ok());auto entries=repo.all().value;QStringList ids;for(const auto& item:entries)ids.append(item.id);QCOMPARE(ids.indexOf(c.value.id)+1,ids.indexOf(a.value.id));
        const auto stale=current(c.value.id);QVERIFY(drop(c.value.id,b.value.id,QStringLiteral("inside")).ok());QCOMPARE(current(c.value.id).parentId,b.value.id);QCOMPARE(tasks.find(task.value.id).value.projectId,c.value.id);QCOMPARE(tasks.find(task.value.id).value.rowVersion,task.value.rowVersion);
        QCOMPARE(service.drop(c.value.id,stale.rowVersion,a.value.id,current(a.value.id).rowVersion,QStringLiteral("inside")).error.key,QStringLiteral("conflict"));
        QCOMPARE(drop(a.value.id,c.value.id,QStringLiteral("inside")).error.key,QStringLiteral("categoryDepth"));QCOMPARE(drop(b.value.id,a.value.id,QStringLiteral("inside")).error.key,QStringLiteral("categoryHasChildren"));
        QVERIFY(drop(c.value.id,a.value.id,QStringLiteral("after")).ok());QVERIFY(current(c.value.id).parentId.isEmpty());
        const auto before=repo.all().value;QSqlQuery trigger(db.connection());QVERIFY(trigger.exec(QStringLiteral("CREATE TRIGGER reject_category_rank BEFORE UPDATE OF sort_order ON projects BEGIN SELECT RAISE(ABORT,'fixture'); END")));
        QVERIFY(!drop(c.value.id,b.value.id,QStringLiteral("inside")).ok());const auto after=repo.all().value;QCOMPARE(after.size(),before.size());for(int i=0;i<after.size();++i){QCOMPARE(after[i].id,before[i].id);QCOMPARE(after[i].parentId,before[i].parentId);QCOMPARE(after[i].rowVersion,before[i].rowVersion);}QCOMPARE(tasks.find(task.value.id).value.rowVersion,task.value.rowVersion);
    }
    void categoryHierarchyPromotesDemotesAndPreservesTasks() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("categories.sqlite"))).ok());
        SystemClock clock;CatalogRepository repo(db.connection());CatalogService service(repo,clock);TaskRepository tasks(db.connection());TaskService actions(tasks,clock,&service);
        CatalogItem item;item.kind=QStringLiteral("project");item.title=QStringLiteral("Work");const auto root=service.save(item);QVERIFY(root.ok());
        item.title=QStringLiteral("Release");item.parentId=root.value.id;auto child=service.save(item);QVERIFY(child.ok());
        auto task=actions.create(QStringLiteral("same task"),QStringLiteral("project:")+child.value.id);QVERIFY(task.ok());const auto id=task.value.id;
        QVERIFY(QueryService::inView(task.value,QStringLiteral("project:")+root.value.id,clock.localToday()));
        auto cycle=root.value;cycle.parentId=root.value.id;QCOMPARE(service.save(cycle).error.key,QStringLiteral("categoryCycle"));
        cycle.parentId=child.value.id;QCOMPARE(service.save(cycle).error.key,QStringLiteral("categoryDepth"));
        child.value.parentId.clear();child=service.save(child.value);QVERIFY(child.ok());QVERIFY(!QueryService::inView(tasks.find(id).value,QStringLiteral("project:")+root.value.id,clock.localToday()));
        child.value.parentId=root.value.id;child=service.save(child.value);QVERIFY(child.ok());QCOMPARE(tasks.find(id).value.rowVersion,task.value.rowVersion);
        item.parentId.clear();item.title=QStringLiteral("Personal");const auto other=service.save(item);QVERIFY(other.ok());cycle=root.value;cycle.parentId=other.value.id;QCOMPARE(service.save(cycle).error.key,QStringLiteral("categoryHasChildren"));
        auto archived=root.value;archived.archived=true;const auto hidden=service.save(archived);QVERIFY(hidden.ok());QVERIFY(!QueryService::effectiveActive(tasks.find(id).value));
        archived=hidden.value;archived.archived=false;const auto restored=service.save(archived);QVERIFY(restored.ok());QVERIFY(QueryService::effectiveActive(tasks.find(id).value));
        QVERIFY(service.remove(QStringLiteral("project"),root.value.id,restored.value.rowVersion).ok());QVERIFY(repo.find(QStringLiteral("project"),child.value.id).value.parentId.isEmpty());QCOMPARE(tasks.find(id).value.projectId,child.value.id);
    }
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
        patch.tagIds=FieldPatch<QStringList>::set({t.value.id});
        auto saved=tasks.edit(task.value.id,task.value.rowVersion,patch);QVERIFY(saved.ok());QCOMPARE(saved.value.tagNames,QStringList{QStringLiteral("Review")});
        auto completed=tasks.changeState(saved.value.id,saved.value.rowVersion,QStringLiteral("complete"));QVERIFY(completed.ok());
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
