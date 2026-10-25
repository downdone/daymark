#pragma once
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
inline void initializeLegacySchemaResources(){Q_INIT_RESOURCE(daymark_schema);}
// Real historical schemas, not a current DB with a forged user_version.
inline bool createLegacyDatabase(const QString& path,int version)
{
    initializeLegacySchemaResources();
    const auto name=QUuid::createUuid().toString(QUuid::WithoutBraces);bool ok=true;
    {auto db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);db.setDatabaseName(path);ok=db.open();QSqlQuery q(db);
        const QStringList files={"001_init.sql","002_area_archive.sql","003_calendar_events.sql","004_event_series.sql","005_categories.sql","006_task_relations.sql","007_recurring_reminders.sql","008_unified_tasks.sql","009_task_tree_actions.sql"};
        ok=ok && version>=1 && version<=files.size();
        for(int i=0;ok && i<version;++i){QFile file(QStringLiteral(":/daymark/src/persistence/migrations/")+files[i]);ok=file.open(QIODevice::ReadOnly);if(ok)for(const auto& statement:QString::fromUtf8(file.readAll()).split(';',Qt::SkipEmptyParts))if(!statement.trimmed().isEmpty() && !q.exec(statement)){ok=false;break;}}
        if(ok)ok=q.exec(QStringLiteral("PRAGMA user_version=%1").arg(version));q.finish();db.close();}
    QSqlDatabase::removeDatabase(name);return ok;
}
