#include "persistence/SqliteDatabase.h"
#include "persistence/TaskRepository.h"
#include "services/TaskService.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace daymark;

class FixedClock final : public IClock {
public:
    QDate localToday() const override { return {2026,10,22}; }
    QDateTime nowUtc() const override { return QDateTime(QDate(2026,10,22),QTime(8,0),QTimeZone::UTC); }
};

class TaskServiceTest : public QObject {
    Q_OBJECT
    static Result<Task> legacyChild(TaskRepository& repo,TaskService& service,const QString& parentId) {
        auto task=service.create("nested","anytime");if(!task.ok())return task;
        task.value.parentTaskId=parentId;
        return repo.update(task.value,task.value.rowVersion);
    }
private slots:
    void batchReopenPreservesGroupsAndRollsBackWholeSelection() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("batch-reopen.sqlite")).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        const auto root=service.create("parent","anytime");QVERIFY(root.ok());
        const auto child=service.createSubtask("same completion",root.value.id);QVERIFY(child.ok());
        const auto prior=service.createSubtask("previously completed",root.value.id);QVERIFY(prior.ok());
        const auto other=service.create("other","anytime");QVERIFY(other.ok());
        QVERIFY(service.changeState(prior.value.id,prior.value.rowVersion,"complete").ok());
        QVERIFY(service.changeState(root.value.id,repo.find(root.value.id).value.rowVersion,"complete").ok());
        QVERIFY(service.changeState(other.value.id,other.value.rowVersion,"complete").ok());
        const auto before=repo.all().value;
        QList<QPair<QString,qint64>> selection{{root.value.id,repo.find(root.value.id).value.rowVersion},{other.value.id,repo.find(other.value.id).value.rowVersion},{child.value.id,repo.find(child.value.id).value.rowVersion}};
        auto stale=selection;--stale.last().second;
        QCOMPARE(service.batch(stale,"reopen",{}).error.key,QStringLiteral("conflict"));QCOMPARE(repo.all().value,before);
        QSqlQuery trigger(db.connection());
        QVERIFY(trigger.exec(QStringLiteral("CREATE TRIGGER reject_batch_reopen BEFORE UPDATE OF status ON tasks WHEN OLD.id='%1' AND NEW.status='OPEN' BEGIN SELECT RAISE(ABORT,'injected reopen failure'); END").arg(other.value.id)));
        QVERIFY(!service.batch(selection,"reopen",{}).ok());QCOMPARE(repo.all().value,before);
        QVERIFY(trigger.exec("DROP TRIGGER reject_batch_reopen"));
        QVERIFY(service.batch(selection,"reopen",{}).ok());
        for(const auto& id:{root.value.id,child.value.id,other.value.id})QCOMPARE(repo.find(id).value.status,QStringLiteral("OPEN"));
        QCOMPARE(repo.find(prior.value.id).value.status,QStringLiteral("COMPLETED"));
    }
    void twoLevelRelationsRejectNewDepthWithoutMutatingData() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("two-level.sqlite")).ok());TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        const auto root=service.create("root","anytime");QVERIFY(root.ok());const auto child=service.createSubtask("child",root.value.id);QVERIFY(child.ok());const auto other=service.create("other","anytime");QVERIFY(other.ok());const auto before=repo.all().value;
        QCOMPARE(service.createSubtask("grandchild",child.value.id).error.key,QStringLiteral("taskParentDepth"));QCOMPARE(repo.all().value,before);
        TaskPatch patch;patch.parentTaskId=FieldPatch<QString>::set(child.value.id);QCOMPARE(service.edit(other.value.id,other.value.rowVersion,patch).error.key,QStringLiteral("taskParentDepth"));QCOMPARE(repo.all().value,before);QVERIFY(repo.editHistory(other.value.id).value.isEmpty());
        patch.parentTaskId=FieldPatch<QString>::set(other.value.id);QCOMPARE(service.edit(root.value.id,root.value.rowVersion,patch).error.key,QStringLiteral("taskHasChildren"));QCOMPARE(repo.all().value,before);QVERIFY(repo.editHistory(root.value.id).value.isEmpty());
        patch.parentTaskId=FieldPatch<QString>::clear();const auto detached=service.edit(child.value.id,child.value.rowVersion,patch);QVERIFY(detached.ok());QVERIFY(detached.value.parentTaskId.isEmpty());
        patch.parentTaskId=FieldPatch<QString>::set(other.value.id);const auto moved=service.edit(child.value.id,detached.value.rowVersion,patch);QVERIFY(moved.ok());QCOMPARE(moved.value.parentTaskId,other.value.id);
        const auto legacy=legacyChild(repo,service,child.value.id);QVERIFY(legacy.ok());patch={};patch.title=FieldPatch<QString>::set("legacy still editable");QVERIFY(service.edit(legacy.value.id,legacy.value.rowVersion,patch).ok());
        QCOMPARE(service.createSubtask("deeper",legacy.value.id).error.key,QStringLiteral("taskParentDepth"));
    }

    void smartListsMatchExactlyAndOrdinaryListsKeepHierarchy() {
        const QDate today(2026,10,22);Task parent;parent.id="parent";parent.title="parent";parent.planningState="ACTIVE";parent.startOn=today;parent.important=true;
        Task child;child.id="child";child.title="child";child.parentTaskId=parent.id;child.planningState="ACTIVE";
        Task scheduled=child;scheduled.id="scheduled";scheduled.startOn=today.addDays(4);Task done=child;done.id="done";done.status="COMPLETED";Task deleted=child;deleted.id="deleted";deleted.deletedAtUtc="2026-10-22T08:00:00Z";
        const QList<Task> tasks{parent,child,scheduled,done,deleted};
        QCOMPARE(QueryService::select(tasks,"today",today,{},false,{},0,0,{}, {},true,false).size(),1);
        QCOMPARE(QueryService::select(tasks,"scheduled",today,{},false,{},0,0,{}, {},true,false).size(),2);
        QCOMPARE(QueryService::select(tasks,"important",today).size(),1);QCOMPARE(QueryService::select(tasks,"completed",today).size(),1);QCOMPARE(QueryService::select(tasks,"trash",today).size(),1);
        const auto tree=QueryService::select(tasks,"all",today);QCOMPARE(tree.size(),3);QCOMPARE(tree.first().id,parent.id);QCOMPARE(tree[1].listDepth,1);QCOMPARE(tree[2].listDepth,1);
        QCOMPARE(QueryService::select(tasks,"all",today,{},false,{},0,0,{}, {parent.id}).size(),1);
        auto datedChild=child;datedChild.startOn=today;const auto flat=QueryService::select({parent,datedChild},"today",today,{},false,{},0,0,{}, {parent.id},true,false);QCOMPARE(flat.size(),2);QCOMPARE(flat[1].listDepth,0);
        Task timed=child;TaskSchedule schedule;schedule.allDay=false;schedule.startUtc=QDateTime(today,QTime(14,0),QTimeZone::LocalTime);schedule.endUtc=schedule.startUtc.addSecs(3600);timed.schedule=schedule;QCOMPARE(QueryService::plannedDate(timed),today);QCOMPARE(QueryService::todayPeriod(timed,today),2);
        QCOMPARE(QueryService::todayPeriod(timed,today.addDays(1)),0);
    }
    void treeActionsPreserveEarlierCompletionAndDeletionAcrossRestart() {
        QTemporaryDir dir;const auto path=dir.filePath("tree.sqlite");FixedClock clock;QString rootId,childId,nestedId,priorId,deletedId;
        {SqliteDatabase db;QVERIFY(db.open(path).ok());TaskRepository repo(db.connection());TaskService service(repo,clock);
            auto root=service.create("root","anytime");QVERIFY(root.ok());rootId=root.value.id;auto child=service.createSubtask("child",rootId);QVERIFY(child.ok());childId=child.value.id;auto nested=legacyChild(repo,service,childId);QVERIFY(nested.ok());nestedId=nested.value.id;
            auto prior=service.createSubtask("prior",rootId);QVERIFY(prior.ok());priorId=prior.value.id;QVERIFY(service.changeState(priorId,prior.value.rowVersion,"complete").ok());auto deleted=service.createSubtask("deleted",rootId);QVERIFY(deleted.ok());deletedId=deleted.value.id;QVERIFY(service.changeState(deletedId,deleted.value.rowVersion,"delete").ok());
            const auto before=repo.find(rootId).value;const auto completed=service.changeState(rootId,before.rowVersion,"complete");QVERIFY(completed.ok());for(const auto& id:{rootId,childId,nestedId}){const auto task=repo.find(id).value;QCOMPARE(task.status,QStringLiteral("COMPLETED"));QCOMPARE(task.completionGroupId,completed.value.completionGroupId);}QVERIFY(!completed.value.completionGroupId.isEmpty());
            const auto noOp=service.changeState(rootId,completed.value.rowVersion,"complete");QVERIFY(noOp.ok());QCOMPARE(noOp.value.rowVersion,completed.value.rowVersion);
            QVERIFY(service.changeState(rootId,completed.value.rowVersion,"reopen").ok());for(const auto& id:{rootId,childId,nestedId})QCOMPARE(repo.find(id).value.status,QStringLiteral("OPEN"));QCOMPARE(repo.find(priorId).value.status,QStringLiteral("COMPLETED"));
            const auto removed=service.changeState(rootId,repo.find(rootId).value.rowVersion,"delete");QVERIFY(removed.ok());for(const auto& id:{rootId,childId,nestedId,priorId})QCOMPARE(repo.find(id).value.deletionGroupId,removed.value.deletionGroupId);QVERIFY(repo.find(deletedId).value.deletionGroupId!=removed.value.deletionGroupId);
            TaskPatch patch;patch.title=FieldPatch<QString>::set("forbidden");QVERIFY(!service.edit(childId,repo.find(childId).value.rowVersion,patch).ok());}
        SqliteDatabase db;QVERIFY(db.open(path).ok());TaskRepository repo(db.connection());TaskService service(repo,clock);const auto root=repo.find(rootId).value;QVERIFY(service.changeState(rootId,root.rowVersion,"restore").ok());for(const auto& id:{rootId,childId,nestedId,priorId})QVERIFY(repo.find(id).value.deletedAtUtc.isEmpty());QCOMPARE(repo.find(childId).value.parentTaskId,rootId);QCOMPARE(repo.find(nestedId).value.parentTaskId,childId);QVERIFY(!repo.find(deletedId).value.deletedAtUtc.isEmpty());
    }
    void failedTreeCommandsRollBackEveryTaskAndHistory() {
        for(const auto& action:QStringList{"complete","delete","move"}){QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("rollback.sqlite")).ok());TaskRepository repo(db.connection());FixedClock clock;CatalogRepository catalog(db.connection());CatalogService catalogs(catalog,clock);TaskService service(repo,clock,&catalogs);CatalogItem category;category.kind="project";category.title="target";const auto target=catalogs.save(category);QVERIFY(target.ok());
            const auto root=service.create("root","anytime");QVERIFY(root.ok());const auto child=service.createSubtask("child",root.value.id);QVERIFY(child.ok());const auto nested=legacyChild(repo,service,child.value.id);QVERIFY(nested.ok());const auto before=repo.all().value;
            QSqlQuery q(db.connection());QVERIFY(q.exec(QStringLiteral("CREATE TRIGGER fail_tree BEFORE UPDATE ON tasks WHEN OLD.id='%1' BEGIN SELECT RAISE(ABORT,'injected'); END").arg(nested.value.id)));
            if(action=="move"){TaskPatch patch;patch.projectId=FieldPatch<QString>::set(target.value.id);QVERIFY(!service.edit(root.value.id,root.value.rowVersion,patch).ok());}else QVERIFY(!service.changeState(root.value.id,root.value.rowVersion,action).ok());QCOMPARE(repo.all().value,before);for(const auto& task:before)QCOMPARE(repo.editHistory(task.id).value.size(),task.id==nested.value.id ? 1 : 0);
            QVERIFY(q.exec("DROP TRIGGER fail_tree"));QCOMPARE(service.changeState(root.value.id,root.value.rowVersion+1,"delete").error.code,ErrorCode::Conflict);QCOMPARE(repo.all().value,before);
        }
    }
    void movingAChildExtractsItsWholeBranchAndBatchHandlesOverlap() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("move.sqlite")).ok());FixedClock clock;TaskRepository repo(db.connection());CatalogRepository catalog(db.connection());CatalogService catalogs(catalog,clock);TaskService service(repo,clock,&catalogs);CatalogItem category;category.kind="project";category.title="target";const auto target=catalogs.save(category);QVERIFY(target.ok());
        const auto root=service.create("root","anytime");const auto child=service.createSubtask("child",root.value.id);const auto nested=legacyChild(repo,service,child.value.id);TaskPatch patch;patch.projectId=FieldPatch<QString>::set(target.value.id);const auto moved=service.edit(root.value.id,root.value.rowVersion,patch);QVERIFY(moved.ok());for(const auto& id:{root.value.id,child.value.id,nested.value.id})QCOMPARE(repo.find(id).value.projectId,target.value.id);QCOMPARE(repo.find(child.value.id).value.parentTaskId,root.value.id);
        patch={};patch.projectId=FieldPatch<QString>::clear();const auto extracted=service.edit(child.value.id,repo.find(child.value.id).value.rowVersion,patch);QVERIFY(extracted.ok());QVERIFY(extracted.value.parentTaskId.isEmpty());QVERIFY(repo.find(nested.value.id).value.projectId.isEmpty());QCOMPARE(repo.find(nested.value.id).value.parentTaskId,child.value.id);QCOMPARE(repo.find(root.value.id).value.projectId,target.value.id);
        const auto c=repo.find(child.value.id).value,n=repo.find(nested.value.id).value;QVERIFY(service.batch({{n.id,n.rowVersion},{c.id,c.rowVersion}},"complete").ok());QCOMPARE(repo.find(c.id).value.completionGroupId,repo.find(n.id).value.completionGroupId);QVERIFY(service.changeState(c.id,repo.find(c.id).value.rowVersion,"reopen").ok());QCOMPARE(repo.find(n.id).value.status,QStringLiteral("OPEN"));QVERIFY(!service.batch({{c.id,c.rowVersion},{n.id,n.rowVersion}},"complete").ok());
        patch={};patch.parentTaskId=FieldPatch<QString>::set(root.value.id);QCOMPARE(service.edit(c.id,repo.find(c.id).value.rowVersion,patch).error.key,QStringLiteral("taskHasChildren"));
        // Explicitly promote the old descendant before reattaching both leaves.
        patch.parentTaskId=FieldPatch<QString>::clear();QVERIFY(service.edit(n.id,repo.find(n.id).value.rowVersion,patch).ok());patch.parentTaskId=FieldPatch<QString>::set(root.value.id);QVERIFY(service.edit(n.id,repo.find(n.id).value.rowVersion,patch).ok());QVERIFY(service.edit(c.id,repo.find(c.id).value.rowVersion,patch).ok());for(const auto& id:{c.id,n.id})QCOMPARE(repo.find(id).value.projectId,target.value.id);
    }
    void parentCyclesAndIndependentPinsRemainProtected() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("tree-attributes.sqlite")).ok());TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);const auto parent=service.create("parent","anytime");QVERIFY(parent.ok());const auto child=service.createSubtask("child",parent.value.id);QVERIFY(child.ok());const auto nested=legacyChild(repo,service,child.value.id);QVERIFY(nested.ok());const auto before=repo.find(parent.value.id).value;
        TaskPatch patch;patch.parentTaskId=FieldPatch<QString>::set(nested.value.id);QCOMPARE(service.edit(parent.value.id,parent.value.rowVersion,patch).error.key,QStringLiteral("taskParentCycle"));QCOMPARE(repo.find(parent.value.id).value,before);patch.parentTaskId=FieldPatch<QString>::set(parent.value.id);QCOMPARE(service.edit(parent.value.id,parent.value.rowVersion,patch).error.key,QStringLiteral("taskParentCycle"));QCOMPARE(repo.find(parent.value.id).value,before);
        patch={};patch.pinned=FieldPatch<bool>::set(true);patch.important=FieldPatch<bool>::set(true);const auto pinned=service.edit(child.value.id,child.value.rowVersion,patch);QVERIFY(pinned.ok());QVERIFY(pinned.value.pinned && pinned.value.important);QVERIFY(!pinned.value.todayFocus);QVERIFY(!repo.find(parent.value.id).value.pinned && !repo.find(parent.value.id).value.important);
        const auto tree=QueryService::select(repo.all().value,"all",clock.localToday());QCOMPARE(tree.size(),3);QCOMPARE(tree[0].id,parent.value.id);QCOMPARE(tree[1].id,child.value.id);QCOMPARE(tree[1].listDepth,1);QCOMPARE(tree[2].listDepth,2);const auto priority=QueryService::select(repo.all().value,"important",clock.localToday());QCOMPARE(priority.size(),1);QCOMPARE(priority.first().id,child.value.id);QCOMPARE(priority.first().listDepth,0);
        patch={};patch.parentTaskId=FieldPatch<QString>::clear();const auto detached=service.edit(child.value.id,pinned.value.rowVersion,patch);QVERIFY(detached.ok());QVERIFY(detached.value.parentTaskId.isEmpty());QVERIFY(detached.value.pinned && detached.value.important);QCOMPARE(repo.find(nested.value.id).value.parentTaskId,child.value.id);QCOMPARE(repo.editHistory(child.value.id).value.size(),2);QVERIFY(!service.edit(child.value.id,pinned.value.rowVersion,patch).ok());QCOMPARE(repo.editHistory(child.value.id).value.size(),2);
    }
    void navigationDoesNotPretendPausedProjectsAreActionable() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("paused-navigation.sqlite"))).ok());
        FixedClock clock;CatalogRepository catalog(db.connection());CatalogService catalogs(catalog,clock);TaskRepository repo(db.connection());TaskService service(repo,clock,&catalogs);
        CatalogItem project;project.kind=QStringLiteral("project");project.title=QStringLiteral("paused");project.planningState=QStringLiteral("SOMEDAY");
        const auto saved=catalogs.save(project);QVERIFY(saved.ok());const auto created=service.create(QStringLiteral("keep me"),QStringLiteral("project:")+saved.value.id);QVERIFY(created.ok());
        for(const auto& view:{QStringLiteral("today"),QStringLiteral("anytime"),QStringLiteral("upcoming")}){
            const auto moved=service.moveToView(created.value.id,created.value.rowVersion,view,QDate(2026,10,25));QVERIFY(!moved.ok());QCOMPARE(moved.error.key,QStringLiteral("destinationUnavailable"));QCOMPARE(repo.find(created.value.id).value,created.value);
        }
    }
    void navigationMovesPreserveContentAndRejectConflicts() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("navigation.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("moving"),QStringLiteral("date:2026-10-26")).value;
        TaskPatch patch;patch.deadlineOn=FieldPatch<QDate>::set(QDate(2026,10,30));patch.notes=FieldPatch<QString>::set(QStringLiteral("keep notes"));
        patch.remindAtUtc=FieldPatch<QString>::set(QStringLiteral("2026-10-25T09:00:00.000Z"));
        task=service.edit(task.id,task.rowVersion,patch).value;
        const auto before=task;
        QVERIFY(!service.moveToView(task.id,task.rowVersion,QStringLiteral("upcoming"),QDate(2026,11,1)).ok());QCOMPARE(repo.find(task.id).value,before);
        QVERIFY(!service.moveToView(task.id,task.rowVersion,QStringLiteral("upcoming"),clock.localToday()).ok());QCOMPARE(repo.find(task.id).value,before);
        auto moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("today"));QVERIFY(moved.ok());task=moved.value;
        QCOMPARE(task.startOn,std::optional<QDate>(clock.localToday()));QCOMPARE(task.deadlineOn,before.deadlineOn);QVERIFY(task.todayFocus);
        QVERIFY(!service.moveToView(task.id,before.rowVersion,QStringLiteral("inbox")).ok());QCOMPARE(repo.find(task.id).value,task);
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("someday"));QVERIFY(moved.ok());task=moved.value;
        QVERIFY(QueryService::inView(task,QStringLiteral("someday"),clock.localToday()));QVERIFY(!task.todayFocus);QCOMPARE(task.deadlineOn,before.deadlineOn);
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("anytime"));QVERIFY(moved.ok());task=moved.value;
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),clock.localToday()));QCOMPARE(task.startOn,std::optional<QDate>(clock.localToday()));
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("inbox"));QVERIFY(moved.ok());task=moved.value;
        QVERIFY(QueryService::inView(task,QStringLiteral("inbox"),clock.localToday()));QCOMPARE(task.startOn,std::optional<QDate>(clock.localToday()));QCOMPARE(task.deadlineOn,before.deadlineOn);QVERIFY(task.projectId.isEmpty());QVERIFY(task.areaId.isEmpty());QVERIFY(!task.evening);
        QCOMPARE(task.notes,before.notes);QCOMPARE(task.remindAtUtc,before.remindAtUtc);
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("upcoming"),QDate(2026,10,28));QVERIFY(moved.ok());task=moved.value;
        QVERIFY(QueryService::inView(task,QStringLiteral("upcoming"),clock.localToday()));
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("anytime"));QVERIFY(moved.ok());task=moved.value;QVERIFY(!task.startOn);
        QVERIFY(!service.moveToView(task.id,task.rowVersion,QStringLiteral("unsupported")).ok());QCOMPARE(repo.find(task.id).value,task);
        moved=service.moveToView(task.id,task.rowVersion,QStringLiteral("logbook"));QVERIFY(moved.ok());QCOMPARE(moved.value.status,QStringLiteral("COMPLETED"));
        QVERIFY(!service.moveToView(task.id,moved.value.rowVersion,QStringLiteral("inbox")).ok());
    }
    void batchRollsBackConflictsAndLateDatabaseFailure()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("batch.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        const auto a=service.create(QStringLiteral("first"),QStringLiteral("anytime")).value;
        const auto b=service.create(QStringLiteral("second"),QStringLiteral("anytime")).value;
        QVERIFY(!service.batch({{a.id,1},{b.id,99}},QStringLiteral("complete")).ok());
        QCOMPARE(repo.find(a.id).value,a);QCOMPARE(repo.find(b.id).value,b);
        QSqlQuery trigger(db.connection());
        QVERIFY(trigger.exec(QStringLiteral("CREATE TRIGGER fail_second BEFORE UPDATE ON tasks WHEN OLD.title='second' BEGIN SELECT RAISE(ABORT,'injected'); END")));
        QVERIFY(!service.batch({{a.id,1},{b.id,1}},QStringLiteral("complete")).ok());
        QCOMPARE(repo.find(a.id).value,a);QCOMPARE(repo.find(b.id).value,b);
        QVERIFY(trigger.exec(QStringLiteral("DROP TRIGGER fail_second")));
        TaskPatch patch;patch.startOn=FieldPatch<QDate>::set(QDate(2026,10,23));
        QVERIFY(service.batch({{a.id,1},{b.id,1}},QStringLiteral("edit"),patch).ok());
        QCOMPARE(repo.find(a.id).value.startOn,std::optional<QDate>(QDate(2026,10,23)));
        QVERIFY(service.batch({{a.id,2},{b.id,2}},QStringLiteral("complete")).ok());
        QCOMPARE(QueryService::select(repo.all().value,QStringLiteral("logbook"),clock.localToday()).size(),2);
        QVERIFY(!service.batch({},QStringLiteral("complete")).ok());
    }
    void notesSearchAndHistoryWindow()
    {
        Task t;t.id=QStringLiteral("a");t.title=QStringLiteral("parent");t.notes=QStringLiteral("中英AbC");
        QCOMPARE(QueryService::select({t},QStringLiteral("inbox"),QDate(2026,10,22),QStringLiteral("abc")).size(),1);
        t.projectId=QStringLiteral("p");t.projectStatus=QStringLiteral("OPEN");
        QCOMPARE(QueryService::select({t},QStringLiteral("inbox"),QDate(2026,10,22),QStringLiteral("abc"),false,{},0,0,QStringLiteral("p")).size(),1);
        QVERIFY(QueryService::select({t},QStringLiteral("inbox"),QDate(2026,10,22),QStringLiteral("abc"),false,{},0,0,QStringLiteral("other")).isEmpty());
        t.status=QStringLiteral("COMPLETED");t.completedAtUtc=QStringLiteral("2026-10-01T00:00:00.000Z");
        QVERIFY(QueryService::select({t},QStringLiteral("logbook"),QDate(2026,10,22),{},false,{},0,7).isEmpty());
        QCOMPARE(QueryService::select({t},QStringLiteral("logbook"),QDate(2026,10,22),{},false,{},0,30).size(),1);
    }
    void lifecyclePersistsAndBacksUpWal()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const auto path=dir.filePath(QStringLiteral("tasks.sqlite"));
        QString deletedId;
        {
            SqliteDatabase db; QVERIFY(db.open(path).ok());
            {
                QSqlQuery wal(db.connection());
                QVERIFY(wal.exec(QStringLiteral("PRAGMA wal_autocheckpoint=0")));
                QVERIFY(wal.exec(QStringLiteral("PRAGMA wal_checkpoint(TRUNCATE)")));
            }
            TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
            QVERIFY(!service.create(QStringLiteral("   ")).ok());
            for(int i=0;i<20;++i) QVERIFY(service.create(QStringLiteral("中文 task %1").arg(i)).ok());
            QCOMPARE(repo.all().value.size(),20);
            auto task=repo.all().value.first();
            auto completed=service.changeState(task.id,task.rowVersion,QStringLiteral("complete"));
            QVERIFY(completed.ok()); QCOMPARE(completed.value.status,QStringLiteral("COMPLETED"));
            QVERIFY(!completed.value.completedAtUtc.isEmpty());
            auto reopened=service.changeState(task.id,completed.value.rowVersion,QStringLiteral("reopen"));
            QVERIFY(reopened.ok()); QVERIFY(reopened.value.completedAtUtc.isEmpty());
            auto cancelled=service.changeState(task.id,reopened.value.rowVersion,QStringLiteral("cancel"));
            QVERIFY(cancelled.ok());QCOMPARE(cancelled.value.status,QStringLiteral("CANCELLED"));QVERIFY(!cancelled.value.cancelledAtUtc.isEmpty());
            auto activeAgain=service.changeState(task.id,cancelled.value.rowVersion,QStringLiteral("reopen"));
            QVERIFY(activeAgain.ok());QVERIFY(activeAgain.value.cancelledAtUtc.isEmpty());
            auto deleted=service.changeState(task.id,activeAgain.value.rowVersion,QStringLiteral("delete"));
            QVERIFY(deleted.ok()); deletedId=task.id;
            QCOMPARE(repo.all().value.size(),20); // Soft deletion retains the row.
            QCOMPARE(QueryService::select(repo.all().value,QStringLiteral("inbox"),clock.localToday()).size(),19);
            QVERIFY(QFileInfo(path+QStringLiteral("-wal")).size()>0);
            auto snapshot=db.createSnapshot(dir.filePath(QStringLiteral("backups"))); QVERIFY(snapshot.ok());
            SqliteDatabase backup; QVERIFY(backup.open(snapshot.value).ok());
            TaskRepository copied(backup.connection()); QCOMPARE(copied.all().value,repo.all().value);
        }
        SqliteDatabase reopened; QVERIFY(reopened.open(path).ok());
        TaskRepository repo(reopened.connection()); FixedClock clock; TaskService service(repo,clock);
        QCOMPARE(repo.all().value.size(),20);
        auto deleted=repo.find(deletedId); QVERIFY(!deleted.value.deletedAtUtc.isEmpty());
        QVERIFY(service.changeState(deletedId,deleted.value.rowVersion,QStringLiteral("restore")).ok());
        QCOMPARE(QueryService::select(repo.all().value,QStringLiteral("inbox"),clock.localToday()).size(),20);
    }

    void validationPatchAndConflictAreAtomic()
    {
        QTemporaryDir dir; SqliteDatabase db; QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
        const auto emoji=QString::fromUcs4(U"😀");
        QVERIFY(service.create(emoji.repeated(512)).ok());
        QVERIFY(!service.create(emoji.repeated(513)).ok());
        auto task=service.create(QStringLiteral("  Date task  ")).value;
        QCOMPARE(task.title,QStringLiteral("Date task"));
        TaskPatch patch;
        patch.startOn=FieldPatch<QDate>::set(QDate(2026,10,24));
        patch.deadlineOn=FieldPatch<QDate>::set(QDate(2026,10,23));
        QVERIFY(!service.edit(task.id,task.rowVersion,patch).ok());
        QCOMPARE(repo.find(task.id).value,task);
        patch.deadlineOn=FieldPatch<QDate>::set(QDate(2026,10,25));
        auto updated=service.edit(task.id,task.rowVersion,patch); QVERIFY(updated.ok());
        QCOMPARE(updated.value.planningState,QStringLiteral("ACTIVE"));
        QCOMPARE(service.edit(task.id,task.rowVersion,patch).error.code,ErrorCode::Conflict);
        TaskPatch clear; clear.startOn=FieldPatch<QDate>::clear();
        auto cleared=service.edit(task.id,updated.value.rowVersion,clear); QVERIFY(cleared.ok());
        QVERIFY(!cleared.value.startOn); QVERIFY(cleared.value.deadlineOn.has_value());
        TaskPatch invalid; invalid.startOn=FieldPatch<QDate>::set(QDate(2026,2,30));
        QVERIFY(!service.edit(task.id,cleared.value.rowVersion,invalid).ok());
        QCOMPARE(repo.find(task.id).value,cleared.value);
        QSqlQuery query(db.connection()); QVERIFY(query.exec(QStringLiteral("PRAGMA query_only=ON")));
        const auto count=repo.all().value.size();
        QVERIFY(!service.create(QStringLiteral("must not persist")).ok());
        QCOMPARE(repo.all().value.size(),count);
    }

    void focusAndSomedayKeepDistinctDateSemantics()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("plan"),QStringLiteral("upcoming"));QVERIFY(task.ok());
        QCOMPARE(*task.value.startOn,clock.localToday().addDays(1));
        TaskPatch someday; someday.planningState=FieldPatch<QString>::set(QStringLiteral("SOMEDAY"));
        someday.todayFocus=FieldPatch<bool>::set(true);
        auto paused=service.edit(task.value.id,task.value.rowVersion,someday);QVERIFY(paused.ok());
        QCOMPARE(paused.value.startOn,task.value.startOn); // Paused dates must not be rewritten.
        QVERIFY(!QueryService::inView(paused.value,QStringLiteral("today"),clock.localToday()));
        TaskPatch activate;activate.planningState=FieldPatch<QString>::set(QStringLiteral("ACTIVE"));
        activate.notes=FieldPatch<QString>::set(QString());
        auto today=service.edit(task.value.id,paused.value.rowVersion,activate);QVERIFY(today.ok());
        QCOMPARE(*today.value.startOn,clock.localToday());QVERIFY(today.value.notes.isEmpty());
        QCOMPARE(QueryService::select({paused.value},QStringLiteral("inbox"),clock.localToday(),QStringLiteral("PLAN")).size(),1);
        auto completed=service.changeState(task.value.id,today.value.rowVersion,QStringLiteral("complete"));QVERIFY(completed.ok());
        auto duplicate=service.changeState(task.value.id,completed.value.rowVersion,QStringLiteral("complete"));
        QVERIFY(duplicate.ok());QCOMPARE(duplicate.value.rowVersion,completed.value.rowVersion);
        QFile blocked(dir.filePath(QStringLiteral("blocked")));QVERIFY(blocked.open(QIODevice::WriteOnly));blocked.close();
        QVERIFY(!db.createSnapshot(blocked.fileName()+QStringLiteral("/backups")).ok());
        QCOMPARE(repo.find(task.value.id).value,completed.value);
    }

    void repositoryJoinsProjectVisibility()
    {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath(QStringLiteral("tasks.sqlite"))).ok());
        TaskRepository repo(db.connection());FixedClock clock;TaskService service(repo,clock);
        const auto task=service.create(QStringLiteral("project task"),QStringLiteral("today"));QVERIFY(task.ok());
        QSqlQuery sql(db.connection());
        QVERIFY(sql.exec(QStringLiteral("INSERT INTO projects(id,title,created_at_utc,updated_at_utc) VALUES('p','test','2026-10-22T00:00:00Z','2026-10-22T00:00:00Z')")));
        sql.prepare(QStringLiteral("UPDATE tasks SET project_id='p' WHERE id=?"));sql.addBindValue(task.value.id);QVERIFY(sql.exec());
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET planning_state='SOMEDAY' WHERE id='p'")));
        QVERIFY(!QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("someday"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET planning_state='ACTIVE',status='COMPLETED' WHERE id='p'")));
        QVERIFY(!QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
        QVERIFY(sql.exec(QStringLiteral("UPDATE projects SET status='OPEN' WHERE id='p'")));
        QVERIFY(QueryService::inView(repo.find(task.value.id).value,QStringLiteral("today"),clock.localToday()));
    }

    void unifiedViewsHonorDatesAndProjectState()
    {
        const QDate today(2026,10,22);
        Task task; task.planningState=QStringLiteral("ACTIVE"); task.title=QStringLiteral("中文 Review");
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.deadlineOn=today.addDays(1);
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("upcoming"),today));
        task.startOn=today.addDays(1);
        QVERIFY(QueryService::inView(task,QStringLiteral("upcoming"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("anytime"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today.addDays(1)));
        task.startOn=today.addDays(-1);
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("anytime"),today));
        task.planningState=QStringLiteral("SOMEDAY");
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        QVERIFY(QueryService::inView(task,QStringLiteral("someday"),today));
        task.planningState=QStringLiteral("INBOX");
        QVERIFY(QueryService::inView(task,QStringLiteral("inbox"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.planningState=QStringLiteral("ACTIVE"); task.projectId=QStringLiteral("project");
        task.projectPlanning=QStringLiteral("SOMEDAY"); task.projectStatus=QStringLiteral("OPEN");
        QVERIFY(QueryService::inView(task,QStringLiteral("someday"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.projectPlanning=QStringLiteral("ACTIVE"); task.projectStatus=QStringLiteral("COMPLETED");
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
        task.projectStatus=QStringLiteral("OPEN");
        QVERIFY(QueryService::inView(task,QStringLiteral("today"),today));
        QCOMPARE(QueryService::select({task},QStringLiteral("today"),today,QStringLiteral("review")).size(),1);
        task.status=QStringLiteral("COMPLETED");
        QVERIFY(QueryService::inView(task,QStringLiteral("logbook"),today));
        QVERIFY(!QueryService::inView(task,QStringLiteral("today"),today));
    }

    void unknownAndFutureSchemasAreNotOverwritten()
    {
        QTemporaryDir dir; const auto path=dir.filePath(QStringLiteral("tasks.sqlite"));
        {
            SqliteDatabase db; QVERIFY(db.open(path).ok());
            TaskRepository repo(db.connection()); FixedClock clock; TaskService service(repo,clock);
            QVERIFY(service.create(QStringLiteral("retained")).ok());
            QSqlQuery query(db.connection()); QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=999")));
        }
        SqliteDatabase future; QCOMPARE(future.open(path).error.key,QStringLiteral("schemaTooNew"));
        auto raw=future.connection(); QVERIFY(raw.open());
        QSqlQuery query(raw); QVERIFY(query.exec(QStringLiteral("SELECT count(*) FROM tasks"))); QVERIFY(query.next()); QCOMPARE(query.value(0).toInt(),1); query.finish();
        QVERIFY(query.exec(QStringLiteral("PRAGMA user_version=0"))); query.finish(); raw.close();
        SqliteDatabase unknown; QCOMPARE(unknown.open(path).error.key,QStringLiteral("unknownSchema"));
    }
};

QTEST_GUILESS_MAIN(TaskServiceTest)
#include "TaskServiceTest.moc"
