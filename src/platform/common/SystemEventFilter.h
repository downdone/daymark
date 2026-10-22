#pragma once
#include <QAbstractNativeEventFilter>
#include <functional>

namespace daymark {
class SystemEventFilter final:public QAbstractNativeEventFilter {
public:
    explicit SystemEventFilter(std::function<void()> refresh);
    ~SystemEventFilter()override;
    bool nativeEventFilter(const QByteArray& eventType,void* message,qintptr* result)override;
private:
    std::function<void()> m_refresh;
};
}
