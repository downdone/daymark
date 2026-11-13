#include "Telemetry.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QTimeZone>

namespace daymark::telemetry {
namespace {
QString uuid(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QByteArray json(const QJsonObject& value){return QJsonDocument(value).toJson(QJsonDocument::Compact);}
Result<QJsonObject> failure(const QString& key=QStringLiteral("telemetryLocalStorageFailed")){return Result<QJsonObject>::failure(ErrorCode::PersistenceError,key);}
QJsonObject emptyState(){return {{"enabled",false},{"confirmed",false},{"pending",false},{"epoch",0},{"needs_confirmation",false},{"last_flush_ms",0},{"retry_after_ms",0}};}
bool validEvent(const QJsonObject& event){
    const auto name=event.value("name").toString();const auto properties=event.value("properties").toObject();
    if(event.size()!=5 || event.value("version").toInt()!=1 || QUuid(event.value("id").toString()).isNull() || !QDateTime::fromString(event.value("occurred_at").toString(),Qt::ISODateWithMs).isValid())return false;
    if(!QRegularExpression(QStringLiteral("^(0|[1-9][0-9]{0,2})\\.(0|[1-9][0-9]{0,2})\\.(0|[1-9][0-9]{0,3})$")).match(properties.value("app_version").toString()).hasMatch() || !QStringList{"windows","macos","linux"}.contains(properties.value("os").toString()))return false;
    QString field;QStringList values;
    if(name=="app.startup"){field="duration_bucket";values={"lt_250ms","250_500ms","500_1000ms","1000_2000ms","ge_2000ms"};}
    else if(name=="surface.used"){field="surface";values={"main","widget","mini","ball"};}
    else if(name=="update.outcome"){field="outcome";values={"ready","updated","cancelled","failed"};}
    else if(name=="performance.sample"){field="memory_bucket";values={"lt_64MiB","64_128MiB","128_256MiB","256_512MiB","ge_512MiB"};}
    else return false;
    return properties.size()==3 && values.contains(properties.value(field).toString());
}
class Connection {
public:
    QString name=QStringLiteral("daymark-telemetry-")+uuid();
    QSqlDatabase database=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);
    ~Connection(){database.close();database=QSqlDatabase();QSqlDatabase::removeDatabase(name);}
    bool run(const QString& sql,const QVariantList& args={}){QSqlQuery q(database);if(!q.prepare(sql))return false;for(const auto& value:args)q.addBindValue(value);return q.exec();}
};
}
QJsonObject Event::payload() const {
    QJsonObject properties{{"app_version",QCoreApplication::applicationVersion()}};
#ifdef Q_OS_WIN
    properties.insert("os","windows");
#elif defined(Q_OS_MACOS)
    properties.insert("os","macos");
#else
    properties.insert("os","linux");
#endif
    QString name;
    switch(kind){
    case Kind::Startup:name="app.startup";properties.insert("duration_bucket",measurement<250?"lt_250ms":measurement<500?"250_500ms":measurement<1000?"500_1000ms":measurement<2000?"1000_2000ms":"ge_2000ms");break;
    case Kind::SurfaceUsed:name="surface.used";properties.insert("surface",surface==Surface::Main?"main":surface==Surface::Widget?"widget":surface==Surface::Mini?"mini":"ball");break;
    case Kind::UpdateResult:name="update.outcome";properties.insert("outcome",outcome==UpdateOutcome::Ready?"ready":outcome==UpdateOutcome::Updated?"updated":outcome==UpdateOutcome::Cancelled?"cancelled":"failed");break;
    case Kind::Performance:name="performance.sample";properties.insert("memory_bucket",measurement<64ll*1024*1024?"lt_64MiB":measurement<128ll*1024*1024?"64_128MiB":measurement<256ll*1024*1024?"128_256MiB":measurement<512ll*1024*1024?"256_512MiB":"ge_512MiB");break;
    }
    return {{"name",name},{"properties",properties}};
}
QString Queue::path(const QString& taskDatabase,const QString& binding){return taskDatabase+QStringLiteral(".telemetry-")+QString::fromLatin1(QCryptographicHash::hash(binding.toUtf8(),QCryptographicHash::Sha256).toHex().left(24))+QStringLiteral(".sqlite");}
Result<QJsonObject> Queue::execute(const QString& file,const QString& binding,const QString& operation,const QJsonObject& input,qint64 now){
    if(binding.isEmpty())return failure(QStringLiteral("telemetryProtocolMismatch"));
    if(!QFileInfo::exists(file) && operation!=QStringLiteral("set"))return {emptyState(),{}};
    Connection c;c.database.setDatabaseName(file);c.database.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=250"));if(!c.database.open())return failure();
    if(!c.run(QStringLiteral("PRAGMA journal_mode=DELETE")) || !c.run(QStringLiteral("PRAGMA synchronous=FULL")) || !c.run(QStringLiteral("PRAGMA max_page_count=256")))return failure();
    QSqlQuery version(c.database);if(!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())return failure();const int schema=version.value(0).toInt();version.finish();
    if(schema>1)return failure(QStringLiteral("telemetryProtocolMismatch"));
    if(!c.database.transaction())return failure();
    auto rollback=[&](const QString& key=QStringLiteral("telemetryLocalStorageFailed")){c.database.rollback();return failure(key);};
    if(schema==0){
        QSqlQuery tables(c.database);if(!tables.exec(QStringLiteral("SELECT count(*) FROM sqlite_master WHERE type='table'")) || !tables.next() || tables.value(0).toInt()!=0)return rollback(QStringLiteral("telemetryProtocolMismatch"));tables.finish();
        if(!c.run(QStringLiteral("CREATE TABLE state(id INTEGER PRIMARY KEY CHECK(id=1),binding TEXT NOT NULL,value TEXT NOT NULL)")) || !c.run(QStringLiteral("CREATE TABLE events(seq INTEGER PRIMARY KEY,id TEXT NOT NULL UNIQUE,payload BLOB NOT NULL,queued_ms INTEGER NOT NULL)")) || !c.run(QStringLiteral("CREATE TABLE batch(id INTEGER PRIMARY KEY CHECK(id=1),value BLOB NOT NULL)")) || !c.run(QStringLiteral("INSERT INTO state VALUES(1,?,?)"),{binding,QString::fromUtf8(json(emptyState()))}) || !c.run(QStringLiteral("PRAGMA user_version=1")))return rollback();
    }
    QSqlQuery read(c.database);if(!read.exec(QStringLiteral("SELECT binding,value FROM state WHERE id=1")) || !read.next())return rollback();
    if(read.value(0).toString()!=binding)return rollback(QStringLiteral("telemetryProtocolMismatch"));
    QJsonParseError error;auto document=QJsonDocument::fromJson(read.value(1).toByteArray(),&error);if(error.error!=QJsonParseError::NoError || !document.isObject())return rollback(QStringLiteral("telemetryProtocolMismatch"));QJsonObject state=document.object();read.finish();
    if(state.contains("consent_body")){
        const auto body=state.value("consent_body").toObject();
        if(body.size()!=4 || QUuid(body.value("operation_id").toString()).isNull() || !body.value("enabled").isBool() || body.value("policy_version").toInt()!=1 || body.value("expected_epoch").toDouble(-1)<0)return rollback(QStringLiteral("telemetryProtocolMismatch"));
    }
    auto clearEvents=[&](){return c.run(QStringLiteral("DELETE FROM batch")) && c.run(QStringLiteral("DELETE FROM events"));};
    if(operation=="set"){
        const bool enabled=input.value("enabled").toBool();if(!clearEvents())return rollback();state.insert("enabled",enabled);state.insert("confirmed",false);state.insert("pending",true);state.insert("needs_confirmation",false);state.insert("operation_id",uuid());state.remove("consent_body");state.insert("retry_after_ms",0);state.insert("failures",0);
    }else if(operation=="remote"){
        const auto remote=input.value("remote").toObject();if(remote.value("epoch").toDouble(-1)<0 || !remote.value("enabled").isBool() || remote.value("policy_version").toInt()!=1 || !remote.value("needs_confirmation").isBool())return rollback(QStringLiteral("telemetryProtocolMismatch"));
        state.insert("epoch",remote.value("epoch"));state.insert("server_known",true);
        if(remote.value("needs_confirmation").toBool() && state.value("pending").toBool() && state.value("enabled").toBool() && !input.value("allow_reconfirm").toBool()){
            state.insert("enabled",false);state.insert("confirmed",false);state.insert("pending",false);state.insert("needs_confirmation",true);state.remove("consent_body");state.remove("operation_id");if(!clearEvents())return rollback();
        }
        if(state.value("pending").toBool()){
            if(!state.contains("consent_body")){QJsonObject consent{{"operation_id",state.value("operation_id")},{"expected_epoch",remote.value("epoch")},{"enabled",state.value("enabled")},{"policy_version",1}};state.insert("consent_body",consent);}
        }else{
            const bool confirmed=state.value("enabled").toBool() && remote.value("enabled").toBool() && !remote.value("needs_confirmation").toBool();state.insert("confirmed",confirmed);state.insert("needs_confirmation",remote.value("needs_confirmation"));if(!confirmed){state.insert("enabled",false);if(!clearEvents())return rollback();}
        }
    }else if(operation=="confirm"){
        const auto remote=input.value("remote").toObject();if(!state.value("pending").toBool() || remote.value("epoch").toDouble()<=state.value("consent_body").toObject().value("expected_epoch").toDouble() || remote.value("enabled").toBool()!=state.value("enabled").toBool() || remote.value("policy_version").toInt()!=1 || remote.value("needs_confirmation").toBool())return rollback(QStringLiteral("telemetryProtocolMismatch"));
        state.insert("epoch",remote.value("epoch"));state.insert("pending",false);state.insert("confirmed",state.value("enabled"));state.insert("server_known",true);state.insert("needs_confirmation",false);state.insert("last_flush_ms",0);state.insert("retry_after_ms",0);state.remove("consent_body");state.remove("operation_id");
    }else if(operation=="pause"){
        if(!clearEvents())return rollback();state.insert("enabled",false);state.insert("confirmed",false);state.insert("pending",false);state.insert("needs_confirmation",true);state.remove("consent_body");state.remove("operation_id");
    }else if(operation=="conflict"){
        state.insert("confirmed",false);state.remove("consent_body");state.insert("operation_id",uuid());state.insert("retry_after_ms",now+30000);
    }else if(operation=="retry"){
        const int failures=qMin(8,state.value("failures").toInt()+1);state.insert("failures",failures);const auto serverDelay=qBound<qint64>(0,qint64(input.value("server_retry_delay_ms").toDouble()),24ll*60*60*1000);state.insert("server_retry_after_ms",now+serverDelay);state.insert("retry_after_ms",now+qMax<qint64>(serverDelay,qMin<qint64>(24ll*60*60*1000,30000ll*(1<<failures)+QRandomGenerator::global()->bounded(10000))));
    }else if(operation=="retry_now"){
        state.insert("retry_after_ms",state.value("server_retry_after_ms").toDouble());
    }else if(operation=="record"){
        if(state.value("enabled").toBool() && state.value("confirmed").toBool() && !state.value("pending").toBool()){
            QJsonObject event=input.value("event").toObject();event.insert("id",uuid());event.insert("version",1);event.insert("occurred_at",QDateTime::fromMSecsSinceEpoch(now,QTimeZone::UTC).toString(Qt::ISODateWithMs));if(!validEvent(event))return rollback(QStringLiteral("telemetryProtocolMismatch"));
            if(!c.run(QStringLiteral("DELETE FROM events WHERE queued_ms<?"),{now-3ll*24*60*60*1000}) || !c.run(QStringLiteral("DELETE FROM events WHERE seq IN (SELECT seq FROM events ORDER BY seq LIMIT max(0,(SELECT count(*) FROM events)-999))")) || !c.run(QStringLiteral("INSERT INTO events(id,payload,queued_ms) VALUES(?,?,?)"),{event.value("id").toString(),json(event),now}))return rollback();
        }
    }else if(operation=="ack"){
        QSqlQuery frozen(c.database);if(!frozen.exec(QStringLiteral("SELECT value FROM batch WHERE id=1")) || !frozen.next())return rollback(QStringLiteral("telemetryProtocolMismatch"));const auto body=QJsonDocument::fromJson(frozen.value(0).toByteArray()).object();frozen.finish();if(body.value("batch_id")!=input.value("batch_id"))return rollback(QStringLiteral("telemetryProtocolMismatch"));for(const auto& item:body.value("events").toArray())if(!c.run(QStringLiteral("DELETE FROM events WHERE id=?"),{item.toObject().value("id").toString()}))return rollback();if(!c.run(QStringLiteral("DELETE FROM batch")))return rollback();state.insert("last_flush_ms",now);state.insert("retry_after_ms",0);state.insert("failures",0);
    }else if(operation!="inspect" && operation!="batch")return rollback(QStringLiteral("telemetryProtocolMismatch"));
    QJsonObject output=state;
    if(operation=="batch" && state.value("confirmed").toBool() && !state.value("pending").toBool()){
        QSqlQuery oldest(c.database);if(!oldest.exec(QStringLiteral("SELECT min(queued_ms) FROM events")) || !oldest.next())return rollback();const auto oldestAt=oldest.value(0);oldest.finish();if(!oldestAt.isNull() && oldestAt.toLongLong()<now-3ll*24*60*60*1000){if(!c.run(QStringLiteral("DELETE FROM batch")) || !c.run(QStringLiteral("DELETE FROM events WHERE queued_ms<?"),{now-3ll*24*60*60*1000}))return rollback();}
        QSqlQuery frozen(c.database);if(!frozen.exec(QStringLiteral("SELECT value FROM batch WHERE id=1")))return rollback();QJsonObject body;if(frozen.next())body=QJsonDocument::fromJson(frozen.value(0).toByteArray()).object();frozen.finish();
        if(!body.isEmpty()){
            if(body.size()!=4 || QUuid(body.value("batch_id").toString()).isNull() || body.value("schema_version").toInt()!=1 || body.value("consent_epoch")!=state.value("epoch") || body.value("events").toArray().isEmpty() || body.value("events").toArray().size()>32)return rollback(QStringLiteral("telemetryProtocolMismatch"));
            bool expired=false;for(const auto& event:body.value("events").toArray())if(QDateTime::fromString(event.toObject().value("occurred_at").toString(),Qt::ISODateWithMs).toMSecsSinceEpoch()<now-3ll*24*60*60*1000)expired=true;
            if(expired){if(!c.run(QStringLiteral("DELETE FROM batch")))return rollback();body={};}
        }
        if(body.isEmpty()){
            QSqlQuery rows(c.database);if(!rows.exec(QStringLiteral("SELECT payload FROM events ORDER BY seq LIMIT 32")))return rollback();QJsonArray events;while(rows.next()){auto e=QJsonDocument::fromJson(rows.value(0).toByteArray()).object();if(!validEvent(e))return rollback(QStringLiteral("telemetryProtocolMismatch"));events.append(e);}rows.finish();if(!events.isEmpty()){body={{"batch_id",uuid()},{"consent_epoch",state.value("epoch")},{"schema_version",1},{"events",events}};if(json(body).size()>32768)return rollback(QStringLiteral("telemetryProtocolMismatch"));if(!c.run(QStringLiteral("INSERT INTO batch VALUES(1,?)"),{json(body)}))return rollback();}
        }
        if(!body.isEmpty()){for(const auto& event:body.value("events").toArray())if(!validEvent(event.toObject()))return rollback(QStringLiteral("telemetryProtocolMismatch"));output.insert("batch",body);}
        else{state.insert("last_flush_ms",now);output.insert("last_flush_ms",now);}
    }
    if(!c.run(QStringLiteral("UPDATE state SET value=? WHERE id=1"),{QString::fromUtf8(json(state))}) || !c.database.commit())return rollback();return {output,{}};
}
}
