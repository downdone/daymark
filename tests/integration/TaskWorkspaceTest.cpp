#include "models/TaskWorkspace.h"
#include "persistence/SqliteDatabase.h"
#include "persistence/TaskRepository.h"
#include <QAbstractItemModelTester>
#include <QTemporaryDir>
#include <QtTest>
#include <QSqlQuery>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <memory>

using namespace daymark;
class TaskWorkspaceTest : public QObject {
    Q_OBJECT
private slots:
    void ordinaryListsPersistManualOrderAcrossDateDifferences(){
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("ordinary-order.sqlite"));QTRY_VERIFY(!w.busy());QVERIFY(w.createTask("dated"));QTRY_VERIFY(!w.busy());const auto first=w.tasks()->items().first().id;
        QVERIFY(w.createTask("undated"));QTRY_VERIFY(!w.busy());QString second;for(const auto& task:w.tasks()->items())if(task.title=="undated")second=task.id;QVERIFY(!second.isEmpty());QVERIFY(w.saveTask(first,1,"dated","","ACTIVE",w.dateOffset(2),w.dateOffset(3),false));QTRY_VERIFY(!w.busy());
        for(const auto& view:{QStringLiteral("inbox"),QStringLiteral("all")}){w.setView(view);QVERIFY(w.taskReorderingAllowed());const auto from=w.tasks()->items().first().id,to=w.tasks()->items().last().id;QVERIFY(w.dropTask(from,to));QTRY_VERIFY(!w.busy());QCOMPARE(w.taskIndex(from),1);w.reload();QTRY_VERIFY(!w.busy());QCOMPARE(w.taskIndex(from),1);}
        w.setView("scheduled");QVERIFY(!w.taskReorderingAllowed());QVERIFY(!w.dropTask(first,second));w.setView("inbox");w.setSearch("dated");QVERIFY(!w.taskReorderingAllowed());QVERIFY(!w.dropTask(first,second));w.setSearch("");
        QVERIFY(w.saveDetails(first,w.task(first).value("rowVersion").toLongLong(),{{"pinned",true}}));QTRY_VERIFY(!w.busy());QVERIFY(!w.dropTask(first,second));QVERIFY(!w.canMoveTask(first,1));
    }
    void legacyHiddenCategoryUsesArchiveWithoutLosingMetadata() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("category-archive.sqlite")));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("legacy")},{"status",QStringLiteral("COMPLETED")},{"planningState",QStringLiteral("SOMEDAY")},{"startOn",QStringLiteral("2026-10-01")},{"description",QStringLiteral("keep")}}));QTRY_VERIFY(!w.busy());
        auto item=w.categoryTree().first().toMap();QVERIFY(item.value("archived").toBool());QVERIFY(item.value("hidden").toBool());QVERIFY(w.navigationCatalog().isEmpty());
        item.insert("archived",false);item.insert("status",QStringLiteral("OPEN"));item.insert("planningState",QStringLiteral("ACTIVE"));QVERIFY(w.saveCatalog(item));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
        QCOMPARE(w.navigationCatalog().size(),1);const auto restored=w.categoryTree().first().toMap();QCOMPARE(restored.value("id"),item.value("id"));QCOMPARE(restored.value("startOn"),item.value("startOn"));QCOMPARE(restored.value("description"),item.value("description"));QVERIFY(!restored.value("archived").toBool());
    }
    void categoryTreeMovePreservesMetadataAndTaskOwnership() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("tree-move.sqlite")));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("Work")},{"description",QStringLiteral("keep me")},{"startOn",QStringLiteral("2026-10-01")}}));QTRY_VERIFY(!w.busy());const auto root=w.categoryTree().first().toMap();
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("Release")},{"parentId",root.value("id")}}));QTRY_VERIFY(!w.busy());auto child=w.categoryTree().last().toMap();
        QCOMPARE(child.value("path").toString(),QStringLiteral("Work / Release"));QCOMPARE(child.value("parentTitle"),root.value("title"));QCOMPARE(w.categoryTree().first().toMap().value("childCount").toInt(),1);
        w.setView(QStringLiteral("project:")+child.value("id").toString());QVERIFY(w.createTask(QStringLiteral("owned")));QTRY_VERIFY(!w.busy());const auto task=w.tasks()->items().first();
        QVERIFY(w.reparentCategory(root.value("id").toString(),root.value("rowVersion").toLongLong(),child.value("id").toString()));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.task(task.id).value("projectId"),child.value("id"));
        QVERIFY(w.reparentCategory(child.value("id").toString(),child.value("rowVersion").toLongLong(),QString()));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
        for(const auto& value:w.categoryTree())if(value.toMap().value("id")==child.value("id"))child=value.toMap();QCOMPARE(child.value("depth").toInt(),0);QCOMPARE(child.value("path"),child.value("title"));
        QVERIFY(w.reparentCategory(root.value("id").toString(),root.value("rowVersion").toLongLong(),child.value("id").toString()));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
        const auto moved=w.categoryTree().last().toMap();QCOMPARE(moved.value("id"),root.value("id"));QCOMPARE(moved.value("description"),root.value("description"));QCOMPARE(moved.value("startOn"),root.value("startOn"));
        QVERIFY(w.reparentCategory(root.value("id").toString(),root.value("rowVersion").toLongLong(),QString()));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.categoryTree().last().toMap(),moved);
        auto archived=child;archived.insert("archived",true);QVERIFY(w.saveCatalog(archived));QTRY_VERIFY(!w.busy());QVERIFY(w.navigationCatalog().isEmpty());QCOMPARE(w.categoryTree().size(),2);
        QCOMPARE(w.task(task.id).value("projectId"),child.value("id"));
    }
    void smartViewChildrenStayIndependentOfOrdinaryTreePreference() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("child-tree.sqlite")));QTRY_VERIFY(!w.busy());w.setView("today");QVERIFY(w.createTask("parent"));QTRY_VERIFY(!w.busy());const auto parent=w.tasks()->items().first();QVERIFY(w.createSubtask("child",parent.id));QTRY_VERIFY(!w.busy());QCOMPARE(w.tasks()->rowCount(),1);QCOMPARE(w.tasks()->items().first().pendingSubtasks,1);const auto child=w.subtaskChoices(parent.id).first().toMap();
        w.setView("all");QCOMPARE(w.tasks()->rowCount(),2);QCOMPARE(w.tasks()->items()[1].listDepth,1);w.setTaskChildrenExpanded(parent.id,false);QCOMPARE(w.tasks()->rowCount(),1);w.setTaskChildrenExpanded(parent.id,true);w.setPreference("listShowSubtasks",false);QCOMPARE(w.tasks()->rowCount(),1);
        QVERIFY(w.saveTask(child.value("taskId").toString(),child.value("rowVersion").toLongLong(),"child",QString(),"ACTIVE",QDate::currentDate().toString(Qt::ISODate),QString(),false));QTRY_VERIFY(!w.busy());w.setView("today");QCOMPARE(w.tasks()->rowCount(),2);QCOMPARE(w.tasks()->items()[1].listDepth,0);w.setTaskChildrenExpanded(parent.id,false);QCOMPARE(w.tasks()->rowCount(),2);QCOMPARE(w.viewCounts().value("today").toInt(),2);w.setView("scheduled");QCOMPARE(w.tasks()->rowCount(),2);
        QVERIFY(w.changeState(parent.id,w.task(parent.id).value("rowVersion").toLongLong(),"delete"));QTRY_VERIFY(!w.busy());w.setView("trash");QCOMPARE(w.tasks()->rowCount(),2);QCOMPARE(w.tasks()->items().first().id,parent.id);QCOMPARE(w.tasks()->items()[1].listDepth,1);QCOMPARE(w.viewCounts().value("trash").toInt(),2);QVERIFY(w.undoDelete());QTRY_VERIFY(!w.busy());QCOMPARE(w.tasks()->rowCount(),0);w.setView("all");w.setTaskChildrenExpanded(parent.id,true);w.setPreference("listShowSubtasks",true);QCOMPARE(w.tasks()->rowCount(),2);
    }
    void parentPickerOnlyOffersLegalRoots() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath("parent-options.sqlite"));QTRY_VERIFY(!w.busy());w.setView("all");QVERIFY(w.createTask("root"));QTRY_VERIFY(!w.busy());const auto root=w.tasks()->items().first();
        QVERIFY(w.createSubtask("child",root.id));QTRY_VERIFY(!w.busy());const auto child=w.subtaskChoices(root.id).first().toMap();
        QVERIFY(w.parentTaskChoices(root.id,QString()).isEmpty());const auto choices=w.parentTaskChoices(child.value("taskId").toString(),QString());QCOMPARE(choices.size(),1);QCOMPARE(choices.first().toMap().value("id").toString(),root.id);
        const auto before=w.tasks()->rowCount();QVERIFY(w.createSubtask("forbidden",child.value("taskId").toString()));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.tasks()->rowCount(),before);QVERIFY(w.subtaskChoices(child.value("taskId").toString()).isEmpty());
    }
    void categoriesProjectTwoLevelsAndPromoteWithoutMovingTasks() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("categories.sqlite")));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("Work")}}));QTRY_VERIFY(!w.busy());const auto root=w.catalogOf(QStringLiteral("project")).first().toMap();
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("Release")},{"parentId",root.value("id")}}));QTRY_VERIFY(!w.busy());
        auto nav=w.navigationCatalog();QCOMPARE(nav.size(),2);QCOMPARE(nav[0].toMap().value("title").toString(),QStringLiteral("Work"));QCOMPARE(nav[1].toMap().value("depth").toInt(),1);
        const auto child=nav[1].toMap();w.setView(QStringLiteral("project:")+child.value("id").toString());QVERIFY(w.createTask(QStringLiteral("same task")));QTRY_VERIFY(!w.busy());const auto id=w.tasks()->items().first().id;
        w.setView(QStringLiteral("project:")+root.value("id").toString());QCOMPARE(w.tasks()->rowCount(),1);
        auto promoted=child;promoted.insert(QStringLiteral("parentId"),QString());QVERIFY(w.saveCatalog(promoted));QTRY_VERIFY(!w.busy());QCOMPARE(w.tasks()->rowCount(),0);QCOMPARE(w.task(id).value("projectId"),child.value("id"));
        for(const auto& value:w.navigationCatalog())QCOMPARE(value.toMap().value("depth").toInt(),0);
    }
    void taskAndEventRangesShareOverflowWithoutLosingAgendaEntries() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("mixed-ranges.sqlite")));QTRY_VERIFY(!w.busy());
        for(int i=0;i<4;++i){QVERIFY(w.createTask(QStringLiteral("task range %1").arg(i)));QTRY_VERIFY(!w.busy());QString id;for(const auto& task:w.tasks()->items())if(task.title==QStringLiteral("task range %1").arg(i))id=task.id;QVERIFY(!id.isEmpty());
            QVERIFY(w.saveTask(id,1,QStringLiteral("task range %1").arg(i),QString(),QStringLiteral("ACTIVE"),QStringLiteral("2024-02-28"),QStringLiteral("2024-03-03"),false));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());}
        QVERIFY(w.saveEvent({{"title",QStringLiteral("event range")},{"startDate",QStringLiteral("2024-02-28")},{"endDate",QStringLiteral("2024-03-03")}}));QTRY_VERIFY(!w.busy());
        bool checked=false;
        for(const auto& value:w.calendarDaysFor(QStringLiteral("2024-02-29"),false)){
            const auto day=value.toMap();if(day.value("date")!=QStringLiteral("2024-02-29"))continue;checked=true;
            QCOMPARE(day.value("entries").toList().size(),4);QCOMPARE(day.value("events").toList().size(),1);
            QCOMPARE(day.value("hiddenTaskBands").toInt()+day.value("hiddenEventBands").toInt(),2);QCOMPARE(day.value("bandLanes").toInt(),3);
        }
        QVERIFY(checked);
    }
    void taskRangesSplitAcrossWeeksAndRemainOneTask() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("task-range.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.createTask(QStringLiteral("span")));QTRY_VERIFY(!w.busy());
        const auto id=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();QVERIFY(w.saveTask(id,1,QStringLiteral("span"),QString(),QStringLiteral("ACTIVE"),QStringLiteral("2026-10-24"),QStringLiteral("2026-11-20"),false));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
        const auto task=w.task(id);QVariantList segments;int interior=0;
        for(const auto& value:w.calendarDaysFor(QStringLiteral("2026-10-24"),false)){
            const auto day=value.toMap();for(const auto& band:day.value("bands").toList())if(band.toMap().value("taskId")==id)segments.append(band);
            if(day.value("date")==QStringLiteral("2026-11-01")){const auto entries=day.value("entries").toList();QCOMPARE(entries.size(),1);QCOMPARE(entries.first().toMap().value("kind").toString(),QStringLiteral("range"));++interior;}
        }
        QCOMPARE(interior,1);QCOMPARE(segments.size(),3);QCOMPARE(segments.first().toMap().value("column").toInt(),5);QCOMPARE(segments.first().toMap().value("span").toInt(),2);
        QVERIFY(segments.first().toMap().value("startsHere").toBool());QVERIFY(!segments.last().toMap().value("endsHere").toBool());
        segments.clear();for(const auto& value:w.calendarDaysFor(QStringLiteral("2026-11-01"),false))for(const auto& band:value.toMap().value("bands").toList())if(band.toMap().value("taskId")==id)segments.append(band);
        QCOMPARE(segments.size(),4);QVERIFY(!segments.first().toMap().value("startsHere").toBool());QVERIFY(segments.last().toMap().value("endsHere").toBool());
        QCOMPARE(w.task(id),task);
        QVERIFY(w.saveTask(id,task.value("rowVersion").toLongLong(),QStringLiteral("span"),QString(),QStringLiteral("SOMEDAY"),QStringLiteral("2026-10-24"),QStringLiteral("2026-11-20"),false));QTRY_VERIFY(!w.busy());
        for(const auto& day:w.calendarDaysFor(QStringLiteral("2026-11-01"),false))QVERIFY(day.toMap().value("bands").toList().isEmpty());
    }
    void searchNotifiesQmlReorderingPermissionBeforeDebounce() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("search-binding.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        w.setView(QStringLiteral("today"));QVERIFY(w.createTask(QStringLiteral("needle")));QTRY_VERIFY(!w.busy());
        QQmlEngine engine;engine.rootContext()->setContextProperty(QStringLiteral("controller"),&w);
        QQmlComponent component(&engine);component.setData("import QtQml\nQtObject { property bool canSort: controller.taskReorderingAllowed }",QUrl());
        std::unique_ptr<QObject> binding(component.create());QVERIFY2(binding,qPrintable(component.errorString()));QVERIFY(binding->property("canSort").toBool());
        const auto count=w.tasks()->rowCount();w.setSearch(QStringLiteral("not present"));
        QVERIFY(!binding->property("canSort").toBool());QCOMPARE(w.tasks()->rowCount(),count);
        // A state refresh while filtering used to strand the delegate at false.
        w.setInteractionActive(true);w.setInteractionActive(false);w.setSearch(QString());
        QVERIFY(binding->property("canSort").toBool());QCOMPARE(w.tasks()->rowCount(),count);
        QTest::qWait(300);QVERIFY(binding->property("canSort").toBool());QCOMPARE(w.tasks()->rowCount(),count);
    }
    void multiDayBandsSplitAndAllocateWithoutOverlap() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("bands.sqlite")));QTRY_VERIFY(!w.busy());
        for(const auto& title:{QStringLiteral("trip"),QStringLiteral("overlap"),QStringLiteral("third"),QStringLiteral("fourth")}) {
            QVERIFY(w.saveEvent({{"title",title},{"startDate",QStringLiteral("2026-12-30")},{"endDate",QStringLiteral("2027-01-06")}}));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
        }
        const auto days=w.calendarDaysFor(QStringLiteral("2027-01-01"),false);QVariantList segments;bool overflow=false;
        for(const auto& value:days){const auto day=value.toMap();segments.append(day.value("bands").toList());if(day.value("hiddenBands").toInt()>0)overflow=true;QVERIFY(day.value("bandLanes").toInt()<=3);}
        QCOMPARE(segments.size(),8);QVERIFY(overflow);
        for(const auto& a:segments){const auto x=a.toMap();QVERIFY(x.value("span").toInt()>0);QVERIFY(x.value("column").toInt()+x.value("span").toInt()<=7);
            for(const auto& b:segments){const auto y=b.toMap();if(x==y || x.value("week")!=y.value("week"))continue;
                const int maskX=((1<<x.value("span").toInt())-1)<<x.value("column").toInt(),maskY=((1<<y.value("span").toInt())-1)<<y.value("column").toInt();
                if(maskX&maskY)QVERIFY(x.value("lane")!=y.value("lane"));
            }
        }
        const auto first=segments.first().toMap();QCOMPARE(first.value("span").toInt(),5);QVERIFY(first.value("startsHere").toBool());QVERIFY(!first.value("endsHere").toBool());
        w.setSundayFirst(true);const auto sunday=w.calendarDaysFor(QStringLiteral("2027-01-01"),false);QCOMPARE(sunday.first().toMap().value("date").toString(),QStringLiteral("2026-12-27"));
        QVERIFY(w.saveEvent({{"title",QStringLiteral("midnight")},{"allDay",false},{"startDate",QStringLiteral("2027-01-02")},{"endDate",QStringLiteral("2027-01-03")},{"startTime",QStringLiteral("23:00")},{"endTime",QStringLiteral("00:00")},{"zone",w.systemTimeZone()}}));QTRY_VERIFY(!w.busy());
        for(const auto& value:w.calendarDaysFor(QStringLiteral("2027-01-02"),true)){const auto day=value.toMap();for(const auto& event:day.value("events").toList())if(event.toMap().value("title")==QStringLiteral("midnight")){QCOMPARE(day.value("date").toString(),QStringLiteral("2027-01-02"));QVERIFY(!event.toMap().value("multiDay").toBool());}}
        QVERIFY(w.saveEvent({{"title",QStringLiteral("birthday")},{"startDate",QStringLiteral("2026-10-07")},{"endDate",QStringLiteral("2026-10-07")},{"repeatFrequency",QStringLiteral("YEARLY")},{"alertMinutes",1440}}));QTRY_VERIFY(!w.busy());
        const auto recurring=w.recurringEvents();QCOMPARE(recurring.size(),1);QCOMPARE(recurring.first().toMap().value("title").toString(),QStringLiteral("birthday"));QVERIFY(recurring.first().toMap().value("ruleLabel").toString().contains(QStringLiteral("年")));
    }
    void menuAvailabilityReusesTenThousandVisibleTasks() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("menu-benchmark.sqlite"));
        {SqliteDatabase db;QVERIFY(db.open(path).ok());auto connection=db.connection();QVERIFY(connection.transaction());QSqlQuery query(connection);query.prepare(QStringLiteral("INSERT INTO tasks(id,title,planning_state,today_focus,sort_order,created_at_utc,updated_at_utc) VALUES(?,?,'ACTIVE',1,?,?,?)"));const auto now=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        for(int i=0;i<10000;++i){query.bindValue(0,QStringLiteral("menu-%1").arg(i));query.bindValue(1,QStringLiteral("menu item"));query.bindValue(2,i);query.bindValue(3,now);query.bindValue(4,now);QVERIFY(query.exec());}QVERIFY(connection.commit());}
        TaskWorkspace w;w.start(path);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),10000);QVERIFY(w.ready());w.setView(QStringLiteral("today"));QCOMPARE(w.tasks()->rowCount(),10000);
        const auto id=w.tasks()->data(w.tasks()->index(9999),TaskListModel::Id).toString();QElapsedTimer timer;timer.start();
        for(int i=0;i<50;++i){QVERIFY(w.canMoveTask(id,-1));QVERIFY(!w.canMoveTask(id,1));}
        const auto micros=timer.nsecsElapsed()/1000.0;qInfo()<<"10k menu availability, 50 open pairs, mean us:"<<micros/50.0;QVERIFY2(micros<500000,"Repeated menu availability exceeded 10 ms per pair");
    }
    void catalogCreationReturnsIdentityAndKeepsTaskUnchanged() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("create.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QVERIFY(w.createTask(QStringLiteral("draft owner")));QTRY_VERIFY(!w.busy());const auto id=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();const auto before=w.task(id);
        QSignalSpy saved(&w,&TaskWorkspace::catalogSaved);QVERIFY(w.saveCatalog({{"kind",QStringLiteral("tag")},{"title",QStringLiteral("here")}}));QTRY_VERIFY(!w.busy());
        QCOMPARE(saved.size(),1);const auto tag=w.catalogOf(QStringLiteral("tag")).first().toMap();QCOMPARE(saved.first().at(0).toString(),tag.value("id").toString());QCOMPARE(saved.first().at(1).toString(),QStringLiteral("tag"));QCOMPARE(w.task(id),before);
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("tag")},{"title",QStringLiteral("here")}}));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(saved.size(),1);QCOMPARE(w.task(id),before);
    }
    void navigationSectionPreferencesPersistIndependently() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("navigation.sqlite"));
        {TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());
            for(const auto& key:{QStringLiteral("sidebarTasksExpanded"),QStringLiteral("sidebarPlanningExpanded"),QStringLiteral("sidebarLibraryExpanded"),QStringLiteral("sidebarTagsExpanded")})QVERIFY(w.preferences().value(key).toBool());
            QVERIFY(w.setPreference(QStringLiteral("sidebarTasksExpanded"),false));QVERIFY(w.setPreference(QStringLiteral("sidebarLibraryExpanded"),false));QVERIFY(!w.setPreference(QStringLiteral("sidebarPlanningExpanded"),2));}
        TaskWorkspace restored;restored.start(path);QTRY_VERIFY(!restored.busy());
        QVERIFY(!restored.preferences().value("sidebarTasksExpanded").toBool());QVERIFY(restored.preferences().value("sidebarPlanningExpanded").toBool());QVERIFY(!restored.preferences().value("sidebarLibraryExpanded").toBool());
        QVERIFY(restored.createTask(QStringLiteral("validation")));QTRY_VERIFY(!restored.busy());const auto item=restored.tasks()->items().first();
        QVERIFY(!restored.saveTaskWithDetails(item.id,item.rowVersion,item.title,QString(),QStringLiteral("ACTIVE"),QString(),QString(),true,{{"remindAtLocal",QStringLiteral("invalid")}}));
        QVERIFY(restored.validationError());restored.clearError();QVERIFY(!restored.validationError());restored.settingsFailed();QVERIFY(!restored.validationError());QVERIFY(!restored.errorText().isEmpty());
    }
    void reducedMotionPreferenceIsExplicitAndPersistent() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("motion.sqlite"));
        {TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());QVERIFY(!w.preferences().value("reduceMotion").toBool());QVERIFY(!w.setPreference(QStringLiteral("reduceMotion"),QStringLiteral("yes")));QVERIFY(w.setPreference(QStringLiteral("reduceMotion"),true));}
        TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());QVERIFY(w.preferences().value("reduceMotion").toBool());
    }

    void highContrastPreferencePersistsAndRejectsInvalidValue() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("appearance.sqlite"));
        {TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());QVERIFY(!w.preferences().value("highContrast").toBool());QVERIFY(!w.setPreference(QStringLiteral("highContrast"),QStringLiteral("yes")));QVERIFY(w.setPreference(QStringLiteral("highContrast"),true));}
        TaskWorkspace restored;restored.start(path);QTRY_VERIFY(!restored.busy());QVERIFY(restored.preferences().value("highContrast").toBool());
    }
    void calendarOccurrenceOrderAndReadOnlyPreview() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("browse.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QVariantMap event{{"title",QStringLiteral("later weekly")},{"allDay",false},{"startDate",w.dateOffset(-7)},{"endDate",w.dateOffset(-7)},{"startTime",QStringLiteral("16:00")},{"endTime",QStringLiteral("17:00")},{"zone",w.systemTimeZone()},{"repeatFrequency",QStringLiteral("WEEKLY")},{"alertMinutes",15}};
        QVERIFY(w.saveEvent(event));QTRY_VERIFY(!w.busy());
        event.insert("title",QStringLiteral("earlier once"));event.insert("startDate",w.dateOffset(0));event.insert("endDate",w.dateOffset(0));event.insert("startTime",QStringLiteral("09:00"));event.insert("endTime",QStringLiteral("10:00"));event.insert("repeatFrequency",QStringLiteral("NONE"));
        QVERIFY(w.saveEvent(event));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveEvent({{"title",QStringLiteral("all day")},{"startDate",w.dateOffset(0)},{"endDate",w.dateOffset(0)}}));QTRY_VERIFY(!w.busy());
        QVariantList events;for(const auto& day:w.calendarDaysFor(w.dateOffset(0),false))if(day.toMap().value("date")==w.dateOffset(0))events=day.toMap().value("events").toList();
        QCOMPARE(events.size(),3);QCOMPARE(events[0].toMap().value("title").toString(),QStringLiteral("all day"));QCOMPARE(events[1].toMap().value("title").toString(),QStringLiteral("earlier once"));QCOMPARE(events[2].toMap().value("title").toString(),QStringLiteral("later weekly"));
        const auto instance=events[2].toMap();const auto id=instance.value("taskId").toString(),key=instance.value("occurrenceKey").toString();
        const auto snapshot=w.calendarEvent(id);const auto preview=w.calendarEventPreview(id,key);const auto timeline=preview.value("timeline").toMap();QVERIFY(timeline.value("start").toDouble()>=0);QVERIFY(timeline.value("end").toDouble()<=1);QVERIFY(timeline.value("start").toDouble()<timeline.value("end").toDouble());QCOMPARE(preview.value("startDate").toString(),w.dateOffset(0));QVERIFY(preview.value("scopeLabel").toString().contains(QStringLiteral("本次")));QVERIFY(preview.value("alertLabel").toString().contains(QStringLiteral("15")));
        QCOMPARE(w.calendarEvent(id),snapshot);QVERIFY(w.calendarEventPreview(id,QStringLiteral("invalid")).isEmpty());QVERIFY(w.calendarEventPreview(QStringLiteral("missing"),key).isEmpty());
        QCOMPARE(w.calendarEventPreview(id).value("startDate"),instance.value("startDate"));
        auto changed=snapshot;changed.insert("startTime",QStringLiteral("18:00"));changed.insert("endTime",QStringLiteral("19:00"));QVERIFY(w.saveEvent(changed));QTRY_VERIFY(!w.busy());QVERIFY(w.calendarEventPreview(id,key).isEmpty());
        const auto latest=w.calendarEvent(id);QVERIFY(w.deleteEvent(id,latest.value("rowVersion").toLongLong()));QTRY_VERIFY(!w.busy());QVERIFY(w.calendarEventPreview(id).isEmpty());
    }
    void independentCalendarSharesDataWithoutChangingMainView() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("calendar.sqlite"));
        {TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        w.setView(QStringLiteral("today"));const auto title=w.calendarTitle();
        QCOMPARE(w.calendarDaysFor(QStringLiteral("2028-02-29"),false).size(),42);
        QCOMPARE(w.calendarDaysFor(QStringLiteral("2028-02-29"),true).size(),7);
        QCOMPARE(w.shiftCalendar(QStringLiteral("2028-01-31"),1,false),QStringLiteral("2028-02-29"));
        QVERIFY(w.calendarDaysFor(QStringLiteral("invalid"),false).isEmpty());
        QVERIFY(w.createForDate(QStringLiteral("shared calendar item"),w.dateOffset(0)));QTRY_VERIFY(!w.busy());
        bool found=false;for(const auto& day:w.calendarDaysFor(w.dateOffset(0),false))for(const auto& item:day.toMap().value(QStringLiteral("entries")).toList())if(item.toMap().value(QStringLiteral("title")).toString()==QStringLiteral("shared calendar item"))found=true;
        QVERIFY(found);QCOMPARE(w.view(),QStringLiteral("today"));QCOMPARE(w.calendarTitle(),title);
        QVERIFY(w.setPreference(QStringLiteral("desktopCalendarVisible"),true));
        QVERIFY(w.setPreference(QStringLiteral("desktopCalendarOpacity"),70));
        QVERIFY(!w.setPreference(QStringLiteral("desktopCalendarOpacity"),49));
        QVERIFY(!w.setPreference(QStringLiteral("desktopCalendarOpacity"),101));}
        TaskWorkspace restored;restored.start(path);QTRY_VERIFY(!restored.busy());QVERIFY(restored.preferences().value(QStringLiteral("desktopCalendarVisible")).toBool());QCOMPARE(restored.preferences().value(QStringLiteral("desktopCalendarOpacity")).toInt(),70);
    }
    void navigationSeparatesOwnershipHierarchyFromTags() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("navigation.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("area")},{"title",QStringLiteral("work")}}));QTRY_VERIFY(!w.busy());
        const auto area=w.catalogOf(QStringLiteral("area")).first().toMap().value("id").toString();
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("release")},{"parentId",area}}));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("independent")}}));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("tag")},{"title",QStringLiteral("phone")}}));QTRY_VERIFY(!w.busy());
        const auto entries=w.navigationCatalog();QCOMPARE(entries.size(),3);
        QCOMPARE(entries[0].toMap().value("id").toString(),area);QCOMPARE(entries[0].toMap().value("depth").toInt(),0);
        QCOMPARE(entries[1].toMap().value("title").toString(),QStringLiteral("release"));QCOMPARE(entries[1].toMap().value("depth").toInt(),1);
        QCOMPARE(entries[2].toMap().value("title").toString(),QStringLiteral("independent"));QCOMPARE(entries[2].toMap().value("depth").toInt(),0);
        QCOMPARE(w.catalogOf(QStringLiteral("tag")).size(),1);
        auto project=w.catalogOf(QStringLiteral("project")).first().toMap();project.insert(QStringLiteral("status"),QStringLiteral("COMPLETED"));QVERIFY(w.saveCatalog(project));QTRY_VERIFY(!w.busy());
        QCOMPARE(w.navigationCatalog().size(),2);QCOMPARE(w.catalogOf(QStringLiteral("project")).size(),2);
        QVERIFY(w.setPreference(QStringLiteral("sidebarTagsExpanded"),false));
        TaskWorkspace restored;restored.start(dir.filePath(QStringLiteral("navigation.sqlite")));QTRY_VERIFY(!restored.busy());QVERIFY(!restored.preferences().value("sidebarTagsExpanded").toBool());
    }
    void backgroundMaintenanceWaitsForPointerInteraction() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("interaction.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        w.setView(QStringLiteral("today"));QVERIFY(w.createTask(QStringLiteral("drag remains enabled")));QTRY_VERIFY(!w.busy());
        const auto before=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();
        QSignalSpy operations(&w,&TaskWorkspace::operationFinished);
        QSignalSpy uiState(&w,&TaskWorkspace::stateChanged);
        QSignalSpy gestures(&w,&TaskWorkspace::interactionActiveChanged);
        w.setInteractionActive(true);QVERIFY(w.interactionActive());
        QCOMPARE(gestures.count(),1);QVERIFY(uiState.isEmpty()); // A press must not re-enter delegate enabled bindings.
        for(int i=0;i<10;++i)w.refreshClock();
        QTest::qWait(30);QVERIFY(!w.busy());QVERIFY(w.taskReorderingAllowed());QVERIFY(operations.isEmpty());
        const auto clockUpdates=uiState.count();
        w.setInteractionActive(false);QCOMPARE(gestures.count(),2);QCOMPARE(uiState.count(),clockUpdates);QTRY_VERIFY(!operations.isEmpty());QTRY_VERIFY(!w.busy());
        QCOMPARE(operations.count(),1);QCOMPARE(operations.first()[0].toString(),QStringLiteral("maintenance"));QVERIFY(operations.first()[1].toBool());
        QCOMPARE(w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString(),before);
    }
    void refreshDuringWriteIsCoalescedAndPreservesValidationError() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("recovery.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QSignalSpy operations(&w,&TaskWorkspace::operationFinished);
        bool sawBackground=false;
        connect(&w,&TaskWorkspace::stateChanged,&w,[&]{if(w.backgroundBusy())sawBackground=true;});
        QVERIFY(w.createTask(QStringLiteral("accepted before wake")));QVERIFY(w.busy());
        QVERIFY(!w.backgroundBusy());
        for(int i=0;i<100;i++)w.refreshClock();
        QTRY_VERIFY(!w.busy());QTRY_VERIFY(operations.count()>=2);
        int maintained=0;for(const auto& args:operations)if(args[0].toString()==QStringLiteral("maintenance")){QVERIFY(args[1].toBool());++maintained;}
        QCOMPARE(maintained,1);QVERIFY(sawBackground);QVERIFY(!w.backgroundBusy());QCOMPARE(w.tasks()->rowCount(),1);
        const auto id=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();const auto task=w.task(id);
        QVERIFY(w.saveTask(id,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral(""),QString(),QStringLiteral("INBOX"),QString(),QString(),false));QTRY_VERIFY(!w.busy());
        const auto error=w.errorText();QVERIFY(!error.isEmpty());operations.clear();w.refreshClock();
        QTRY_VERIFY(!operations.isEmpty());QTRY_VERIFY(!w.busy());QCOMPARE(w.errorText(),error);QCOMPARE(w.task(id),task);
    }
    void unifiedEditorCommitsOneVersionOrNothing() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("unified.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QVERIFY(w.createTask(QStringLiteral("original")));QTRY_VERIFY(!w.busy());
        const auto id=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();const auto initial=w.task(id);
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("project")},{"title",QStringLiteral("project")}}));QTRY_VERIFY(!w.busy());
        const auto project=w.catalogOf(QStringLiteral("project")).first().toMap().value(QStringLiteral("id")).toString();
        QVERIFY(w.saveCatalog({{"kind",QStringLiteral("tag")},{"title",QStringLiteral("tag")}}));QTRY_VERIFY(!w.busy());
        const auto tag=w.catalogOf(QStringLiteral("tag")).first().toMap().value(QStringLiteral("id")).toString();
        QVariantMap details{{"projectId",project},{"areaId",QString()},{"headingId",QString()},{"tagIds",QStringList{tag}},
            {"important",true},{"remindAtLocal",QString()}};
        const auto version=initial.value(QStringLiteral("rowVersion")).toLongLong();
        QVERIFY(w.saveTaskWithDetails(id,version,QStringLiteral("together"),QStringLiteral("notes"),QStringLiteral("ACTIVE"),w.dateOffset(0),w.dateOffset(2),false,details));
        QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());const auto saved=w.task(id);
        QCOMPARE(saved.value(QStringLiteral("rowVersion")).toLongLong(),version+1);QCOMPARE(saved.value(QStringLiteral("title")).toString(),QStringLiteral("together"));
        QCOMPARE(saved.value(QStringLiteral("projectId")).toString(),project);QCOMPARE(saved.value(QStringLiteral("tagIds")).toStringList(),QStringList{tag});
        QVERIFY(saved.value(QStringLiteral("important")).toBool());
        const auto currentVersion=saved.value(QStringLiteral("rowVersion")).toLongLong();
        auto invalid=details;invalid.insert(QStringLiteral("remindAtLocal"),QStringLiteral("invalid"));
        QVERIFY(!w.saveTaskWithDetails(id,currentVersion,QStringLiteral("must not change"),QString(),QStringLiteral("ACTIVE"),w.dateOffset(0),w.dateOffset(2),false,invalid));QCOMPARE(w.task(id),saved);
        QVERIFY(w.saveTaskWithDetails(id,currentVersion,QStringLiteral("must not change"),QString(),QStringLiteral("ACTIVE"),QStringLiteral("2026-02-31"),w.dateOffset(2),false,details));QTRY_VERIFY(!w.busy());QCOMPARE(w.task(id),saved);
        invalid=details;invalid.insert(QStringLiteral("tagIds"),QStringList{QStringLiteral("missing")});
        QVERIFY(w.saveTaskWithDetails(id,currentVersion,QStringLiteral("must not change"),QString(),QStringLiteral("ACTIVE"),w.dateOffset(0),w.dateOffset(2),false,invalid));QTRY_VERIFY(!w.busy());QCOMPARE(w.task(id),saved);
        QVERIFY(w.saveTaskWithDetails(id,version,QStringLiteral("stale"),QString(),QStringLiteral("ACTIVE"),w.dateOffset(0),w.dateOffset(2),false,details));QTRY_VERIFY(!w.busy());QCOMPARE(w.task(id),saved);
        w.reload();QTRY_VERIFY(!w.busy());QCOMPARE(w.task(id),saved);
        for(const auto& view:{QStringLiteral("today"),QStringLiteral("anytime"),QStringLiteral("inbox"),QStringLiteral("upcoming")}) {
            w.setView(view);
            for(int row=0;row<w.tasks()->rowCount();++row) {
                const auto index=w.tasks()->index(row);
                const int role=view==QStringLiteral("today") ? TaskListModel::Section : view==QStringLiteral("anytime") ? TaskListModel::ContextGroup : view==QStringLiteral("upcoming") ? TaskListModel::StartOn : 0;
                QCOMPARE(w.tasks()->data(index,TaskListModel::ViewSection).toString(),role ? w.tasks()->data(index,role).toString() : QString());
            }
        }
    }
    void localEventsCalendarAndRelationsPersist() {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("calendar.sqlite"));QString taskId,eventId;
        {
            TaskWorkspace w;w.start(path);QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
            w.setView(QStringLiteral("today"));QVERIFY(w.createForDate(QStringLiteral("prepare"),w.dateOffset(0)));QTRY_VERIFY(!w.busy());
            taskId=w.tasks()->data(w.tasks()->index(0),TaskListModel::Id).toString();QVERIFY(!taskId.isEmpty());
            QVariantMap e{{"title",QStringLiteral("review")},{"allDay",true},{"startDate",w.dateOffset(0)},{"endDate",w.dateOffset(0)},{"links",QVariantList{QVariantMap{{"taskId",taskId},{"role",QStringLiteral("followup")}}}}};
            QVERIFY(w.saveEvent(e));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());QCOMPARE(w.calendarEvents().size(),1);
            auto saved=w.calendarEvents().first().toMap();eventId=saved.value(QStringLiteral("eventId")).toString();QCOMPARE(w.eventsForTask(taskId).size(),1);
            w.setView(QStringLiteral("month"));bool found=false;for(const auto& value:w.calendarDays()){const auto day=value.toMap();if(day.value(QStringLiteral("date"))==w.dateOffset(0)){QCOMPARE(day.value(QStringLiteral("entries")).toList().size(),1);QCOMPARE(day.value(QStringLiteral("events")).toList().size(),1);found=true;}}QVERIFY(found);
            saved.insert(QStringLiteral("title"),QStringLiteral("changed"));QVERIFY(w.saveEvent(saved));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());
            QVERIFY(w.saveEvent(saved));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QCOMPARE(w.calendarEvent(eventId).value(QStringLiteral("title")).toString(),QStringLiteral("changed"));
            auto task=w.task(taskId);QVERIFY(w.changeState(taskId,task.value(QStringLiteral("rowVersion")).toLongLong(),QStringLiteral("complete")));QTRY_VERIFY(!w.busy());QCOMPARE(w.calendarEvents().size(),1);
            auto current=w.calendarEvent(eventId);QVERIFY(w.deleteEvent(eventId,current.value(QStringLiteral("rowVersion")).toLongLong()));QTRY_VERIFY(!w.busy());QVERIFY(w.calendarEvents().isEmpty());QVERIFY(w.canUndoEvent());QVERIFY(!w.task(taskId).isEmpty());
            QVERIFY(w.undoEventDelete());QTRY_VERIFY(!w.busy());QCOMPARE(w.eventsForTask(taskId).size(),1);
            current=w.calendarEvent(eventId);current.insert(QStringLiteral("allDay"),false);current.insert(QStringLiteral("zone"),QStringLiteral("invalid"));QVERIFY(w.saveEvent(current));QTRY_VERIFY(!w.busy());QVERIFY(!w.errorText().isEmpty());QVERIFY(w.calendarEvent(eventId).value(QStringLiteral("allDay")).toBool());
            current.insert(QStringLiteral("zone"),QStringLiteral("UTC"));current.insert(QStringLiteral("startTime"),QStringLiteral("09:00:12.123"));current.insert(QStringLiteral("endTime"),QStringLiteral("10:00:20.456"));QVERIFY(w.saveEvent(current));QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());current=w.calendarEvent(eventId);QCOMPARE(current.value(QStringLiteral("startTime")).toString(),QStringLiteral("09:00:12.123"));current.insert(QStringLiteral("notes"),QStringLiteral("precision preserved"));QVERIFY(w.saveEvent(current));QTRY_VERIFY(!w.busy());QCOMPARE(w.calendarEvent(eventId).value(QStringLiteral("startTime")).toString(),QStringLiteral("09:00:12.123"));
        }
        TaskWorkspace reopened;reopened.start(path);QTRY_VERIFY(!reopened.busy());QVERIFY(reopened.ready());QCOMPARE(reopened.calendarEvents().size(),1);QCOMPARE(reopened.eventsForTask(taskId).size(),1);
    }
    void candidatePreferencesAndBatchCommandsPersist()
    {
        QTemporaryDir dir;const auto path=dir.filePath(QStringLiteral("batch.sqlite"));QString id;
        {
            TaskWorkspace workspace;workspace.start(path);QTRY_VERIFY(!workspace.busy());QVERIFY(workspace.ready());
            QVERIFY(workspace.setPreference(QStringLiteral("compact"),true));
            QVERIFY(workspace.setPreference(QStringLiteral("backupDays"),7));
            QVERIFY(!workspace.setPreference(QStringLiteral("backupDays"),2));
            workspace.setView(QStringLiteral("today"));QVERIFY(workspace.createTask(QStringLiteral("reschedule")));QTRY_VERIFY(!workspace.busy());
            const auto task=workspace.tasks()->data(workspace.tasks()->index(0),TaskListModel::Id).toString();id=task;
            QVariantList selected{workspace.task(id)};
            QVERIFY(workspace.batchTasks(selected,QStringLiteral("start"),workspace.dateOffset(2)));QTRY_VERIFY(!workspace.busy());
            QVERIFY(workspace.errorText().isEmpty());QCOMPARE(workspace.task(id).value(QStringLiteral("startOn")).toString(),workspace.dateOffset(2));
            QCOMPARE(workspace.todayTasks()->rowCount(),0);
            QVERIFY(workspace.batchTasks(selected,QStringLiteral("complete"),{}));QTRY_VERIFY(!workspace.busy());QVERIFY(!workspace.errorText().isEmpty());
            QCOMPARE(workspace.task(id).value(QStringLiteral("taskStatus")).toString(),QStringLiteral("OPEN"));
            selected={workspace.task(id)};QVERIFY(workspace.batchTasks(selected,QStringLiteral("complete"),{}));QTRY_VERIFY(!workspace.busy());
            QCOMPARE(workspace.task(id).value(QStringLiteral("taskStatus")).toString(),QStringLiteral("COMPLETED"));
        }
        TaskWorkspace reopened;reopened.start(path);QTRY_VERIFY(!reopened.busy());
        QVERIFY(reopened.preferences().value(QStringLiteral("compact")).toBool());QCOMPARE(reopened.preferences().value(QStringLiteral("backupDays")).toInt(),7);
        QCOMPARE(reopened.task(id).value(QStringLiteral("taskStatus")).toString(),QStringLiteral("COMPLETED"));
    }
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
        const auto first=id(0),last=id(2);
        QVERIFY(!w.canMoveTask(first,-1));QVERIFY(!w.canMoveTask(last,1));QVERIFY(w.canMoveTask(first,1));QCOMPARE(w.taskIndex(first),0);QCOMPARE(w.taskIndex(QStringLiteral("missing")),-1);
        w.setIncludeCompleted(true);QVERIFY(!w.canMoveTask(first,1));w.setIncludeCompleted(false);
        w.setSearch(QStringLiteral("a"));QVERIFY(!w.canMoveTask(first,1));QVERIFY(!w.moveTask(first,1));w.setSearch(QString());
        w.setTagFilter(QStringLiteral("missing"));QVERIFY(!w.canMoveTask(first,1));w.setTagFilter(QString());
        w.setProjectFilter(QStringLiteral("missing"));QVERIFY(!w.canMoveTask(first,1));w.setProjectFilter(QString());
        w.setView(QStringLiteral("upcoming"));QVERIFY(!w.canMoveTask(first,1));w.setView(QStringLiteral("today"));
        QVERIFY(w.moveTask(first,1));QTRY_VERIFY(!w.busy());QCOMPARE(id(1),first);
        auto map=w.task(last);QVERIFY(w.saveDetails(last,map.value(QStringLiteral("rowVersion")).toLongLong(),{{QStringLiteral("evening"),true}}));QTRY_VERIFY(!w.busy());
        QVERIFY(!w.canMoveTask(first,1)); // Different Today sections cannot be swapped by an arrow action.
        QVERIFY(w.dropTask(first,last));QTRY_VERIFY(!w.busy());QVERIFY(w.task(first).value(QStringLiteral("evening")).toBool());
        w.reload();QTRY_VERIFY(!w.busy());QVERIFY(w.task(first).value(QStringLiteral("evening")).toBool());
        w.setView(QStringLiteral("anytime"));QCOMPARE(id(0),first); // Today order is independent of base order.
        QVERIFY(!w.taskReorderingAllowed());QVERIFY(!w.dropTask(last,first));
        w.setView(QStringLiteral("today"));QVERIFY(w.taskReorderingAllowed());w.setIncludeCompleted(true);QVERIFY(!w.taskReorderingAllowed());QVERIFY(!w.dropTask(last,first));w.setIncludeCompleted(false);
        w.setTagFilter(QStringLiteral("missing"));QVERIFY(!w.dropTask(last,first));w.setTagFilter(QString());
        QVERIFY(w.createTask("timed afternoon"));QTRY_VERIFY(!w.busy());QString timed;for(const auto& task:w.tasks()->items())if(task.title=="timed afternoon")timed=task.id;QVERIFY(!timed.isEmpty());const QVariantMap schedule{{"allDay",false},{"startDate",w.dateOffset(0)},{"endDate",w.dateOffset(0)},{"startTime","14:00"},{"endTime","15:00"},{"zone",w.systemTimeZone()}};QVERIFY(w.saveDetails(timed,w.task(timed).value("rowVersion").toLongLong(),{{"schedule",schedule}}));QTRY_VERIFY(!w.busy());const auto before=w.task(timed);QVERIFY(!w.dropTask(timed,first));QCOMPARE(w.task(timed),before);

    }
};
QTEST_GUILESS_MAIN(TaskWorkspaceTest)
#include "TaskWorkspaceTest.moc"
