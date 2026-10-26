#pragma once
#include "models/TaskWorkspace.h"
#include <QDeadlineTimer>
#include <QSignalSpy>

namespace daymark::test {
// Wait on the same notification the UI observes. Preserve the previous 5s
// failure deadline without a 50ms polling floor for every worker command.
inline bool awaitIdle(TaskWorkspace& workspace,bool requireReady=false,int timeoutMs=5000)
{
    const auto settled=[&]{return !workspace.busy() && (!requireReady || workspace.ready());};
    if(settled())return true;
    QSignalSpy changes(&workspace,&TaskWorkspace::stateChanged);
    if(!changes.isValid())return false;
    QDeadlineTimer deadline(timeoutMs);
    while(!settled()) {
        if(deadline.hasExpired() || !changes.wait(static_cast<int>(deadline.remainingTime())))return settled();
    }
    return true;
}
}
