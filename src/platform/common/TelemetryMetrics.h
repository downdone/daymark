#pragma once
#include <QtGlobal>
#include <optional>
#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif
namespace daymark {
// Sparse sampling only. This is PrivateUsage (private committed bytes), not
// working set, and is deliberately bucketed before entering the telemetry SDK.
inline std::optional<qint64> telemetryPrivateCommittedBytes(){
#if defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
    if(K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counters),sizeof(counters)))return qint64(counters.PrivateUsage);
#endif
    return std::nullopt;
}
}
