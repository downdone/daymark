#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

// Toolchain integration probe; not a test of the future Repository or MigrationManager.
class SqliteRuntimeTest : public QObject {
    Q_OBJECT
private slots:
    void rollbackAndReopen()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto connectionName = QStringLiteral("daymark-runtime-test");
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
            db.setDatabaseName(directory.filePath(QStringLiteral("runtime.sqlite")));
            QVERIFY2(db.open(), qPrintable(db.lastError().text()));
            {
                QSqlQuery query(db);
                QVERIFY(query.exec(QStringLiteral("PRAGMA journal_mode=WAL")));
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toString(), QStringLiteral("wal"));
                QVERIFY(query.exec(QStringLiteral("CREATE TABLE probe (id INTEGER PRIMARY KEY, title TEXT NOT NULL)")));
                QVERIFY(query.exec(QStringLiteral("INSERT INTO probe VALUES (1, '中文 task')")));
                QVERIFY(db.transaction());
                QVERIFY(query.exec(QStringLiteral("INSERT INTO probe VALUES (2, 'rollback')")));
                QVERIFY(db.rollback());
            }
            db.close();
            QVERIFY(db.open());
            {
                QSqlQuery query(db);
                QVERIFY(query.exec(QStringLiteral("SELECT title FROM probe ORDER BY id")));
                QVERIFY(query.next());
                QCOMPARE(query.value(0).toString(), QString::fromUtf8("中文 task"));
                QVERIFY(!query.next());
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName);
    }
};

QTEST_GUILESS_MAIN(SqliteRuntimeTest)
#include "SqliteRuntimeTest.moc"
