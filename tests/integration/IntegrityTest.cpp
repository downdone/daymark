#include "persistence/SqliteDatabase.h"
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>
using namespace daymark;
class IntegrityTest:public QObject {
    Q_OBJECT
private slots:
    void reportsForeignKeyViolationWithoutChangingRows() {
        QTemporaryDir dir;SqliteDatabase database;
        QVERIFY(database.open(dir.filePath(QStringLiteral("integrity.sqlite"))).ok());
        QVERIFY(database.checkIntegrity().ok());
        QSqlQuery query(database.connection());
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=OFF")));
        QVERIFY(query.exec(QStringLiteral("INSERT INTO task_tags(task_id,tag_id) VALUES('missing-task','missing-tag')")));
        QVERIFY(query.exec(QStringLiteral("PRAGMA foreign_keys=ON")));
        const auto result=database.checkIntegrity();QVERIFY(!result.ok());QCOMPARE(result.error.code,ErrorCode::PersistenceError);
        QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM task_tags")));QVERIFY(query.next());QCOMPARE(query.value(0).toInt(),1);
        QVERIFY(query.exec(QStringLiteral("DELETE FROM task_tags")));QVERIFY(database.checkIntegrity().ok());
    }
};
QTEST_GUILESS_MAIN(IntegrityTest)
#include "IntegrityTest.moc"
