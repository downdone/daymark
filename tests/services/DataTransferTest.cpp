#include "persistence/DataTransfer.h"
#include "services/TaskService.h"
#include "services/RecurrenceService.h"
#ifndef Q_MOC_RUN
#include "sync/Journal.h"
#endif
#include "persistence/ScopedTransaction.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QSettings>
#include <QSqlQuery>
#include <QtTest>
#include "../support/LegacyDatabase.h"
using namespace daymark;
class DataTransferTest:public QObject {
    Q_OBJECT
private slots:
    void syncRapidAfterCompletionKeepsDistinctIdentityAndSameActualDay() {
        QTemporaryDir dir;
        SqliteDatabase db, remote;
        QVERIFY(db.open(dir.filePath("cycles.sqlite")).ok());
        QVERIFY(remote.open(dir.filePath("remote.sqlite")).ok());
        SystemClock clock;
        TaskRepository tasks(db.connection());
        TaskService service(tasks, clock);
        RecurrenceRepository rules(db.connection());
        RecurrenceService recurrence(rules, tasks, clock);
        auto source = service.create("rapid recurrence", "anytime");
        QVERIFY(source.ok());
        const auto ordinary = sync::Mapping(db.connection()).capture();
        QVERIFY(ordinary.ok());
        RecurrenceRule spec;
        spec.frequency = "AFTER_DAYS";
        spec.weekdays = 8; // UI's prior weekday choice is irrelevant in this mode.
        spec.interval = 1;
        TaskPatch patch;
        patch.repeat = FieldPatch<RecurrenceRule>::set(spec);
        source = service.edit(source.value.id, source.value.rowVersion, patch);
        QVERIFY(source.ok());
        const auto attached = sync::Mapping(db.connection()).capture();
        QVERIFY(attached.ok());
        const auto delta = sync::commandsBetween(ordinary.value, attached.value);
        QVERIFY2(!delta.unsupported, qPrintable(QString::fromUtf8(sync::json({{"commands",delta.commands}}))));
        const auto rule = source.value.recurrenceId;
        QString previous = source.value.id;
        QStringList successors;
        for (int i = 0; i < 2; ++i) {
            auto task = tasks.find(previous);
            QVERIFY(task.ok());
            QVERIFY(service.changeState(previous, task.value.rowVersion, "complete").ok());
            const auto generated = recurrence.generate({}, {}, {previous});
            QVERIFY2(generated.ok(), qPrintable(generated.error.key));
            QCOMPARE(generated.value, 1);
            Task next;
            for (const auto &candidate : tasks.all().value)
                if (candidate.recurrenceId == rule && candidate.status == "OPEN") next = candidate;
            QCOMPARE(next.syncOccurrenceKey, QStringLiteral("A:") + previous);
            QCOMPARE(next.syncOccurrenceOn, clock.localToday().addDays(1));
            successors.append(next.id);
            previous = next.id;
        }
        QVERIFY(successors[0] != successors[1]);
        QVERIFY(tasks.find(successors[0]).value.occurrenceOn != tasks.find(successors[1]).value.occurrenceOn);
        const auto captured = sync::Mapping(db.connection()).capture();
        QVERIFY2(captured.ok(), qPrintable(captured.error.key));
        QCOMPARE(captured.value.edges.size(), 2);
        { ScopedTransaction tx(remote.connection());
          const auto applied = sync::Mapping(remote.connection()).apply(captured.value.array());
          QVERIFY2(applied.ok(), qPrintable(applied.error.key)); QVERIFY(tx.commit()); }
        const auto roundTrip = sync::Mapping(remote.connection()).capture();
        QVERIFY2(roundTrip.ok(), qPrintable(roundTrip.error.key));
        QCOMPARE(roundTrip.value.entities, captured.value.entities);
        QCOMPARE(roundTrip.value.edges, captured.value.edges);
        auto confirmed = captured.value.entities.value("task:" + successors[0]);
        auto value = confirmed.value("value").toObject();
        value.insert("created_at_utc", "2000-01-01T00:00:00.000Z");
        confirmed.insert("value", value);
        { ScopedTransaction tx(remote.connection());
          QVERIFY(sync::Mapping(remote.connection()).apply({confirmed}).ok()); QVERIFY(tx.commit()); }
        QCOMPARE(TaskRepository(remote.connection()).find(successors[0]).value.createdAtUtc,
                 QStringLiteral("2000-01-01T00:00:00.000Z"));
    }
    void syncLegacyRecurrenceAliasesPreservePhysicalIdentityAndTypedReferences() {
        const auto rule = sync::uuid(), source = sync::uuid(), native = sync::uuid(), canonical = sync::uuid();
        const QString occurrence = "D:2030-01-02";
        auto task = [](const QString &id, const QString &rule, const QString &occurrence) {
            return QJsonObject{{"type","task"},{"id",id},{"revision","1"},{"value",QJsonObject{{"id",id},{"recurrence_id",rule},{"occurrence_key",occurrence},{"parent_task_id",QJsonValue::Null},{"links",QJsonArray{}}}}};
        };
        sync::Graph graph;
        auto sourceEntity = task(source, rule, "D:2030-01-01"), legacy = task(native, rule, occurrence);
        auto generation = QJsonObject{{"type","recurrence_rule"},{"id",rule},{"value",QJsonObject{{"id",rule},{"source_task_id",source}}}};
        for (const auto &e : {sourceEntity, legacy, generation}) graph.entities.insert(sync::key(e), e);
        sync::Identity index;
        index.known.insert(canonical);
        index.occurrences.insert(rule + "/" + occurrence, canonical);
        index.knownKeys.insert(canonical, rule + "/" + occurrence);
        index.sources.insert(rule, source);
        const auto mapped = index.canonicalize(graph);
        QVERIFY(mapped.ok());
        QCOMPARE(mapped.value.taskWireIds.value(native), canonical);
        QCOMPARE(mapped.value.taskLocalIds.value(canonical), native);
        QVERIFY(graph.entities.contains("task:" + native));
        const auto intent = QJsonObject{{"commands",QJsonArray{QJsonObject{{"type","task.patch"},{"task_id",native},{"patch",QJsonObject{{"parent_task_id",source},{"category_id",native}}}}}}};
        const auto converted = sync::Identity::documentTo(intent, mapped.value.taskWireIds, {});
        const auto command = converted.value("commands").toArray().first().toObject();
        QCOMPARE(command.value("task_id").toString(), canonical);
        QCOMPARE(command.value("patch").toObject().value("category_id").toString(), native);
        QCOMPARE(sync::Identity::entityTo(mapped.value.entities.value("task:" + canonical), mapped.value.taskLocalIds), legacy);
    }
    void registrationRequestUsesStrictSharedSchema() {
        QJsonObject input{{"protocol_version",1},{"login",QStringLiteral("new-user")},{"password",QStringLiteral("long-valid-password")}};
        QVERIFY(sync::Schema::accepts("RegistrationRequest",input));
        input.insert("extra",true);QVERIFY(!sync::Schema::accepts("RegistrationRequest",input));input.remove("extra");
        input.insert("password",QStringLiteral("short"));QVERIFY(!sync::Schema::accepts("RegistrationRequest",input));
        input.insert("password",QStringLiteral("long-valid-password"));input.remove("login");QVERIFY(!sync::Schema::accepts("RegistrationRequest",input));
    }
    void syncSharedContractFixturesAndGoldenCanonicalBytes() {
        QFile fixture(QFINDTESTDATA("../../docs/api/fixtures/contract-v1.json"));QVERIFY(fixture.open(QIODevice::ReadOnly));const auto root=QJsonDocument::fromJson(fixture.readAll()).object();
        const auto cases=root.value("schema_cases").toArray();QCOMPARE(cases.size(),77);for(const auto& x:cases){const auto c=x.toObject();const auto schema=c.value("schema").toString();QVERIFY2(sync::Schema::definitions().contains(schema),qPrintable(schema));QVERIFY2(sync::Schema::accepts(schema,c.value("value"))==c.value("valid").toBool(),qPrintable(c.value("id").toString()));}
        QFile golden(QFINDTESTDATA("../../docs/api/fixtures/canonical-v1.json"));QVERIFY(golden.open(QIODevice::ReadOnly));const auto vectors=QJsonDocument::fromJson(golden.readAll()).object().value("cases").toArray();QCOMPARE(vectors.size(),16);
        for(const auto& x:vectors){const auto c=x.toObject();QCOMPARE(sync::canonical(c.value("semantic")).toHex(),c.value("expected_hex").toString().toLatin1());QCOMPARE(sync::digest(c.value("semantic")).toHex(),c.value("expected_sha256").toString().toLatin1());}
        for(const auto& bad:{QByteArray("{\"a\":1,\"\\u0061\":2}"),QByteArray("{\"a\":1.0}"),QByteArray("\"\\ud800\""),QByteArray("[] []")})QVERIFY(!sync::StrictJson::parse(bad).ok());
    }
    void syncMappingAndOuterTransactionAreAtomic() {
        QTemporaryDir dir;SqliteDatabase db,other;QVERIFY(db.open(dir.filePath("source.sqlite")).ok());QVERIFY(other.open(dir.filePath("remote.sqlite")).ok());TaskRepository repo(db.connection());SystemClock clock;TaskService service(repo,clock);const auto created=service.create("同步任务","anytime");QVERIFY(created.ok());const auto before=sync::Mapping(db.connection()).capture();QVERIFY(before.ok());
        const auto payload=QJsonObject{{"dto_version",1},{"base_head_seq",QStringLiteral("0")},{"entities",before.value.array()},{"aliases",before.value.aliases},{"successor_edges",before.value.edges}};QVERIFY(sync::Schema::accepts("ImportPayload",payload));
        {ScopedTransaction tx(other.connection());QVERIFY(tx.active());const auto applied=sync::Mapping(other.connection()).apply(before.value.array());QVERIFY2(applied.ok(),qPrintable(applied.error.key));QVERIFY(tx.commit());}QCOMPARE(TaskRepository(other.connection()).find(created.value.id).value.title,created.value.title);
        {ScopedTransaction tx(db.connection());QVERIFY(tx.active());QVERIFY(service.changeState(created.value.id,created.value.rowVersion,"complete").ok());tx.rollback();}QCOMPARE(repo.find(created.value.id).value.status,QStringLiteral("OPEN"));
    }
    void syncJournalRollbackFrozenRetryAndOwnEchoPreserveLaterEdit() {
        QTemporaryDir dir;SqliteDatabase db;QVERIFY(db.open(dir.filePath("journal.sqlite")).ok());TaskRepository repo(db.connection());SystemClock clock;TaskService service(repo,clock);const auto created=service.create("base","anytime");QVERIFY(created.ok());auto sql=[&](const QString& text,const QList<QVariant>& bindings=QList<QVariant>{}){QSqlQuery q(db.connection());q.prepare(text);for(const auto& b:bindings)q.addBindValue(b);return q.exec();};
        const auto pid=sync::uuid(),epoch=sync::uuid(),dataset=sync::uuid(),installation=sync::uuid();QVERIFY(sql("INSERT INTO sync_profiles(profile_id,mode,server_origin,server_instance_id,user_id,dataset_id,data_epoch,generation,auth_state,installation_id,phase) VALUES(?,'ACCOUNT','https://example.invalid',?,?,?,?,1,'AUTHENTICATED',?,'ACTIVE')",{pid,sync::uuid(),sync::uuid(),dataset,epoch,installation}));QVERIFY(sql("INSERT INTO sync_cursor VALUES(?,?,?,0,0)",{pid,epoch,QStringLiteral("cursor0")}));
        const auto base=sync::Mapping(db.connection()).capture();QVERIFY(base.ok());for(const auto& e:base.value.entities)QVERIFY(sql("INSERT INTO sync_base(profile_id,data_epoch,entity_type,entity_id,server_revision,canonical_snapshot) VALUES(?,?,?,?,3,?)",{pid,epoch,e.value("type").toVariant(),e.value("id").toVariant(),QString::fromUtf8(sync::json(e))}));
        QVERIFY(sql("CREATE TRIGGER fail_journal BEFORE INSERT ON sync_dirty_entities BEGIN SELECT RAISE(ABORT,'injected last journal write');END"));
        {ScopedTransaction tx(db.connection());TaskPatch patch;patch.title=FieldPatch<QString>::set("must roll back");QVERIFY(service.edit(created.value.id,created.value.rowVersion,patch).ok());const auto after=sync::Mapping(db.connection()).capture();QVERIFY(after.ok());QVERIFY(!sync::Journal(db.connection()).record(base.value,after.value).ok());}QCOMPARE(repo.find(created.value.id).value.title,QStringLiteral("base"));QVERIFY(sql("DROP TRIGGER fail_journal"));
        auto edit=[&](const QString& title){ScopedTransaction tx(db.connection());const auto before=sync::Mapping(db.connection()).capture();auto t=repo.find(created.value.id);TaskPatch patch;patch.title=FieldPatch<QString>::set(title);auto changed=service.edit(created.value.id,t.value.rowVersion,patch);if(!changed.ok())return false;const auto after=sync::Mapping(db.connection()).capture();return after.ok() && sync::Journal(db.connection()).record(before.value,after.value).ok() && tx.commit();};
        QVERIFY(edit("first"));const auto firstGraph=sync::Mapping(db.connection()).capture();QVERIFY(edit("second"));sync::Journal journal(db.connection());auto prepared=journal.execute("prepare",{});QVERIFY(prepared.ok());QVERIFY(!prepared.value.isEmpty());const auto frozen=prepared.value.value("payload").toString();const auto payload=QJsonDocument::fromJson(frozen.toUtf8()).object();const auto mid=payload.value("mutation_id");QVERIFY(!sql("UPDATE sync_outbox SET intent_json='{}' WHERE mutation_id=?",{mid.toVariant()}));
        QVERIFY(journal.execute("retry",{{"mutation_id",mid},{"delay_ms",-1000},{"code",QStringLiteral("network")}}).ok());QCOMPARE(journal.execute("prepare",{}).value.value("payload").toString(),frozen);
        QJsonObject receipt{{"protocol_version",1},{"mutation_id",mid},{"data_epoch",epoch},{"result",QStringLiteral("committed")},{"batch_seq",QStringLiteral("1")},{"observed_head_seq",QStringLiteral("1")},{"entity_revisions",QJsonArray{QJsonObject{{"type",QStringLiteral("task")},{"id",created.value.id},{"revision",QStringLiteral("4")}}}},{"structure_revisions",QJsonArray{}},{"committed_at_utc",sync::now()}};QVERIFY(sync::Schema::accepts("Receipt",receipt));QVERIFY(journal.execute("receipt",{{"receipt",receipt}}).ok());QVERIFY(journal.execute("prepare",{}).value.isEmpty());
        auto echo=firstGraph.value.entities.value("task:"+created.value.id);echo.insert("revision",QStringLiteral("4"));QJsonObject batch{{"seq",QStringLiteral("1")},{"origin_mutation_id",mid},{"origin_installation_id",installation},{"committed_at_utc",sync::now()},{"changes",QJsonArray{echo}},{"structure_revisions",QJsonArray{}},{"aliases",QJsonArray{}},{"successor_edges",QJsonArray{}}};QVERIFY(journal.execute("batch",{{"batch",batch},{"expected_cursor",QStringLiteral("cursor0")},{"after_cursor",QStringLiteral("cursor1")}}).ok());QCOMPARE(repo.find(created.value.id).value.title,QStringLiteral("second"));const auto next=journal.execute("prepare",{});QVERIFY(next.ok());QVERIFY(!next.value.isEmpty());const auto nextPayload=QJsonDocument::fromJson(next.value.value("payload").toString().toUtf8()).object();bool correct=false;for(const auto& v:nextPayload.value("preconditions").toObject().value("entities").toArray())if(v.toObject().value("id")==created.value.id)correct=v.toObject().value("revision")=="4";QVERIFY(correct);
        auto foreign=echo;foreign.insert("revision",QStringLiteral("5"));auto remote=foreign.value("value").toObject();remote.insert("title",QStringLiteral("other device"));foreign.insert("value",remote);batch.insert("seq",QStringLiteral("2"));batch.insert("origin_mutation_id",sync::uuid());batch.insert("origin_installation_id",sync::uuid());batch.insert("changes",QJsonArray{foreign});QVERIFY(journal.execute("batch",{{"batch",batch},{"expected_cursor",QStringLiteral("cursor1")},{"after_cursor",QStringLiteral("cursor2")}}).ok());QCOMPARE(repo.find(created.value.id).value.title,QStringLiteral("second"));foreign.insert("revision",QStringLiteral("6"));remote.insert("title",QStringLiteral("newer other device"));foreign.insert("value",remote);batch.insert("seq",QStringLiteral("3"));batch.insert("origin_mutation_id",sync::uuid());batch.insert("changes",QJsonArray{foreign});QVERIFY(journal.execute("batch",{{"batch",batch},{"expected_cursor",QStringLiteral("cursor2")},{"after_cursor",QStringLiteral("cursor3")}}).ok());foreign.insert("revision",QStringLiteral("7"));remote.insert("deleted_at_utc",sync::now());remote.insert("deletion_group_id",sync::uuid());foreign.insert("value",remote);batch.insert("seq",QStringLiteral("4"));batch.insert("origin_mutation_id",sync::uuid());batch.insert("changes",QJsonArray{foreign});QVERIFY(journal.execute("batch",{{"batch",batch},{"expected_cursor",QStringLiteral("cursor3")},{"after_cursor",QStringLiteral("cursor4")}}).ok());const auto preserved=repo.find(created.value.id);QVERIFY(preserved.ok());QCOMPARE(preserved.value.title,QStringLiteral("second"));QVERIFY(preserved.value.syncRemoteDeletionPending);QVERIFY(!QueryService::inView(preserved.value,"all",clock.localToday()));QVERIFY(!QueryService::effectiveActive(preserved.value));QVERIFY(QueryService::select(repo.all().value,"all",clock.localToday(),"second",true).isEmpty());const auto conflicts=journal.profile().value.value("conflicts").toArray();QCOMPARE(conflicts.size(),1);QVERIFY(conflicts.first().toObject().value("remote_summary").toString().contains("newer other device"));QVERIFY(journal.execute("resolve",{{"conflict_id",conflicts.first().toObject().value("id")},{"decision",QStringLiteral("local")}}).ok());QVERIFY(!repo.find(created.value.id).value.syncRemoteDeletionPending);const auto replacement=journal.execute("prepare",{});QVERIFY(replacement.ok());QVERIFY(!replacement.value.isEmpty());const auto replacementPayload=QJsonDocument::fromJson(replacement.value.value("payload").toString().toUtf8()).object();QVERIFY(replacementPayload.value("mutation_id")!=nextPayload.value("mutation_id"));QCOMPARE(replacementPayload.value("commands").toArray().last().toObject().value("patch").toObject().value("title").toString(),QStringLiteral("second"));
    }
    void syncInitialIdentityConflictResolvesAndEpochRebuildRetainsFrozenEvidence() {
        for(const auto& choice:{QStringLiteral("local"),QStringLiteral("remote")}){
            QTemporaryDir dir;SqliteDatabase local,remote;QVERIFY(local.open(dir.filePath("local.sqlite")).ok());QVERIFY(remote.open(dir.filePath("remote.sqlite")).ok());SystemClock clock;TaskRepository localRepo(local.connection()),remoteRepo(remote.connection());TaskService service(localRepo,clock);const auto made=service.create("local candidate","anytime");QVERIFY(made.ok());auto serverTask=made.value;serverTask.title="remote candidate";QVERIFY(remoteRepo.insert(serverTask).ok());const auto guest=sync::Mapping(local.connection()).capture(),server=sync::Mapping(remote.connection()).capture();QVERIFY(guest.ok());QVERIFY(server.ok());
            const auto pid=sync::uuid(),dataset=sync::uuid(),epoch=sync::uuid(),installation=sync::uuid();const QJsonObject initial{{"dto_version",1},{"base_head_seq",QStringLiteral("0")},{"entities",guest.value.array()},{"aliases",guest.value.aliases},{"successor_edges",guest.value.edges}};QSqlQuery q(local.connection());q.prepare("INSERT INTO sync_profiles(profile_id,mode,server_origin,server_instance_id,user_id,dataset_id,data_epoch,generation,auth_state,installation_id,phase,initial_payload_json) VALUES(?,'ACCOUNT','https://example.invalid',?,?,?,?,1,'AUTHENTICATED',?,'BOOTSTRAP',?)");for(const auto& v:QList<QVariant>{pid,sync::uuid(),sync::uuid(),dataset,epoch,installation,QString::fromUtf8(sync::json(initial))})q.addBindValue(v);QVERIFY(q.exec());sync::Journal journal(local.connection());const auto bootstrap=journal.execute("bootstrap",{});QVERIFY(bootstrap.ok());QJsonArray entities;for(auto e:server.value.entities){e.insert("revision",QStringLiteral("2"));entities.append(e);}const QJsonObject snapshot{{"dto_version",1},{"dataset_id",dataset},{"data_epoch",epoch},{"cutoff_seq",QStringLiteral("7")},{"entities",entities},{"structure_revisions",QJsonArray{}},{"aliases",server.value.aliases},{"successor_edges",server.value.edges}};
            const auto conflict=journal.execute("snapshot",{{"snapshot",snapshot},{"resume_cursor",QStringLiteral("cursor7")}});QVERIFY(conflict.ok());QVERIFY(conflict.value.value("needs_resolution").toBool());QCOMPARE(localRepo.find(made.value.id).value.title,QStringLiteral("local candidate"));const auto candidates=journal.profile().value.value("conflicts").toArray();QVERIFY(!candidates.isEmpty());QVERIFY(journal.execute("resolve",{{"conflict_id",candidates.first().toObject().value("id")},{"decision",choice}}).ok());QCOMPARE(localRepo.find(made.value.id).value.title,choice=="local"?QStringLiteral("local candidate"):QStringLiteral("remote candidate"));const auto import=journal.execute("initial",{});QVERIFY(import.ok());QVERIFY(sync::Schema::accepts("ImportPayload",import.value));QVERIFY(journal.execute("freeze_import",{{"payload",import.value}}).ok());
            const auto mid=bootstrap.value.value("mutation_id");QJsonObject receipt{{"protocol_version",1},{"mutation_id",mid},{"data_epoch",epoch},{"result",QStringLiteral("no_op")},{"batch_seq",QJsonValue(QJsonValue::Null)},{"observed_head_seq",QStringLiteral("7")},{"entity_revisions",QJsonArray{}},{"structure_revisions",QJsonArray{}},{"committed_at_utc",sync::now()}};QVERIFY(journal.execute("receipt",{{"receipt",receipt}}).ok());QVERIFY(journal.execute("initial_committed",{}).ok());const auto pending=journal.execute("prepare",{});QVERIFY(pending.ok());if(choice=="local")QVERIFY(!pending.value.isEmpty());else QVERIFY(pending.value.isEmpty());
            const auto oldFrozen=pending.value.value("payload").toString();QVERIFY(journal.execute("reconcile",{}).ok());const auto nextEpoch=sync::uuid();const auto rebuilt=journal.execute("rebuild",{{"dataset",QJsonObject{{"id",dataset},{"data_epoch",nextEpoch},{"head_seq",QStringLiteral("7")}}}});QVERIFY(rebuilt.ok());QCOMPARE(rebuilt.value.value("data_epoch").toString(),nextEpoch);QCOMPARE(rebuilt.value.value("phase").toString(),QStringLiteral("BOOTSTRAP"));
            q.prepare("SELECT origin_epoch,frozen_payload,state FROM sync_outbox WHERE profile_id=? AND mutation_id=?");q.addBindValue(pid);q.addBindValue(mid.toString());QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toString(),epoch);QVERIFY(!q.value(1).toByteArray().isEmpty());QCOMPARE(q.value(2).toString(),QStringLiteral("SUPERSEDED"));q.finish();if(!oldFrozen.isEmpty()){q.prepare("SELECT count(*) FROM sync_outbox WHERE profile_id=? AND frozen_payload=?");q.addBindValue(pid);q.addBindValue(oldFrozen.toUtf8());QVERIFY(q.exec());QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);}
        }
    }
    void schemaEightUpgradeBacksUpBeforeAddingTreeCohorts() {
        QTemporaryDir dir;const auto path=dir.filePath("schema-eight.sqlite");QVERIFY(createLegacyDatabase(path,8));const auto name=QUuid::createUuid().toString();
        {auto raw=QSqlDatabase::addDatabase("QSQLITE",name);raw.setDatabaseName(path);QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES('kept','kept','2026-10-25T00:00:00.000Z','2026-10-25T00:00:00.000Z')"));q.finish();raw.close();}QSqlDatabase::removeDatabase(name);
        SqliteDatabase db;QVERIFY(db.open(path).ok());TaskRepository repo(db.connection());const auto kept=repo.find("kept");QVERIFY(kept.ok());QVERIFY(kept.value.deletionGroupId.isEmpty());QVERIFY(kept.value.completionGroupId.isEmpty());QCOMPARE(kept.value.rowVersion,1);const auto backups=QDir(dir.filePath("backups/migrations")).entryList({"*.sqlite"},QDir::Files);QCOMPARE(backups.size(),1);
        const auto checkName=QUuid::createUuid().toString();{auto raw=QSqlDatabase::addDatabase("QSQLITE",checkName);raw.setDatabaseName(dir.filePath("backups/migrations/"+backups.first()));QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),8);q.finish();raw.close();}QSqlDatabase::removeDatabase(checkName);
    }
    void treeCohortsRoundTripAndOldJsonNormalizesWithoutPartialImport() {
        QTemporaryDir dir;SqliteDatabase source,target;QVERIFY(source.open(dir.filePath("source.sqlite")).ok());QVERIFY(target.open(dir.filePath("target.sqlite")).ok());SystemClock clock;TaskRepository repo(source.connection());TaskService service(repo,clock);const auto parent=service.create("parent","anytime");QVERIFY(parent.ok());QVERIFY(service.createSubtask("child",parent.value.id).ok());const auto removed=service.changeState(parent.value.id,parent.value.rowVersion,"delete");QVERIFY(removed.ok());
        DataTransfer out(source),in(target);const auto path=dir.filePath("tree.json");QVERIFY(out.exportJson(path).ok());auto plan=DataTransfer::preview(path);QVERIFY(plan.ok());QCOMPARE(plan.value.document.value("schemaVersion").toInt(),10);QVERIFY(in.apply(plan.value,false).ok());TaskRepository imported(target.connection());TaskService commands(imported,clock);QCOMPARE(imported.find(parent.value.id).value.deletionGroupId,removed.value.deletionGroupId);QVERIFY(commands.changeState(parent.value.id,imported.find(parent.value.id).value.rowVersion,"restore").ok());for(const auto& task:imported.all().value)QVERIFY(task.deletedAtUtc.isEmpty());
        const auto before=imported.all().value;auto malformed=plan.value;auto tables=malformed.document.value("tables").toObject();auto rows=tables.value("tasks").toArray();auto row=rows.last().toObject();row.insert("deleted_at_utc",QJsonValue::Null);rows[rows.size()-1]=row;tables.insert("tasks",rows);malformed.document.insert("tables",tables);QVERIFY(!in.apply(malformed,true).ok());QCOMPARE(imported.all().value,before);
        auto old=plan.value.document;old.insert("schemaVersion",8);tables=old.value("tables").toObject();rows=tables.value("tasks").toArray();for(int i=0;i<rows.size();++i){row=rows[i].toObject();row.remove("deletion_group_id");row.remove("completion_group_id");rows[i]=row;}tables.insert("tasks",rows);old.insert("tables",tables);QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(old).toJson());file.close();const auto legacy=DataTransfer::preview(path);QVERIFY(legacy.ok());QVERIFY(in.apply(legacy.value,true).ok());for(const auto& task:imported.all().value){QVERIFY(task.deletionGroupId.isEmpty());QVERIFY(!task.deletedAtUtc.isEmpty());}
    }
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
        QVERIFY(createLegacyDatabase(path,1));
        {const auto name=QUuid::createUuid().toString();{auto raw=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);raw.setDatabaseName(path);QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec(QStringLiteral("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES('retained','retained','2026-10-24T00:00:00.000Z','2026-10-24T00:00:00.000Z')")));q.finish();raw.close();}QSqlDatabase::removeDatabase(name);}

        QFile blocker(dir.filePath(QStringLiteral("backups")));QVERIFY(blocker.open(QIODevice::WriteOnly));blocker.close();
        {
            SqliteDatabase blocked;QCOMPARE(blocked.open(path).error.key,QStringLiteral("backupFailed"));
            auto raw=blocked.connection();QVERIFY(raw.open());QSqlQuery q(raw);QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),1);q.finish();raw.close();
        }
        QVERIFY(blocker.remove());
        SqliteDatabase upgraded;QVERIFY(upgraded.open(path).ok());
        QCOMPARE(TaskRepository(upgraded.connection()).all().value.size(),1);
        const auto backups=QDir(dir.filePath(QStringLiteral("backups/migrations"))).entryList({QStringLiteral("*.sqlite")},QDir::Files);QCOMPARE(backups.size(),1);
        QSqlQuery q(upgraded.connection());QVERIFY(q.exec(QStringLiteral("PRAGMA user_version")));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),10);
    }
    void roundtripPreviewMergeAndRestore()
    {
        QTemporaryDir dir;SqliteDatabase source,target;QVERIFY(source.open(dir.filePath(QStringLiteral("source.sqlite"))).ok());QVERIFY(target.open(dir.filePath(QStringLiteral("target.sqlite"))).ok());
        SystemClock clock;TaskRepository repo(source.connection());TaskService service(repo,clock);
        auto task=service.create(QStringLiteral("中文 \"quote\"\n=SUM(A1)"),QStringLiteral("today"));QVERIFY(task.ok());
        TaskPatch patch;patch.notes=FieldPatch<QString>::set(QStringLiteral("多行\n备注"));QVERIFY(service.edit(task.value.id,1,patch).ok());QVERIFY(service.createSubtask(QStringLiteral("item"),task.value.id).ok());
        DataTransfer exporter(source),importer(target);const auto json=dir.filePath(QStringLiteral("tasks.json"));QVERIFY(exporter.exportJson(json).ok());QVERIFY(exporter.exportCsv(dir.filePath(QStringLiteral("tasks.csv"))).ok());
        QVERIFY(!exporter.exportJson(source.connection().databaseName()).ok());
        auto plan=DataTransfer::preview(json);QVERIFY(plan.ok());QCOMPARE(plan.value.rows,3);QCOMPARE(TaskRepository(target.connection()).all().value.size(),0);
        auto imported=importer.apply(plan.value,false);QVERIFY(imported.ok());QCOMPARE(imported.value.added,3);QCOMPARE(TaskRepository(target.connection()).find(task.value.id).value,repo.find(task.value.id).value);
        auto merged=importer.apply(plan.value,false);QVERIFY(merged.ok());QCOMPARE(merged.value.skipped,3);
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
