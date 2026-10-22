#pragma once
#include <QObject>
#include <QSettings>
#include <QtQml/qqmlregistration.h>
#include <memory>

namespace daymark {
class StartupController:public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppRuntime")
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
public:
    explicit StartupController(const QString& isolatedSettings={},QObject* parent=nullptr);
    bool supported()const;
    bool enabled()const;
    QString errorText()const{return m_error;}
    Q_INVOKABLE bool setEnabled(bool value);
signals:
    void changed();
private:
    std::unique_ptr<QSettings> m_settings;
    QString m_error;
    bool m_enabled=false;
};
}
