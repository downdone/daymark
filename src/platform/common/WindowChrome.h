#pragma once
#include <QObject>
#include <QPointer>
#include <QQmlParserStatus>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml/qqmlregistration.h>

namespace daymark {
class WindowChrome;
class WindowChromeController final : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppRuntime")
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(int mode READ mode NOTIFY changed)
    Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
  public:
    explicit WindowChromeController(const QString &settingsPath,
                                    bool writable = true,
                                    QObject *parent = nullptr);
    bool supported() const;
    int mode() const { return m_mode; }
    QString errorText() const { return m_error; }
    QString errorKey() const { return m_key; }
    void setWritable(bool value) { m_writable = value; }
    Q_INVOKABLE bool requestMode(int mode);
  signals:
    void changed();

  private:
    friend class WindowChrome;
    void add(WindowChrome *binding);
    void remove(WindowChrome *binding);
    void fail(const QString &key, const QString &message,
              quint32 nativeCode = 0);
    QString m_path, m_error, m_key;
    QList<WindowChrome *> m_bindings;
    int m_mode = 0;
    bool m_writable;
};

class WindowChrome : public QObject, public QQmlParserStatus {
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(
        QQuickWindow *window READ window WRITE setWindow NOTIFY targetChanged)
    Q_PROPERTY(daymark::WindowChromeController *controller READ controller WRITE
                   setController NOTIFY targetChanged)
    Q_PROPERTY(QQuickItem *titleBar MEMBER m_titleBar)
    Q_PROPERTY(QQuickItem *buttonStrip MEMBER m_buttons)
    Q_PROPERTY(QQuickItem *maximizeButton MEMBER m_maximizeButton)
    Q_PROPERTY(QVariantList interactiveItems READ interactiveItems WRITE setInteractiveItems NOTIFY interactiveItemsChanged)
    Q_PROPERTY(bool customEnabled READ customEnabled NOTIFY customChanged)
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY appearanceChanged)
    Q_PROPERTY(bool nativeMaximizeHovered READ nativeMaximizeHovered NOTIFY
                   interactionChanged)
    Q_PROPERTY(bool nativeMaximizePressed READ nativeMaximizePressed NOTIFY
                   interactionChanged)
  public:
    explicit WindowChrome(QObject *parent = nullptr);
    ~WindowChrome() override;
    void classBegin() override {}
    void componentComplete() override;
    QQuickWindow *window() const { return m_window; }
    WindowChromeController *controller() const { return m_controller; }
    void setWindow(QQuickWindow *window);
    void setController(WindowChromeController *controller);
    bool customEnabled() const { return m_custom; }
    bool dark() const { return m_dark; }
    void setDark(bool dark);
    QVariantList interactiveItems() const;
    void setInteractiveItems(const QVariantList &items);
    bool nativeMaximizeHovered() const { return m_hovered; }
    bool nativeMaximizePressed() const { return m_pressed; }
    // Win32-compatible results; coordinates are client DIP, never screen
    // pixels.
    int hitTest(const QPointF &local, qreal border) const;
    quint32 nativeError() const { return m_nativeError; }
    virtual bool applyMode(bool custom);
    Q_INVOKABLE void toggleMaximize();
    Q_INVOKABLE void showSystemMenu();
    bool handleNativeMessage(const QByteArray &type, void *message,
                             qintptr *result);
  signals:
    void targetChanged();
    void customChanged();
    void appearanceChanged();
    void interactionChanged();
    void interactiveItemsChanged();

  protected:
    bool eventFilter(QObject *object, QEvent *event) override;

  private:
    void attach();
    bool configureNative();
    void detachNative();
    QRect nativeFrameGeometry() const;
    QByteArray nativePlacement() const;
    bool restoreNativePlacement(const QByteArray &placement, bool visible);
    bool restoreNativeFrameGeometry(const QRect &geometry);
    void setInteraction(bool hovered, bool pressed);
    bool contains(QQuickItem *item, const QPointF &point) const;
    void popupSystemMenu(const QPoint &screenPoint);
    QPointer<QQuickWindow> m_window;
    QPointer<WindowChromeController> m_controller;
    QPointer<QQuickItem> m_titleBar, m_buttons, m_maximizeButton;
    QList<QPointer<QQuickItem>> m_interactiveItems;
    quintptr m_nativeId = 0;
    quintptr m_originalFrameStyle = 0;
    quint32 m_nativeError = 0;
    bool m_complete = false, m_registered = false, m_custom = false,
         m_dark = false;
    bool m_hovered = false, m_pressed = false;
};
} // namespace daymark
