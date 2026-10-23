#include "SqliteDatabase.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QSqlQuery>
#include <QUuid>
#include <QSettings>

static void initializeSchemaResource() { Q_INIT_RESOURCE(daymark_schema); }

namespace daymark {
Result<void> SqliteDatabase::checkIntegrity() const
{
    QSqlQuery check(m_db);
    if(!check.exec(QStringLiteral("PRAGMA quick_check")) || !check.next() || check.value(0).toString()!=QStringLiteral("ok") || check.next())
        return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("integrityFailed"));
    if(!check.exec(QStringLiteral("PRAGMA foreign_key_check")) || check.next())
        return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("integrityFailed"));
    return {};
}

Result<void> MigrationManager::initialize(QSqlDatabase& db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseRead"));
    const int version = query.value(0).toInt();
    query.finish();
    if (version > 3)
        return Result<void>::failure(ErrorCode::ValidationError, QStringLiteral("schemaTooNew"));
    if (version == 3)
        return {};
    if(version==2) {
        initializeSchemaResource();
        QFile schema(QStringLiteral(":/daymark/src/persistence/migrations/003_calendar_events.sql"));
        if(!schema.open(QIODevice::ReadOnly))return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("schemaResource"));
        if(!db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
        for(const auto& statement:QString::fromUtf8(schema.readAll()).split(';',Qt::SkipEmptyParts)) {
            if(!statement.trimmed().isEmpty() && !query.exec(statement)){db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("migrationFailed"));}
        }
        if(!query.exec(QStringLiteral("PRAGMA user_version=3")) || !db.commit()){db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("migrationFailed"));}
        return {};
    }
    if(version==1) {
        initializeSchemaResource();
        QFile schema(QStringLiteral(":/daymark/src/persistence/migrations/002_area_archive.sql"));
        if(!schema.open(QIODevice::ReadOnly))return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("schemaResource"));
        if(!db.transaction())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseWrite"));
        for(const auto& statement:QString::fromUtf8(schema.readAll()).split(';',Qt::SkipEmptyParts)) {
            if(!statement.trimmed().isEmpty() && !query.exec(statement)){db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("migrationFailed"));}
        }
        if(!query.exec(QStringLiteral("PRAGMA user_version=2")) || !db.commit()){db.rollback();return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("migrationFailed"));}
        return initialize(db);
    }
    if (version != 0)
        return Result<void>::failure(ErrorCode::ValidationError, QStringLiteral("unknownSchema"));
    if (!query.exec(QStringLiteral("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'")) || !query.next())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseRead"));
    if (query.value(0).toInt() != 0)
        return Result<void>::failure(ErrorCode::ValidationError, QStringLiteral("unknownSchema"));
    query.finish();
    initializeSchemaResource();
    QFile schema(QStringLiteral(":/daymark/src/persistence/migrations/001_init.sql"));
    if (!schema.open(QIODevice::ReadOnly))
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("schemaResource"));
    if (!db.transaction())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseWrite"));
    const auto statements = QString::fromUtf8(schema.readAll()).split(';', Qt::SkipEmptyParts);
    for (const auto& statement : statements) {
        if (!statement.trimmed().isEmpty() && !query.exec(statement)) {
            db.rollback();
            return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("migrationFailed"));
        }
    }
    if (!query.exec(QStringLiteral("PRAGMA user_version=1")) || !db.commit()) {
        db.rollback();
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("migrationFailed"));
    }
    return initialize(db);
}

SqliteDatabase::SqliteDatabase() : m_connectionName(QUuid::createUuid().toString(QUuid::WithoutBraces)) {}
SqliteDatabase::~SqliteDatabase()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = {};
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

