#pragma once

#include "platform/IWindowPlatform.h"

#include <QObject>
#include <QPointer>
#include <QWindow>
#include <QtQml/qqmlregistration.h>
#include <QSettings>
#include <QTimer>
#include <memory>

namespace daymark {

class WidgetViewModel final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppRuntime")
    Q_PROPERTY(bool ready READ ready NOTIFY stateChanged)
    Q_PROPERTY(bool mini READ mini NOTIFY stateChanged)
    Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop NOTIFY stateChanged)
    Q_PROPERTY(bool desktopPinned READ desktopPinned NOTIFY stateChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)
    Q_PROPERTY(int errorCode READ errorCode NOTIFY errorChanged)
    Q_PROPERTY(bool clickThrough READ clickThrough NOTIFY stateChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY stateChanged)
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    Q_PROPERTY(double backgroundOpacity READ backgroundOpacity NOTIFY stateChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray NOTIFY stateChanged)

public:
    explicit WidgetViewModel(IWindowPlatform& platform, QObject* parent = nullptr);
    bool attachWindow(QWindow* window);
    bool ready() const { return !m_window.isNull(); }
    bool mini() const { return m_mini; }
    bool alwaysOnTop() const { return m_window && m_window->flags().testFlag(Qt::WindowStaysOnTopHint); }
    bool desktopPinned() const {return m_window && m_window->flags().testFlag(Qt::WindowStaysOnBottomHint);}
    bool visible() const { return m_window && m_window->isVisible(); }
    QString errorText() const;
    int errorCode() const { return static_cast<int>(m_error); }
    bool clickThrough() const {return m_window && m_window->flags().testFlag(Qt::WindowTransparentForInput);}
    bool trayAvailable() const {return m_trayAvailable;}
    bool locked() const {return m_locked;}
    double backgroundOpacity() const {return m_backgroundOpacity;}
    bool closeToTray() const {return m_closeToTray;}
    void setTrayAvailable(bool available);
    void configureSettings(const QString& path);
    void saveSettings();
    bool initialVisible() const {return !m_settings || m_settings->value(QStringLiteral("widget/visible"),true).toBool();}

    Q_INVOKABLE bool setMini(bool mini);
    Q_INVOKABLE bool setAlwaysOnTop(bool enabled);
    Q_INVOKABLE bool setDesktopPinned(bool enabled);
    Q_INVOKABLE bool setVisible(bool visible);
    Q_INVOKABLE bool startMove();
    Q_INVOKABLE bool startResize();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE bool setClickThrough(bool enabled);
    Q_INVOKABLE void setLocked(bool locked);
    Q_INVOKABLE void setBackgroundOpacity(double opacity);
    Q_INVOKABLE void setCloseToTray(bool enabled);
    Q_INVOKABLE void resetPosition();
    Q_INVOKABLE void normalizeGeometry();

signals:
    void stateChanged();
    void errorChanged();

private:
    bool accept(WindowResult result);
    IWindowPlatform& m_platform;
    QPointer<QWindow> m_window;
    QSize m_floatingSize{340, 190};
    bool m_mini = false;
    WindowError m_error = WindowError::None;
    bool m_trayAvailable=false,m_locked=false,m_closeToTray=true;
    double m_backgroundOpacity=0.9;
    std::unique_ptr<QSettings> m_settings;
    QTimer m_saveTimer;
};

} // namespace daymark
