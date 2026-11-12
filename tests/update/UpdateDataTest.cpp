#include "persistence/SqliteDatabase.h"
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
using namespace daymark;
class UpdateDataTest final : public QObject {
    Q_OBJECT
  private slots:
    void schema10MigrationBacksUpBeforeAddingRemovalState() {
        QTemporaryDir dir;const auto path=dir.filePath("old.sqlite");
        {SqliteDatabase old;QVERIFY(old.open(path).ok());QSqlQuery q(old.connection());
         QVERIFY(q.exec("INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) VALUES('retained','synthetic','2026-11-10T00:00:00.000Z','2026-11-10T00:00:00.000Z')"));
         QVERIFY(q.exec("ALTER TABLE tasks DROP COLUMN purged_at_utc"));QVERIFY(q.exec("ALTER TABLE tasks DROP COLUMN local_removed_at_utc"));QVERIFY(q.exec("PRAGMA user_version=10"));}
        QVERIFY(SqliteDatabase::probeForUpdate(path).ok());
        SqliteDatabase upgraded;QVERIFY(upgraded.open(path).ok());QSqlQuery q(upgraded.connection());
        QVERIFY(q.exec("PRAGMA user_version"));QVERIFY(q.next());QCOMPARE(q.value(0).toInt(),11);q.finish();
        QVERIFY(q.exec("SELECT title,local_removed_at_utc,purged_at_utc FROM tasks WHERE id='retained'"));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("synthetic"));QVERIFY(q.value(1).isNull());QVERIFY(q.value(2).isNull());
        const auto backups=QDir(dir.filePath("backups/migrations")).entryList({"*.sqlite"},QDir::Files);QCOMPARE(backups.size(),1);
        QVERIFY(upgraded.checkIntegrity().ok());
    }
    void readOnlyProbeNeverMigrates() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("data.sqlite");
        {
            SqliteDatabase db;
            QVERIFY(db.open(path).ok());
            QSqlQuery query(db.connection());
            QVERIFY(query.exec(
                "INSERT INTO tasks(id,title,created_at_utc,updated_at_utc) "
                "VALUES('update-test','synthetic','2026-11-05T00:00:00Z','2026-11-05T00:00:00Z')"));
            QVERIFY(query.exec("PRAGMA wal_checkpoint(TRUNCATE)"));
        }
        QFile before(path);
        QVERIFY(before.open(QIODevice::ReadOnly));
        const auto bytes = before.readAll();
        before.close();
        const auto backups =
            QDir(dir.filePath("backups")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QVERIFY(SqliteDatabase::probeForUpdate(path).ok());
        QFile after(path);
        QVERIFY(after.open(QIODevice::ReadOnly));
        QCOMPARE(after.readAll(), bytes);
        after.close();
        QCOMPARE(QDir(dir.filePath("backups")).entryList(QDir::Dirs | QDir::NoDotAndDotDot),
                 backups);
        const QString connection = "update-probe-future";
        {
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(path);
            QVERIFY(db.open());
            QSqlQuery query(db);
            QVERIFY(query.exec("PRAGMA user_version=12"));
            db.close();
        }
        QSqlDatabase::removeDatabase(connection);
        auto result = SqliteDatabase::probeForUpdate(path);
        QVERIFY(!result.ok());
        QCOMPARE(result.error.key, QStringLiteral("update.data_compatibility"));
        const auto absent = dir.filePath("absent.sqlite");
        QVERIFY(SqliteDatabase::probeForUpdate(absent).ok());
        QVERIFY(!QFile::exists(absent));
    }
};
QTEST_GUILESS_MAIN(UpdateDataTest)
#include "UpdateDataTest.moc"
