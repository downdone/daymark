# Windows连续稳定性验收

本工具运行一个持续存活的原生Daymark进程，使用QTemporaryDir合成数据库；不会打开现有任务库、注册开机启动、建立托盘或发送系统通知。先执行完整UI smoke，再循环完整任务编辑、完成/恢复、软删/撤销、备份、完整性检查、重载及窗口/视图切换。退出时清理临时库，只保留匿名验收报告。

## Windows

先按[环境说明](environment.md)设置DAYMARK_QT/DAYMARK_MINGW并构建Release：

```powershell
./scripts/build-windows.ps1 -Preset windows-release
./scripts/test-windows.ps1 -Preset windows-release
# 首次短跑只验证工具，不等于8小时门禁。
./scripts/test-stability-windows.ps1 -DurationSeconds 60 -SampleSeconds 2
# 默认8小时，5秒资源采样；前台仍会出现合成测试窗口。
./scripts/test-stability-windows.ps1
```

可用`-Executable`选择已解压的候选版daymark.exe。每次输出新build/stability-时间-随机后缀目录，保留runtime.json、resources.csv、summary.json和本机诊断日志。报告不记录标题/备注或数据库路径；诊断日志可能含SDK路径，留在忽略的build内，不直接上传。

- runtime.json：实际连续循环时长、UTC起止、循环数、SQLite quick_check/foreign_key_check次数、事件循环最大延迟和长暂停次数。写失败或业务/完整性错误非零退出。
- resources.csv：采样时刻、工作集、私有内存、CPU占用。cpuCorePercent以一个逻辑核占满为100%，cpuMachinePercent除以逻辑核数。包含初始化及活跃操作，不是纯闲置CPU基准；不以瞬时峰值直接宣称泄漏。
- summary.json：进程总墙钟时间（包含UI smoke）、资源峰值、是否完成请求时长。eightHourRunCompleted仅表示合成负载时长达到8h；manualSleepMultiScreenVerified始终false，必须另附人工证据。

进程异常退出、报告缺失、时长不足或总超时均失败。超时仅结束本工具创建的子进程。中止时保留资源CSV，未完成报告不能算PASS。不得手工改报告来补门禁。

## 真实睡眠、多屏与跨日

在独立测试账号/合成数据中，分别记录睡眠前后UTC、本地日期/时区、任务/提醒预期与实际、通知权限；确保编辑中的草稿不丢、重复提醒不重发。多屏记录每屏逻辑尺寸/DPI、拔屏/工作区变化、主窗和Floating/Mini恢复与穿透外部恢复路径。系统消息注入和几何算法通过仅是自动回归，不能替代这些操作。macOS和独立干净机另行验收。

## macOS差异

PowerShell采样工具仅支持Windows原生后端。macOS仍使用`bash scripts/build-macos.sh`构建，并按对应预设运行CTest；尚无本轮macOS长跑/睡眠/多屏证据，不将Windows结果移植为PASS。
