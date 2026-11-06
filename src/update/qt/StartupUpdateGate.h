#pragma once
#include <QJsonObject>
#include <QString>
#include <memory>

namespace daymark {
// Windows implementation is compiled into the Qt host, never into the native SDK.
class StartupUpdateGate final {
  public:
    StartupUpdateGate();
    ~StartupUpdateGate();
    bool acquire(const QString &handoff, const QString &applicationVersion);
    bool restricted() const;
    bool commitReady();
    bool running();
    QString error() const;
    QString root() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
} // namespace daymark
