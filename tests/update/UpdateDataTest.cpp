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
            QVERIFY(query.exec("PRAGMA user_version=11"));
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
