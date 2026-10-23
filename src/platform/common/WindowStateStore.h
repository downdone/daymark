#pragma once
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QWindow>
namespace daymark {
class WindowStateStore final:public QObject {
    Q_OBJECT
public:
    WindowStateStore(QWindow* window,const QString& settingsPath,QObject* parent=nullptr,const QString& key=QStringLiteral("main"),const QSize& minimum=QSize(880,580));
    void save();
    void normalize();
signals:
    void saveFailed();
private:
    QPointer<QWindow> m_window;
    QSettings m_settings;
    QTimer m_timer;
    QRect m_normal;
    QString m_key;
    QSize m_minimum;
};
}
