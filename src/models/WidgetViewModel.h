#pragma once

#include "platform/IWindowPlatform.h"
#include "WidgetPresentation.h"
#include "WidgetPlacementPolicy.h"

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
    Q_PROPERTY(bool miniAutoCollapse READ miniAutoCollapse NOTIFY stateChanged)
    Q_PROPERTY(bool miniCollapsed READ miniCollapsed NOTIFY stateChanged)
    Q_PROPERTY(bool geometryChanging READ geometryChanging NOTIFY stateChanged)
    Q_PROPERTY(int miniWidth READ miniWidth NOTIFY stateChanged)
    Q_PROPERTY(int selectedForm READ selectedForm NOTIFY stateChanged)
    Q_PROPERTY(int effectiveForm READ effectiveForm NOTIFY stateChanged)
    Q_PROPERTY(int enabledForms READ enabledForms NOTIFY stateChanged)
    Q_PROPERTY(int formCount READ formCount NOTIFY stateChanged)
    Q_PROPERTY(bool canCompact READ canCompact NOTIFY stateChanged)
    Q_PROPERTY(int layer READ layer NOTIFY stateChanged)
    Q_PROPERTY(bool growLeft READ growLeft NOTIFY stateChanged)
    Q_PROPERTY(bool growUp READ growUp NOTIFY stateChanged)
    Q_PROPERTY(int transitionId READ transitionId NOTIFY stateChanged)
    Q_PROPERTY(double revealFrom READ revealFrom NOTIFY stateChanged)
    Q_PROPERTY(bool targetBall READ targetBall NOTIFY stateChanged)
    Q_PROPERTY(bool panelMotion READ panelMotion NOTIFY stateChanged)
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
    bool mini() const { return m_renderForm != WidgetPresentation::Panel; }
    bool miniAutoCollapse() const { return m_autoCompact; }
    bool miniCollapsed() const { return m_renderForm == WidgetPresentation::Ball; }
    bool geometryChanging() const {return m_geometryChanging;}
    int miniWidth() const {return m_miniSize.width();}
    int selectedForm() const {return m_presentation.selected;}
    int effectiveForm() const {return m_presentation.effective;}
    int enabledForms() const {return m_presentation.enabled;}
    int formCount() const {return m_presentation.count();}
    bool canCompact() const {return m_presentation.allows(1) || m_presentation.allows(2);}
    int layer() const {return desktopPinned() ? 2 : alwaysOnTop() ? 1 : 0;}
    bool growLeft() const {return m_anchor.right;}
    bool growUp() const {return m_anchor.bottom;}
    int transitionId() const {return m_transitionId;}
    double revealFrom() const {return m_revealFrom;}
    bool targetBall() const {return m_presentation.effective == WidgetPresentation::Ball;}
    bool panelMotion() const {return m_panelMotion;}
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
    bool initialVisible() const {return m_requestedVisible && m_presentation.enabled;}

    Q_INVOKABLE bool selectForm(int form);
    Q_INVOKABLE bool formEnabled(int form) const {return m_presentation.allows(form);}
    Q_INVOKABLE bool setFormEnabled(int form, bool enabled);
    Q_INVOKABLE void activatePrimary();
    Q_INVOKABLE void returnToCompact();
    Q_INVOKABLE bool setLayer(int layer);
    Q_INVOKABLE void setInteraction(bool hovered, bool pressed, bool menu, bool keyboard);
    Q_INVOKABLE void enableVisualTransitions();
    Q_INVOKABLE void finishTransition(int id);
    Q_INVOKABLE void setRevealRegion(const QRect& rectangle, double radius = 22);
    Q_INVOKABLE void finishMove();
    Q_INVOKABLE bool setMini(bool mini);
    Q_INVOKABLE bool setMiniAutoCollapse(bool enabled);
    Q_INVOKABLE bool setAutoCompact(bool enabled);
    Q_INVOKABLE bool setMiniCollapsed(bool collapsed, int diameter = 36);
    Q_INVOKABLE bool setMiniLayout(int barHeight, int diameter = 36);
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
    void openMainRequested();
    void configurationRequested();
    void stateChanged();
    void errorChanged();

private:
    bool accept(WindowResult result);
    bool applyPresentation(const WidgetPresentation& candidate, bool animate = true);
    void scheduleAutomatic();
    void previewForm(int form);
    void captureUserGeometry();
    void watchGeometry();
    QList<WidgetScreen> screens() const;
    QSize formSize(int form) const;
    IWindowPlatform& m_platform;
    QPointer<QWindow> m_window;
    QSize m_floatingSize{340, 190};
    WidgetPresentation m_presentation;
    WidgetPresentation m_collapseRollback;
    WidgetPresentation::Form m_renderForm = WidgetPresentation::Panel;
    WidgetAnchor m_anchor;
    QRect m_appliedGeometry;
    bool m_anchorValid=false,m_requestedVisible=true,m_autoCompact=false;
    bool m_hovered=false,m_pressed=false,m_menu=false,m_keyboard=false,m_moving=false;
    bool m_visualTransitions=false,m_pendingCollapse=false,m_panelMotion=false;
    int m_transitionId=0;
    double m_revealFrom=1;
    QSize m_miniSize{340,48};
    int m_miniHeight=48,m_ballDiameter=36;
    bool m_geometryChanging=false;
    QTimer m_hoverTimer,m_idleTimer,m_returnTimer,m_moveTimer;
    WindowError m_error = WindowError::None;
    bool m_trayAvailable=false,m_locked=false,m_closeToTray=true;
    double m_backgroundOpacity=0.9;
    std::unique_ptr<QSettings> m_settings;
    QTimer m_saveTimer;
};

} // namespace daymark
