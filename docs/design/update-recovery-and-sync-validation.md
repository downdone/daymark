# UPD-08 更新恢复与同步校验

## 功能、选型和架构

复用 C++ SDK、Win32 安装器、Qt 状态适配与 SQLite 日志。自动检查持久化间隔 24 小时，取消后手动可重试，仍遵守服务端限流。Qt 低频定时器支持长时间运行。底部工具栏单个更新入口显示进度、失败或重启，Ready 走既有保存重启门。

## 模块、接口与协议

Engine::shouldCheck 管调度。Qt UpdateController 清除过期取消反馈，使用原有 IPC 和状态文件。SetupManager::checkIdle 向安装向导提供运行预检，install 再获取独占租约避免检查后的竞争。本地 setup-helper 增加 check-idle。同步继续 HTTP JSON v1，category.remove 已由服务器移出分类内全部任务，客户端省略完全重合的 task.move_tree。

## 数据库、结构与可靠性

不改变 schema。更新沿用 preferences.json 的 last_attempt/not_before/rate_limit_until，自动取消退避保留。仅对已拒绝且未确认的同步日志，使用前后快照证明冗余分类迁移。旧记录保留 SUPERSEDED，后续未发送日志按原顺序生成新 mutation/client_seq，复制 dirty 和依赖，全程调用方事务。IN_FLIGHT、已确认和不确定是否提交的请求不修改。其他不可表示修改保留。

## UX 与隐私

UF-01/04/10/12/13、UX-09～11、P-03/07/13/17。HIG 的清楚反馈、用户控制落实为取消后重查、每日自动检查、键盘更新入口。安装提示退出后重试，不强杀应用。同步拒绝显示真实安全错误码，不把所有校验失败解释为客户端过旧。无第三方依赖或新服务。

## 测试、自审与交付

覆盖取消后立即重试、24 小时与限流，安装运行租约/退出/竞争，SQLite 分类删除中的已删除任务和旧阻塞日志依赖恢复，以及 UI 浅深/200% 和保存重启。自审要求恢复只重放能证明等价的已拒绝命令。公开升级和外部机器现场分别记录，实际结果见 PROGRESS。
