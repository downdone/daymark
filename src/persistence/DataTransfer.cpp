#include "DataTransfer.h"
#include "UnifiedTaskMigration.h"
#include "TaskRepository.h"
#include "services/EventService.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QSet>
#include <QSettings>
#include <cmath>
namespace daymark {
namespace {
const QStringList tables={QStringLiteral("areas"),QStringLiteral("projects"),QStringLiteral("headings"),QStringLiteral("tags"),QStringLiteral("recurrence_templates"),QStringLiteral("tasks"),QStringLiteral("checklist_items"),QStringLiteral("task_tags"),QStringLiteral("task_view_order"),QStringLiteral("events"),QStringLiteral("event_tasks"),QStringLiteral("event_reminder_history"),QStringLiteral("task_edit_history"),QStringLiteral("task_reminder_history"),QStringLiteral("task_schedules"),QStringLiteral("task_links")};
bool supportedSchema(const QJsonObject& root){const double v=root.value(QStringLiteral("schemaVersion")).toDouble(-1);return v==1 || v==2 || v==3 || v==4 || v==5 || v==6 || v==7 || v==8 || v==9;}
QJsonObject normalized(QJsonObject root) {
    if(root.value(QStringLiteral("schemaVersion")).toInt()<3){auto data=root.value(QStringLiteral("tables")).toObject();for(const auto& table:{QStringLiteral("events"),QStringLiteral("event_tasks")})if(!data.contains(table))data.insert(table,QJsonArray{});root.insert(QStringLiteral("tables"),data);}
    if(root.value(QStringLiteral("schemaVersion")).toInt()<4){auto data=root.value(QStringLiteral("tables")).toObject();data.insert(QStringLiteral("event_reminder_history"),QJsonArray{});auto rows=data.value(QStringLiteral("events")).toArray();for(int i=0;i<rows.size();++i){auto row=rows[i].toObject();row.insert(QStringLiteral("repeat_frequency"),QStringLiteral("NONE"));row.insert(QStringLiteral("repeat_interval"),1);row.insert(QStringLiteral("repeat_until"),QJsonValue(QJsonValue::Null));row.insert(QStringLiteral("alert_minutes"),-1);rows[i]=row;}data.insert(QStringLiteral("events"),rows);root.insert(QStringLiteral("tables"),data);}if(root.value(QStringLiteral("schemaVersion")).toInt()<5) {
        auto data=root.value(QStringLiteral("tables")).toObject();auto projects=data.value(QStringLiteral("projects")).toArray();auto areas=data.value(QStringLiteral("areas")).toArray();
        for(int i=0;i<projects.size();++i){auto row=projects[i].toObject();row.insert(QStringLiteral("parent_id"),row.value(QStringLiteral("area_id")));row.insert(QStringLiteral("area_id"),QJsonValue::Null);row.insert(QStringLiteral("archived"),0);projects[i]=row;}
        for(int i=0;i<areas.size();++i){auto area=areas[i].toObject();if(root.value(QStringLiteral("schemaVersion")).toInt()==1 && !area.contains(QStringLiteral("archived")))area.insert(QStringLiteral("archived"),0);QJsonObject row;
            for(const auto& key:{"id","sort_order","row_version","created_at_utc","updated_at_utc","deleted_at_utc"})row.insert(QLatin1String(key),area.value(QLatin1String(key)));
            row.insert(QStringLiteral("title"),area.value(QStringLiteral("name")));row.insert(QStringLiteral("description"),QString());row.insert(QStringLiteral("planning_state"),QStringLiteral("ACTIVE"));row.insert(QStringLiteral("status"),QStringLiteral("OPEN"));
            for(const auto& key:{"area_id","parent_id","start_on","deadline_on","completed_at_utc"})row.insert(QLatin1String(key),QJsonValue::Null);
            row.insert(QStringLiteral("archived"),area.value(QStringLiteral("archived")).toInt());projects.prepend(row);
            if(area.value(QStringLiteral("deleted_at_utc")).isNull())area.insert(QStringLiteral("deleted_at_utc"),area.value(QStringLiteral("updated_at_utc")));areas[i]=area;
        }
        auto tasks=data.value(QStringLiteral("tasks")).toArray();for(int i=0;i<tasks.size();++i){auto row=tasks[i].toObject();if(!row.value(QStringLiteral("direct_area_id")).isNull()){row.insert(QStringLiteral("project_id"),row.value(QStringLiteral("direct_area_id")));row.insert(QStringLiteral("direct_area_id"),QJsonValue::Null);}tasks[i]=row;}
        data.insert(QStringLiteral("projects"),projects);data.insert(QStringLiteral("areas"),areas);data.insert(QStringLiteral("tasks"),tasks);root.insert(QStringLiteral("tables"),data);
    }
    if(root.value(QStringLiteral("schemaVersion")).toInt()<6) {
        auto data=root.value(QStringLiteral("tables")).toObject();auto tasks=data.value(QStringLiteral("tasks")).toArray();QHash<QString,QJsonObject> parents;
        for(int i=0;i<tasks.size();++i){auto row=tasks[i].toObject();row.insert(QStringLiteral("parent_task_id"),QJsonValue::Null);row.insert(QStringLiteral("pinned"),0);row.insert(QStringLiteral("important"),0);tasks[i]=row;parents.insert(row.value(QStringLiteral("id")).toString(),row);}
        auto checklist=data.value(QStringLiteral("checklist_items")).toArray();
        for(int i=0;i<checklist.size();++i){auto item=checklist[i].toObject();const auto parentId=item.value(QStringLiteral("task_id")).toString();if(!parents.contains(parentId))continue;auto child=parents.value(parentId);
            child.insert(QStringLiteral("id"),item.value(QStringLiteral("id")));child.insert(QStringLiteral("title"),item.value(QStringLiteral("title")));child.insert(QStringLiteral("note_text"),QStringLiteral(""));child.insert(QStringLiteral("parent_task_id"),parentId);
            for(const auto& key:{"sort_order","row_version","created_at_utc","updated_at_utc","deleted_at_utc"})child.insert(QLatin1String(key),item.value(QLatin1String(key)));
            for(const auto& key:{"start_on","deadline_on","remind_at_utc","reminder_fired_at_utc","recurrence_template_id","occurrence_on","heading_id","cancelled_at_utc"})child.insert(QLatin1String(key),QJsonValue::Null);
            child.insert(QStringLiteral("today_focus"),0);child.insert(QStringLiteral("evening"),0);child.insert(QStringLiteral("instance_overridden"),0);child.insert(QStringLiteral("status"),item.value(QStringLiteral("is_done")).toInt()?QStringLiteral("COMPLETED"):QStringLiteral("OPEN"));child.insert(QStringLiteral("completed_at_utc"),item.value(QStringLiteral("is_done")).toInt()?item.value(QStringLiteral("updated_at_utc")):QJsonValue(QJsonValue::Null));tasks.append(child);
            if(item.value(QStringLiteral("deleted_at_utc")).isNull())item.insert(QStringLiteral("deleted_at_utc"),item.value(QStringLiteral("updated_at_utc")));checklist[i]=item;
        }
        data.insert(QStringLiteral("tasks"),tasks);data.insert(QStringLiteral("checklist_items"),checklist);data.insert(QStringLiteral("task_edit_history"),QJsonArray{});root.insert(QStringLiteral("tables"),data);
    }
    if(root.value(QStringLiteral("schemaVersion")).toInt()<7) {
        auto data=root.value(QStringLiteral("tables")).toObject();auto tasks=data.value(QStringLiteral("tasks")).toArray();QJsonArray history;
        for(int i=0;i<tasks.size();++i){auto row=tasks[i].toObject();row.insert(QStringLiteral("reminder_repeat"),QStringLiteral("NONE"));row.insert(QStringLiteral("reminder_every"),1);row.insert(QStringLiteral("reminder_zone"),QStringLiteral("UTC"));row.insert(QStringLiteral("reminder_anchor_utc"),row.value(QStringLiteral("remind_at_utc")));
            if(!row.value(QStringLiteral("remind_at_utc")).isNull() && !row.value(QStringLiteral("reminder_fired_at_utc")).isNull())history.append(QJsonObject{{QStringLiteral("id"),row.value(QStringLiteral("id"))},{QStringLiteral("task_id"),row.value(QStringLiteral("id"))},{QStringLiteral("scheduled_at_utc"),row.value(QStringLiteral("remind_at_utc"))},{QStringLiteral("fired_at_utc"),row.value(QStringLiteral("reminder_fired_at_utc"))}});tasks[i]=row;
        }
        data.insert(QStringLiteral("tasks"),tasks);data.insert(QStringLiteral("task_reminder_history"),history);root.insert(QStringLiteral("tables"),data);
    }
    if(root.value(QStringLiteral("schemaVersion")).toInt()<8){auto data=root.value(QStringLiteral("tables")).toObject();for(const auto& table:{QStringLiteral("task_schedules"),QStringLiteral("task_links")})data.insert(table,QJsonArray{});
        auto events=data.value(QStringLiteral("events")).toArray();for(int i=0;i<events.size();++i){auto row=events[i].toObject();row.insert(QStringLiteral("task_migrated"),0);events[i]=row;}data.insert(QStringLiteral("events"),events);
        auto rules=data.value(QStringLiteral("recurrence_templates")).toArray();for(int i=0;i<rules.size();++i){auto row=rules[i].toObject();row.insert(QStringLiteral("source_task_id"),QJsonValue::Null);rules[i]=row;}data.insert(QStringLiteral("recurrence_templates"),rules);root.insert(QStringLiteral("tables"),data);
    }
    if(root.value(QStringLiteral("schemaVersion")).toInt()<9){auto data=root.value(QStringLiteral("tables")).toObject();auto rows=data.value(QStringLiteral("tasks")).toArray();for(int i=0;i<rows.size();++i){auto row=rows[i].toObject();row.insert(QStringLiteral("deletion_group_id"),QJsonValue::Null);row.insert(QStringLiteral("completion_group_id"),QJsonValue::Null);rows[i]=row;}data.insert(QStringLiteral("tasks"),rows);root.insert(QStringLiteral("tables"),data);}
    root.insert(QStringLiteral("schemaVersion"),9);return root;
}
template<class T> Result<T> invalid(){return Result<T>::failure(ErrorCode::ValidationError,QStringLiteral("importInvalid"));}
template<class T> Result<T> ioError(){return Result<T>::failure(ErrorCode::PersistenceError,QStringLiteral("transferFailed"));}
bool protectedOutput(const QString& path,const QString& database)
{
    const QFileInfo target(path),source(database);
    return target.absoluteFilePath().startsWith(source.absoluteFilePath(),Qt::CaseInsensitive)
        || (!target.canonicalFilePath().isEmpty() && target.canonicalFilePath().startsWith(source.canonicalFilePath(),Qt::CaseInsensitive));
}
}
Result<QJsonObject> DataTransfer::document(QSqlDatabase db)
{
    QJsonObject data;
    for(const auto& table:tables) {
        QSqlQuery q(db);if(!q.exec(QStringLiteral("SELECT * FROM ")+table))return ioError<QJsonObject>();
        const auto record=q.record();QJsonArray rows;
        while(q.next()) {QJsonObject row;for(int i=0;i<record.count();++i)row.insert(record.fieldName(i),q.value(i).isNull()?QJsonValue(QJsonValue::Null):QJsonValue::fromVariant(q.value(i)));rows.append(row);}
        data.insert(table,rows);
    }
    QSettings settings(QFileInfo(db.databaseName()).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    QJsonObject portable{{QStringLiteral("sundayFirst"),settings.value(QStringLiteral("calendar/sundayFirst"),false).toBool()},
        {QStringLiteral("themeMode"),qBound(0,settings.value(QStringLiteral("appearance/themeMode"),0).toInt(),2)},
        {QStringLiteral("textScale"),qBound(1.0,settings.value(QStringLiteral("appearance/textScale"),1.0).toDouble(),2.0)}};
    return {QJsonObject{{QStringLiteral("format"),QStringLiteral("daymark")},{QStringLiteral("formatVersion"),1},{QStringLiteral("schemaVersion"),9},
        {QStringLiteral("createdAtUtc"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{QStringLiteral("tables"),data},{QStringLiteral("settings"),portable}}, {}};
}
Result<void> DataTransfer::exportJson(const QString& path) const
{
    if(protectedOutput(path,m_database.connection().databaseName()))return invalid<void>();
    // Single read transaction makes all table reads share one WAL snapshot.
    auto db=m_database.connection();if(!db.transaction())return ioError<void>();
    const auto result=document(db);if(!result.ok()){db.rollback();return {result.error};}
    if(!db.commit()){db.rollback();return ioError<void>();}
    QSaveFile output(path);if(!output.open(QIODevice::WriteOnly))return ioError<void>();
    const auto bytes=QJsonDocument(result.value).toJson(QJsonDocument::Indented);
    if(output.write(bytes)!=bytes.size() || !output.commit())return ioError<void>();return {};
}
Result<void> DataTransfer::exportCsv(const QString& path) const
{
    if(protectedOutput(path,m_database.connection().databaseName()))return invalid<void>();
    QSqlQuery q(m_database.connection());if(!q.exec(QStringLiteral("SELECT id,title,note_text,planning_state,status,start_on,deadline_on FROM tasks WHERE deleted_at_utc IS NULL ORDER BY sort_order,id")))return ioError<void>();
    QByteArray bytes="\xEF\xBB\xBFid,title,notes,planning,status,start_on,deadline_on\r\n";
    while(q.next()) {
        QStringList cells;
        for(int i=0;i<7;++i){auto text=q.value(i).toString();if(!text.isEmpty() && QStringLiteral("=+-@\t\r").contains(text.front()))text.prepend(QLatin1Char('\''));text.replace(QLatin1Char('"'),QStringLiteral("\"\""));cells.append(QLatin1Char('"')+text+QLatin1Char('"'));}
        bytes+=cells.join(QLatin1Char(',')).toUtf8()+"\r\n";
    }
    QSaveFile output(path);if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit())return ioError<void>();return {};
}
Result<ImportPlan> DataTransfer::preview(const QString& path,bool snapshot)
{
    QTemporaryDir temporary;if(!temporary.isValid())return ioError<ImportPlan>();
    QJsonObject root;
    if(snapshot) {
        if(QFileInfo::exists(path+QStringLiteral("-wal")))return invalid<ImportPlan>();
        const auto copy=temporary.filePath(QStringLiteral("snapshot.sqlite"));if(!QFile::copy(path,copy))return ioError<ImportPlan>();
        SqliteDatabase backup;const auto opened=backup.open(copy);if(!opened.ok())return {{},opened.error};
        const auto exported=document(backup.connection());if(!exported.ok())return {{},exported.error};root=exported.value;
    } else {
        QFile input(path);if(!input.open(QIODevice::ReadOnly) || input.size()>50*1024*1024)return ioError<ImportPlan>();
        QJsonParseError error;const auto json=QJsonDocument::fromJson(input.readAll(),&error);
        if(error.error!=QJsonParseError::NoError || !json.isObject())return invalid<ImportPlan>();root=json.object();
    }
    if(root.value(QStringLiteral("format")).toString()!=QStringLiteral("daymark") || root.value(QStringLiteral("formatVersion")).toDouble(-1)!=1
       || !supportedSchema(root) || !root.value(QStringLiteral("tables")).isObject())return invalid<ImportPlan>();
    root=normalized(root);
    const auto data=root.value(QStringLiteral("tables")).toObject();
    auto keys=data.keys();auto expected=tables;keys.sort();expected.sort();if(keys!=expected)return invalid<ImportPlan>();
    int rows=0;for(const auto& table:tables){if(!data.value(table).isArray())return invalid<ImportPlan>();rows+=data.value(table).toArray().size();if(rows>100000)return invalid<ImportPlan>();}
    SqliteDatabase validation;const auto opened=validation.open(temporary.filePath(QStringLiteral("validation.sqlite")));if(!opened.ok())return {{},opened.error};
    const auto valid=insert(validation.connection(),root,false,true);if(!valid.ok())return {{},valid.error};
    const auto converted=document(validation.connection());if(!converted.ok())return {{},converted.error};auto final=converted.value;if(root.contains(QStringLiteral("settings")))final.insert(QStringLiteral("settings"),root.value(QStringLiteral("settings")));else final.remove(QStringLiteral("settings"));
    return {{final,rows},{}};
}
Result<ImportOutcome> DataTransfer::insert(QSqlDatabase db,const QJsonObject& input,bool replace,bool validateOnly)
{
    if(!supportedSchema(input))return invalid<ImportOutcome>();
    const auto root=normalized(input);
    if(root.value(QStringLiteral("format")).toString()!=QStringLiteral("daymark") || root.value(QStringLiteral("formatVersion")).toDouble(-1)!=1)return invalid<ImportOutcome>();
    const auto tableData=root.value(QStringLiteral("tables")).toObject();auto keys=tableData.keys();auto expected=tables;keys.sort();expected.sort();if(keys!=expected)return invalid<ImportOutcome>();
    for(const auto& table:tables)if(!tableData.value(table).isArray())return invalid<ImportOutcome>();
    if(root.contains(QStringLiteral("settings"))) {
        if(!root.value(QStringLiteral("settings")).isObject())return invalid<ImportOutcome>();
        const auto settings=root.value(QStringLiteral("settings")).toObject();
        if(settings.keys()!=QStringList{QStringLiteral("sundayFirst"),QStringLiteral("textScale"),QStringLiteral("themeMode")}
           || !settings.value(QStringLiteral("sundayFirst")).isBool() || !settings.value(QStringLiteral("themeMode")).isDouble()
           || settings.value(QStringLiteral("themeMode")).toDouble()!=settings.value(QStringLiteral("themeMode")).toInt(-1)
           || settings.value(QStringLiteral("themeMode")).toInt(-1)<0 || settings.value(QStringLiteral("themeMode")).toInt()>2
           || !settings.value(QStringLiteral("textScale")).isDouble() || settings.value(QStringLiteral("textScale")).toDouble()<1 || settings.value(QStringLiteral("textScale")).toDouble()>2)return invalid<ImportOutcome>();
    }
    if(!db.transaction())return ioError<ImportOutcome>();
    auto fail=[&]{db.rollback();return invalid<ImportOutcome>();};
    {QSqlQuery deferred(db);if(!deferred.exec(QStringLiteral("PRAGMA defer_foreign_keys=ON")))return fail();}
    QSqlQuery q(db);ImportOutcome result;const auto data=root.value(QStringLiteral("tables")).toObject();QSet<QString> skippedEvents,skippedTasks;
    if(replace)for(auto it=tables.crbegin();it!=tables.crend();++it)if(!q.exec(QStringLiteral("DELETE FROM ")+*it))return fail();
    for(const auto& table:tables) {
        if(!q.exec(QStringLiteral("PRAGMA table_info(")+table+QLatin1Char(')')))return fail();
        QStringList columns,types;while(q.next()){columns<<q.value(1).toString();types<<q.value(2).toString();}q.finish();
        QSet<QString> seen;
        for(const auto& value:data.value(table).toArray()) {
            if(!value.isObject())return fail();auto row=value.toObject();
            if(root.value(QStringLiteral("schemaVersion")).toInt()==1 && table==QStringLiteral("areas") && !row.contains(QStringLiteral("archived")))row.insert(QStringLiteral("archived"),0);
            auto keys=row.keys();auto expected=columns;keys.sort();expected.sort();if(keys!=expected)return fail();
            if((table==QStringLiteral("event_tasks") || table==QStringLiteral("event_reminder_history")) && skippedEvents.contains(row.value(QStringLiteral("event_id")).toString())){++result.skipped;continue;}
            if((table==QStringLiteral("task_schedules") && skippedTasks.contains(row.value(QStringLiteral("task_id")).toString())) || (table==QStringLiteral("task_links") && skippedTasks.contains(row.value(QStringLiteral("source_id")).toString()))){++result.skipped;continue;}
            if(table==QStringLiteral("tasks")) {
                if(!row.value(QStringLiteral("deletion_group_id")).isNull() && (row.value(QStringLiteral("deleted_at_utc")).isNull() || row.value(QStringLiteral("deletion_group_id")).toString().isEmpty()))return fail();
                if(!row.value(QStringLiteral("completion_group_id")).isNull() && (row.value(QStringLiteral("status")).toString()!=QStringLiteral("COMPLETED") || row.value(QStringLiteral("completion_group_id")).toString().isEmpty()))return fail();
                if(!QTimeZone(row.value(QStringLiteral("reminder_zone")).toString().toUtf8()).isValid())return fail();
                if(row.value(QStringLiteral("reminder_repeat")).toString()!=QStringLiteral("NONE") && (row.value(QStringLiteral("remind_at_utc")).isNull() || row.value(QStringLiteral("reminder_anchor_utc")).isNull()))return fail();
            }
            if(table==QStringLiteral("task_edit_history") && !QJsonDocument::fromJson(row.value(QStringLiteral("fields_json")).toString().toUtf8()).isArray())return fail();
            if(table==QStringLiteral("events")) {
                CalendarEvent e;e.title=row.value(QStringLiteral("title")).toString();e.allDay=row.value(QStringLiteral("all_day")).toInt()==1;
                e.startOn=QDate::fromString(row.value(QStringLiteral("start_on")).toString(),Qt::ISODate);e.endOn=QDate::fromString(row.value(QStringLiteral("end_on")).toString(),Qt::ISODate);
                e.startUtc=QDateTime::fromString(row.value(QStringLiteral("start_at_utc")).toString(),Qt::ISODateWithMs);e.endUtc=QDateTime::fromString(row.value(QStringLiteral("end_at_utc")).toString(),Qt::ISODateWithMs);e.timeZone=row.value(QStringLiteral("time_zone")).toString();
                e.repeatFrequency=row.value(QStringLiteral("repeat_frequency")).toString();e.repeatInterval=row.value(QStringLiteral("repeat_interval")).toInt();e.repeatUntil=QDate::fromString(row.value(QStringLiteral("repeat_until")).toString(),Qt::ISODate);e.alertMinutes=row.value(QStringLiteral("alert_minutes")).toInt();if(!row.value(QStringLiteral("repeat_until")).isNull() && !e.repeatUntil.isValid())return fail();
                if(!EventService::validate(e).ok())return fail();
            }
            if(table==QStringLiteral("recurrence_templates")) {
                const auto frequency=row.value(QStringLiteral("frequency")).toString();
                const auto interval=row.value(QStringLiteral("interval_n")).toDouble();
                if(interval<1 || interval>36500)return fail();
                if((frequency==QStringLiteral("MONTHLY") || frequency==QStringLiteral("YEARLY")) && (row.value(QStringLiteral("month_day")).toInt()<1 || row.value(QStringLiteral("month_day")).toInt()>31))return fail();
                if(frequency==QStringLiteral("YEARLY") && !QDate(2000,row.value(QStringLiteral("year_month")).toInt(),row.value(QStringLiteral("month_day")).toInt()).isValid())return fail();
            }
            const auto identity=row.contains(QStringLiteral("id"))?row.value(QStringLiteral("id")).toString():table==QStringLiteral("task_tags")?row.value(QStringLiteral("task_id")).toString()+QLatin1Char(':')+row.value(QStringLiteral("tag_id")).toString():table==QStringLiteral("task_schedules")?row.value(QStringLiteral("task_id")).toString():table==QStringLiteral("task_links")?row.value(QStringLiteral("source_id")).toString()+QLatin1Char(':')+row.value(QStringLiteral("target_id")).toString():table==QStringLiteral("event_tasks")?row.value(QStringLiteral("event_id")).toString()+QLatin1Char(':')+row.value(QStringLiteral("task_id")).toString():row.value(QStringLiteral("view_code")).toString()+QLatin1Char(':')+row.value(QStringLiteral("task_id")).toString();
            if(identity.isEmpty() || seen.contains(identity))return fail();seen.insert(identity);
            QList<QVariant> values;
            for(int i=0;i<columns.size();++i) {
                const auto& column=columns[i];const auto cell=row.value(column);
                if(cell.isNull()){values.append(QVariant());continue;}
                if(types[i]==QStringLiteral("INTEGER")) {
                    if(!cell.isDouble() || !std::isfinite(cell.toDouble()) || std::floor(cell.toDouble())!=cell.toDouble() || std::abs(cell.toDouble())>9007199254740991.0)return fail();
                    if(column==QStringLiteral("row_version") && cell.toDouble()<1)return fail();
                    values.append(qint64(cell.toDouble()));
                } else {
                    if(!cell.isString())return fail();const auto text=cell.toString();
                    if(column.endsWith(QStringLiteral("_on"))) {
                        const auto date=QDate::fromString(text,Qt::ISODate);if(!date.isValid() || date.toString(Qt::ISODate)!=text)return fail();
                    } else if(column.endsWith(QStringLiteral("_utc"))) {
                        const auto date=QDateTime::fromString(text,Qt::ISODateWithMs);if(!text.endsWith(QLatin1Char('Z')) || !date.isValid())return fail();
                    }
                    if((column==QStringLiteral("title") || column==QStringLiteral("name")) && (text.trimmed().isEmpty() || text.toUcs4().size()>512))return fail();
                    if(column==QStringLiteral("normalized_name") && text!=row.value(QStringLiteral("name")).toString().trimmed().toCaseFolded())return fail();
                    if(column==QStringLiteral("task_defaults_json") && !QJsonDocument::fromJson(text.toUtf8()).isObject())return fail();
                    values.append(text);
                }
            }
            QStringList placeholders;for(int i=0;i<columns.size();++i)placeholders<<QStringLiteral("?");
            QString conflict;
            if(!validateOnly && !replace)conflict=table==QStringLiteral("task_schedules")?QStringLiteral(" ON CONFLICT(task_id) DO NOTHING"):table==QStringLiteral("task_links")?QStringLiteral(" ON CONFLICT(source_id,target_id) DO NOTHING"):table==QStringLiteral("task_tags")?QStringLiteral(" ON CONFLICT(task_id,tag_id) DO NOTHING"):table==QStringLiteral("event_tasks")?QStringLiteral(" ON CONFLICT(event_id,task_id) DO NOTHING"):table==QStringLiteral("task_view_order")?QStringLiteral(" ON CONFLICT(view_code,task_id) DO NOTHING"):QStringLiteral(" ON CONFLICT(id) DO NOTHING");
            q.prepare(QStringLiteral("INSERT INTO ")+table+QLatin1Char('(')+columns.join(QLatin1Char(','))+QStringLiteral(") VALUES(")+placeholders.join(QLatin1Char(','))+QLatin1Char(')')+conflict);
            for(const auto& cell:values)q.addBindValue(cell);if(!q.exec())return fail();
            if(q.numRowsAffected()==0){++result.skipped;if(table==QStringLiteral("tasks"))skippedTasks.insert(row.value(QStringLiteral("id")).toString());if(table==QStringLiteral("events"))skippedEvents.insert(row.value(QStringLiteral("id")).toString());}else ++result.added;
        }
    }
    if(!convertLegacyEvents(db,QDateTime::currentDateTimeUtc()).ok())return fail();
    {TaskRepository repository(db);const auto all=repository.all();if(!all.ok())return fail();for(const auto& task:all.value)if(task.schedule){const auto& s=*task.schedule;CalendarEvent e;e.title=task.title;e.allDay=s.allDay;e.startOn=s.startOn;e.endOn=s.endOn;e.startUtc=s.startUtc;e.endUtc=s.endUtc;e.timeZone=s.timeZone;e.alertMinutes=s.alertMinutes;if(!EventService::validate(e).ok())return fail();}}
    if(!q.exec(QStringLiteral("SELECT c.id FROM projects c JOIN projects p ON p.id=c.parent_id WHERE c.id=c.parent_id OR p.parent_id IS NOT NULL LIMIT 1")) || q.next())return fail();q.finish();
    {QHash<QString,QString> parents;QSet<QString> checked;
        if(!q.exec(QStringLiteral("SELECT id,parent_task_id FROM tasks")))return fail();while(q.next())parents.insert(q.value(0).toString(),q.value(1).toString());q.finish();
        for(auto it=parents.cbegin();it!=parents.cend();++it){auto next=it.key();QSet<QString> path;while(!next.isEmpty() && !checked.contains(next)){if(path.contains(next))return fail();path.insert(next);next=parents.value(next);}checked.unite(path);}
    }
    if(!q.exec(QStringLiteral("SELECT e.id FROM events e LEFT JOIN tasks t ON t.id=e.id WHERE e.task_migrated<>1 OR t.id IS NULL LIMIT 1")) || q.next())return fail();q.finish();
    if(!q.exec(QStringLiteral("PRAGMA foreign_key_check")) || q.next())return fail();q.finish();
    if(!q.exec(QStringLiteral("SELECT t.id FROM tasks t JOIN headings h ON h.id=t.heading_id WHERE h.project_id<>t.project_id LIMIT 1")) || q.next())return fail();q.finish();
    if(!q.exec(QStringLiteral("SELECT id FROM recurrence_templates WHERE (frequency='WEEKLY' AND weekdays_mask=0) OR (frequency='AFTER_DAYS' AND mode<>'AFTER_COMPLETION') OR (frequency<>'AFTER_DAYS' AND mode<>'FIXED') LIMIT 1")) || q.next())return fail();q.finish();
    if(!db.commit()){db.rollback();return ioError<ImportOutcome>();}return {result,{}};
}
Result<ImportOutcome> DataTransfer::apply(const ImportPlan& plan,bool replace)
{
    QTemporaryDir temporary;if(!temporary.isValid())return ioError<ImportOutcome>();
    SqliteDatabase validation;const auto opened=validation.open(temporary.filePath(QStringLiteral("validation.sqlite")));if(!opened.ok())return {{},opened.error};
    const auto valid=insert(validation.connection(),plan.document,false,true);if(!valid.ok())return valid;
    if(replace) {
        const auto backup=m_database.createSnapshot(QFileInfo(m_database.connection().databaseName()).dir().filePath(QStringLiteral("backups")));
        if(!backup.ok())return {{},backup.error};
    }
    const auto converted=document(validation.connection());if(!converted.ok())return {{},converted.error};auto result=insert(m_database.connection(),converted.value,replace);
    // Adding data keeps local preferences; replacement imports only the portable subset.
    if(result.ok() && replace && plan.document.contains(QStringLiteral("settings"))) {
        const auto portable=plan.document.value(QStringLiteral("settings")).toObject();
        QSettings settings(QFileInfo(m_database.connection().databaseName()).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
        settings.setValue(QStringLiteral("calendar/sundayFirst"),portable.value(QStringLiteral("sundayFirst")).toBool());
        settings.setValue(QStringLiteral("appearance/themeMode"),portable.value(QStringLiteral("themeMode")).toInt());
        settings.setValue(QStringLiteral("appearance/textScale"),portable.value(QStringLiteral("textScale")).toDouble());settings.sync();
        result.value.settingsApplied=settings.status()==QSettings::NoError;
    }
    return result;
}
}
