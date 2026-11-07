#include "ProcessResources.h"
#ifdef Q_OS_WIN
// clang-format off
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif
namespace daymark {
QJsonObject readProcessResources() {
  QJsonObject result;
#ifdef Q_OS_WIN
  struct Extended {
    PROCESS_MEMORY_COUNTERS_EX base;
    SIZE_T privateWorkingSet;
    ULONG64 sharedCommit;
  };
  Extended value{};
  value.base.cb = sizeof(value);
  using Read = BOOL(WINAPI *)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
  auto read = reinterpret_cast<Read>(GetProcAddress(
      GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo"));
  const bool ex2 =
      read &&
      read(GetCurrentProcess(),
           reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&value), sizeof(value));
  bool valid = ex2;
  if (!valid && read) {
    value.base.cb = sizeof(value.base);
    valid = read(GetCurrentProcess(),
                 reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&value),
                 sizeof(value.base));
  }
  result["available"] = valid;
  if (valid) {
    result["privateCommitBytes"] = qint64(value.base.PrivateUsage);
    result["workingSetBytes"] = qint64(value.base.WorkingSetSize);
    result["osPeakPrivateBytes"] = qint64(value.base.PeakPagefileUsage);
    result["privateWorkingSetBytes"] =
        ex2 ? QJsonValue(qint64(value.privateWorkingSet)) : QJsonValue();
  }
  DWORD handles = 0;
  if (GetProcessHandleCount(GetCurrentProcess(), &handles))
    result["handles"] = int(handles);
  quint64 privateCommitted = 0, mappedCommitted = 0, imageCommitted = 0;
  MEMORY_BASIC_INFORMATION region{};
  quintptr address = 0;
  while (VirtualQuery(reinterpret_cast<void *>(address), &region,
                      sizeof(region)) == sizeof(region)) {
    if (region.State == MEM_COMMIT) {
      if (region.Type == MEM_PRIVATE)
        privateCommitted += region.RegionSize;
      else if (region.Type == MEM_MAPPED)
        mappedCommitted += region.RegionSize;
      else if (region.Type == MEM_IMAGE)
        imageCommitted += region.RegionSize;
    }
    const auto next =
        reinterpret_cast<quintptr>(region.BaseAddress) + region.RegionSize;
    if (next <= address)
      break;
    address = next;
  }
  result["virtualPrivateCommittedBytes"] = qint64(privateCommitted);
  result["virtualMappedCommittedBytes"] = qint64(mappedCommitted);
  result["virtualImageCommittedBytes"] = qint64(imageCommitted);
#else
  result["available"] = false;
#endif
  return result;
}
} // namespace daymark
