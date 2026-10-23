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
    void calendarOccurrenceOrderAndReadOnlyPreview() {
        QTemporaryDir dir;TaskWorkspace w;w.start(dir.filePath(QStringLiteral("browse.sqlite")));QTRY_VERIFY(!w.busy());QVERIFY(w.ready());
        QVariantMap event{{"title",QStringLiteral("later weekly")},{"allDay",false},{"startDate",QStringLiteral("2026-10-16")},{"endDate",QStringLiteral("2026-10-16")},{"startTime",QStringLiteral("16:00")},{"endTime",QStringLiteral("17:00")},{"zone",w.systemTimeZone()},{"repeatFrequency",QStringLiteral("WEEKLY")},{"alertMinutes",15}};
        QVERIFY(w.saveEvent(event));QTRY_VERIFY(!w.busy());const auto series=w.calendarEvents().first().toMap();
        event.insert("title",QStringLiteral("earlier once"));event.insert("startDate",QStringLiteral("2026-10-23"));event.insert("endDate",QStringLiteral("2026-10-23"));event.insert("startTime",QStringLiteral("09:00"));event.insert("endTime",QStringLiteral("10:00"));event.insert("repeatFrequency",QStringLiteral("NONE"));
        QVERIFY(w.saveEvent(event));QTRY_VERIFY(!w.busy());
        QVERIFY(w.saveEvent({{"title",QStringLiteral("all day")},{"startDate",QStringLiteral("2026-10-23")},{"endDate",QStringLiteral("2026-10-23")}}));QTRY_VERIFY(!w.busy());
        QVariantList events;for(const auto& day:w.calendarDaysFor(QStringLiteral("2026-10-23"),false))if(day.toMap().value("date")==QStringLiteral("2026-10-23"))events=day.toMap().value("events").toList();
        QCOMPARE(events.size(),3);QCOMPARE(events[0].toMap().value("title").toString(),QStringLiteral("all day"));QCOMPARE(events[1].toMap().value("title").toString(),QStringLiteral("earlier once"));QCOMPARE(events[2].toMap().value("title").toString(),QStringLiteral("later weekly"));
        const auto id=series.value("eventId").toString(),key=events[2].toMap().value("occurrenceKey").toString();
        const auto preview=w.calendarEventPreview(id,key);QCOMPARE(preview.value("startDate").toString(),QStringLiteral("2026-10-23"));QVERIFY(preview.value("scopeLabel").toString().contains(QStringLiteral("2026-10-16")));QVERIFY(preview.value("alertLabel").toString().contains(QStringLiteral("15")));
        QCOMPARE(w.calendarEvent(id),series);QVERIFY(w.calendarEventPreview(id,QStringLiteral("invalid")).isEmpty());QVERIFY(w.calendarEventPreview(QStringLiteral("missing"),key).isEmpty());
        QCOMPARE(w.calendarEventPreview(id).value("startDate"),series.value("startDate"));
        auto changed=series;changed.insert("startTime",QStringLiteral("18:00"));changed.insert("endTime",QStringLiteral("19:00"));QVERIFY(w.saveEvent(changed));QTRY_VERIFY(!w.busy());QVERIFY(w.calendarEventPreview(id,key).isEmpty());
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
            {"checklist",QVariantList{QVariantMap{{"id",QString()},{"title",QStringLiteral("step")},{"done",true}}}},{"remindAtLocal",QString()}};
        const auto version=initial.value(QStringLiteral("rowVersion")).toLongLong();
        QVERIFY(w.saveTaskWithDetails(id,version,QStringLiteral("together"),QStringLiteral("notes"),QStringLiteral("ACTIVE"),w.dateOffset(0),w.dateOffset(2),false,details));
        QTRY_VERIFY(!w.busy());QVERIFY(w.errorText().isEmpty());const auto saved=w.task(id);
        QCOMPARE(saved.value(QStringLiteral("rowVersion")).toLongLong(),version+1);QCOMPARE(saved.value(QStringLiteral("title")).toString(),QStringLiteral("together"));
        QCOMPARE(saved.value(QStringLiteral("projectId")).toString(),project);QCOMPARE(saved.value(QStringLiteral("tagIds")).toStringList(),QStringList{tag});
        QCOMPARE(saved.value(QStringLiteral("checklist")).toList().size(),1);QVERIFY(saved.value(QStringLiteral("checklist")).toList().first().toMap().value(QStringLiteral("done")).toBool());
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
        QVERIFY(!w.canMoveTask(first,-1));QVERIFY(!w.canMoveTask(last,1));QVERIFY(w.canMoveTask(first,1));
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
    }
};
QTEST_GUILESS_MAIN(TaskWorkspaceTest)
#include "TaskWorkspaceTest.moc"