Result<void> SqliteDatabase::open(const QString& path)
{
    if (!m_db.isValid())
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    if (m_db.isOpen())
        m_db.close();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return Result<void>::failure(ErrorCode::PermissionError, QStringLiteral("dataDirectory"));
    m_db.setDatabaseName(path);
    if (!m_db.open())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseOpen"));
    {
        QSqlQuery version(m_db);
        if(!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
        const int current=version.value(0).toInt();version.finish();
        if(current==1 || current==2) {
            const auto snapshot=createSnapshot(QFileInfo(path).dir().filePath(QStringLiteral("backups/migrations")));
            if(!snapshot.ok()){m_db.close();return {snapshot.error};}
        }
    }
    const auto migration = MigrationManager::initialize(m_db);
    if (!migration.ok()) { m_db.close(); return migration; }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("PRAGMA foreign_keys=ON"))
        || !query.exec(QStringLiteral("PRAGMA busy_timeout=5000"))
        || !query.exec(QStringLiteral("PRAGMA journal_mode=WAL")) || !query.next()
        || query.value(0).toString() != QStringLiteral("wal"))
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseConfigure"));
    query.finish();
    if (!query.exec(QStringLiteral("PRAGMA quick_check")) || !query.next()
        || query.value(0).toString() != QStringLiteral("ok"))
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseIntegrity"));
    query.finish();
    if (!query.exec(QStringLiteral("PRAGMA foreign_key_check")) || query.next())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseIntegrity"));
    query.finish();
    return ensureDailySnapshot();
}

Result<void> SqliteDatabase::ensureDailySnapshot()
{
    QSqlQuery query(m_db);
    if(!query.exec(QStringLiteral("SELECT (SELECT count(*) FROM tasks)+(SELECT count(*) FROM projects)+(SELECT count(*) FROM areas)+(SELECT count(*) FROM recurrence_templates)+(SELECT count(*) FROM tags)+(SELECT count(*) FROM events)")) || !query.next())
        return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    const bool empty=query.value(0).toLongLong()==0;query.finish();if(empty)return {};
    QDir directory(QFileInfo(m_db.databaseName()).dir().filePath(QStringLiteral("backups/daily")));
    const auto prefix=QStringLiteral("daily-%1-").arg(QDate::currentDate().toString(Qt::ISODate));
    QSettings settings(QFileInfo(m_db.databaseName()).dir().filePath(QStringLiteral("ui.ini")),QSettings::IniFormat);
    int days=settings.value(QStringLiteral("preferences/backupDays"),1).toInt();
    if(days!=1 && days!=7 && days!=30)days=1;
    for(const auto& name:directory.entryList({QStringLiteral("daily-*.sqlite")},QDir::Files)) {
        const auto date=QDate::fromString(name.mid(6,10),Qt::ISODate);
        if(date.isValid() && date.daysTo(QDate::currentDate())>=0 && date.daysTo(QDate::currentDate())<days)return {};
    }
    if(!directory.entryList({prefix+QStringLiteral("*.sqlite")},QDir::Files).isEmpty())return {};
    const auto snapshot=createSnapshot(directory.absolutePath());if(!snapshot.ok())return {snapshot.error};
    const auto renamed=directory.filePath(prefix+QFileInfo(snapshot.value).fileName());
    if(!QFile::rename(snapshot.value,renamed))return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("backupFailed"));
    const auto files=directory.entryList({QStringLiteral("daily-*.sqlite")},QDir::Files,QDir::Name);
    for(int i=0;i<files.size()-7;++i)if(!QFile::remove(directory.filePath(files[i])))
        return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("backupFailed"));
    return {};
}

Result<QString> SqliteDatabase::createSnapshot(const QString& directory)
{
    if (!m_db.isOpen() || !QDir().mkpath(directory))
        return Result<QString>::failure(ErrorCode::PersistenceError, QStringLiteral("backupFailed"));
    QSqlQuery version(m_db);
    if(!version.exec(QStringLiteral("PRAGMA user_version")) || !version.next())return Result<QString>::failure(ErrorCode::PersistenceError,QStringLiteral("backupFailed"));
    const int schemaVersion=version.value(0).toInt();version.finish();
    const auto name = QStringLiteral("daymark-v%1-%2-%3.sqlite")
        .arg(schemaVersion).arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")),
             QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto path = QDir(directory).absoluteFilePath(name);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("VACUUM INTO ?"));
    query.addBindValue(path);
    if (!query.exec())
        return Result<QString>::failure(ErrorCode::PersistenceError, QStringLiteral("backupFailed"));
    return {path, {}};
}

} // namespace daymark
