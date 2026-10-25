#include "SingleInstance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QLocalSocket>
#include <QThread>
#include <QTimer>

namespace daymark {

SingleInstance::Outcome SingleInstance::acquire(const QString& dataDirectory)
{
    if(!QDir().mkpath(dataDirectory))return Outcome::Failed;
    const auto canonical=QDir(dataDirectory).canonicalPath();
    auto digest=QCryptographicHash::hash(canonical.toUtf8(),QCryptographicHash::Sha256).toHex();
#ifdef Q_OS_MACOS
    // macOS sun_path holds 104 bytes including NUL. Its per-user temporary
    // directory plus a full hex SHA-256 exceeds that limit; retain 128 bits.
    digest.truncate(32);
#endif
    const auto name=QStringLiteral("daymark-")+QString::fromLatin1(digest);
    m_lock=std::make_unique<QLockFile>(QDir(canonical).filePath(QStringLiteral("daymark.lock")));
    m_lock->setStaleLockTime(0);
    if(!m_lock->tryLock()) {
        for(int attempt=0;attempt<10;++attempt) {
            QLocalSocket socket;socket.connectToServer(name);
            if(socket.waitForConnected(100)) {
                socket.write("activate\n");
                if((socket.bytesToWrite()==0 || socket.waitForBytesWritten(200)) && (socket.bytesAvailable()>0 || socket.waitForReadyRead(300)) && socket.readAll()==QByteArrayLiteral("ok\n"))
                    return Outcome::ActivatedExisting;
            }
            QThread::msleep(50);
        }
        return Outcome::Failed;
    }
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(name); // Only the lock owner may remove a stale endpoint.
    connect(&m_server,&QLocalServer::newConnection,this,[this]{
        while(auto* socket=m_server.nextPendingConnection()) {
            connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
            const auto readRequest=[this,socket]{
                if(socket->bytesAvailable()>64) {socket->disconnectFromServer();return;}
                if(!socket->canReadLine())return;
                if(socket->readLine()==QByteArrayLiteral("activate\n")) {
                    emit activationRequested();socket->write("ok\n");socket->flush();
                }
                socket->disconnectFromServer();
            };
            connect(socket,&QLocalSocket::readyRead,this,readRequest);
            if(socket->bytesAvailable()>0)readRequest();
            QTimer::singleShot(2000,socket,[socket]{socket->disconnectFromServer();});
        }
    });
    if(!m_server.listen(name)) {m_lock->unlock();return Outcome::Failed;}
    return Outcome::Primary;
}

} // namespace daymark
