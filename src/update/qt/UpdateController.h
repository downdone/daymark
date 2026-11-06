#pragma once
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QProcess>
#include <QUrl>

namespace daymark {
class UpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool automatic READ automatic WRITE setAutomatic NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QUrl releaseNotesUrl READ releaseNotesUrl NOTIFY changed)
  public:
    explicit UpdateController(QString dataDirectory, bool verification, QObject *parent = nullptr);
    ~UpdateController() override;
    bool supported() const { return !m_root.isEmpty(); }
    bool ready() const { return m_state.value("ready").toBool(); }
    bool busy() const { return m_worker.state() != QProcess::NotRunning; }
    bool automatic() const { return m_state.value("auto_update").toBool(true); }
    QString statusText() const;
    QString availableVersion() const { return m_state.value("version").toString(); }
    double progress() const;
    Q_INVOKABLE void check(bool automatic = false);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void cancelReady();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void armRestart() { m_restart = true; }
    Q_INVOKABLE void cancelRestart() { m_restart = false; }
    Q_INVOKABLE void showUpdater();
    Q_INVOKABLE bool openReleaseNotes();
    QUrl releaseNotesUrl() const;
    void setAutomatic(bool);
    void runtimeReady();
  signals:
    void changed();
    void restartRequested();
    void updateReady();

  private:
    void reload();
    void command(const QStringList &);
    void localFailure(const QString &key);
    QString m_root, m_data, m_localError, m_notifiedCandidate;
    QJsonObject m_state;
    QProcess m_worker;
    QFileSystemWatcher m_watch;
    QLocalServer m_control;
    bool m_verification = false, m_restart = false, m_restartQueued = false, m_started = false;
};
} // namespace daymark
