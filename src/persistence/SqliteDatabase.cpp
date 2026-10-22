#include "SqliteDatabase.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QResource>
#include <QSqlQuery>
#include <QUuid>

static void initializeSchemaResource() { Q_INIT_RESOURCE(daymark_schema); }

namespace daymark {

Result<void> MigrationManager::initialize(QSqlDatabase& db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
        return Result<void>::failure(ErrorCode::PersistenceError, QStringLiteral("databaseRead"));
    const int version = query.value(0).toInt();
    query.finish();
    if (version > 1)
        return Result<void>::failure(ErrorCode::ValidationError, QStringLiteral("schemaTooNew"));
    if (version == 1)
        return {};
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
    return {};
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
    if(!query.exec(QStringLiteral("SELECT (SELECT count(*) FROM tasks)+(SELECT count(*) FROM projects)+(SELECT count(*) FROM areas)+(SELECT count(*) FROM recurrence_templates)+(SELECT count(*) FROM tags)")) || !query.next())
        return Result<void>::failure(ErrorCode::PersistenceError,QStringLiteral("databaseRead"));
    const bool empty=query.value(0).toLongLong()==0;query.finish();if(empty)return {};
    QDir directory(QFileInfo(m_db.databaseName()).dir().filePath(QStringLiteral("backups/daily")));
    const auto prefix=QStringLiteral("daily-%1-").arg(QDate::currentDate().toString(Qt::ISODate));
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
    const auto name = QStringLiteral("daymark-v1-%1-%2.sqlite")
        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")),
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
