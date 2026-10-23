#pragma once
#include <QAbstractNativeEventFilter>
#include <functional>
#include <QObject>
#include <QTimer>

namespace daymark {
class SystemEventFilter final:public QObject,public QAbstractNativeEventFilter {
public:
    explicit SystemEventFilter(std::function<void()> refresh);
    ~SystemEventFilter()override;
    bool nativeEventFilter(const QByteArray& eventType,void* message,qintptr* result)override;
    void requestRefresh();
private:
    std::function<void()> m_refresh;
    QTimer m_pending;
};
}
