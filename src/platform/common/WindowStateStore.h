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
    WindowStateStore(QWindow* window,const QString& settingsPath,QObject* parent=nullptr);
    void save();
    void normalize();
signals:
    void saveFailed();
private:
    QPointer<QWindow> m_window;
    QSettings m_settings;
    QTimer m_timer;
    QRect m_normal;
};
}
