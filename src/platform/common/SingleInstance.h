#pragma once
#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <memory>

namespace daymark {

class SingleInstance final : public QObject {
    Q_OBJECT
public:
    enum class Outcome { Primary, ActivatedExisting, Failed };
    using QObject::QObject;
    Outcome acquire(const QString& dataDirectory);
signals:
    void activationRequested();
private:
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
};

} // namespace daymark
