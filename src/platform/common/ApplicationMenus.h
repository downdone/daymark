#pragma once
#include <QObject>
#include <memory>
#include <functional>
class QMenuBar;
namespace daymark {
class ApplicationMenus final {
public:
    explicit ApplicationMenus(std::function<void(const QString&)> command);
    ~ApplicationMenus();
private:
    std::unique_ptr<QMenuBar> m_bar;
};
}
