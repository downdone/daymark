# Daymark 业务后台与可选择统计

本轮实现 BIZ-01 / TEL-01；独立通知中心已取消，公告只在后台管理。当前验证、部署和发布状态只见[PROGRESS](../docs/PROGRESS.md)。设计见[业务系统](../docs/design/daymark-business-platform.md)，独立接口见[OpenAPI](../docs/api/business-v1.openapi.json)，取舍见[ADR-036](../docs/ADR/036-business-admin-and-telemetry.md)。

## 启用与管理员引导

继续现有 Go 单体、HTTPS 和业务 SQLite，无额外常驻服务。配置 `admin_enabled`、`telemetry_enabled` 默认均为 `false`，两者独立；服务端启用统计也不会替客户端同意。只启用后台、不开统计时仍可管理用户和公告。正式入口须沿现有 HTTPS 配置，管理路径可在反向代理额外限制运维网络；服务不信任任意转发头。

先停服务，使用同一个配置/数据目录执行维护命令，口令从 stdin 读取。普通桌面注册账号不能进入后台。

Windows PowerShell 7（在仓库根目录）：

```powershell
$adminPassword = Read-Host '新管理员口令' -MaskInput
try { $adminPassword | & ./build/daymark-server.exe --config server/config.local.json --create-admin operator --admin-roles owner }
finally { Remove-Variable adminPassword }
```

macOS/Linux bash：

```bash
read -r -s -p '新管理员口令: ' admin_password
printf '%s\n' "$admin_password" | ./build/daymark-server --config server/config.local.json --create-admin operator --admin-roles owner
unset admin_password
```

命令仅在本机终端返回一次 TOTP 设置密钥/otpauth URL 和8个一次性恢复码；请保管于受保护的运维密码库。不要把输出重定向到公共日志或工单。启动服务后访问 `https://sync.example.com/admin`，使用独立口令及动态验证码或恢复码登录。口令至少8个 Unicode 字符、最多1024 UTF-8字节。

角色：`owner` 管理管理员权限；`support` 管理用户状态与会话；`publisher` 管理公告；`analyst` 只读聚合；`auditor` 只读审计。可组合；首位管理员必须包含 owner。不能停用最后一位有效 owner，也不能通过页面给自己变更权限。管理员最多100位；会话8小时，写操作要求15分钟内 MFA，过期须退出后重新登录。权限变更使旧会话失效。

遗失 MFA 和恢复码时，停服务并使用同样的 stdin 方式执行 `--recover-admin operator`。该维护命令保留角色，重置口令、TOTP、恢复码，增加身份版本使旧会话失效，并记入审计。没有 HTTP MFA 绕过入口。独占目录锁防止维护命令与运行服务同时改库。

`admin.key` 是管理员 TOTP 的库外加密密钥，不进入源码，也不随数据库归档自动托管。必须与现有备份/ledger密钥一样独立加密托管。恢复含管理员记录的业务备份后，先还原匹配的 `admin.key`、核对撤权与删除记录，再开启后台；缺少密钥时不会静默生成新密钥。普通用户的密码恢复仍使用原有 `--reset-password`，不赋予管理权限。

管理初始化失败时只关闭管理入口并返回503，任务接口继续提供服务；维护CLI仍明确失败退出。统计库无法打开同样不阻断任务接口，但有待清理统计的账号删除不能假称已完成。运维日志只记录固定错误码和请求关联，不输出密钥或原始表单。

## 后台行为

- 用户页只读基础账号状态与有效会话数量；支持搜索/状态筛选/分页。停用、恢复、撤销全部会话都有版本校验、原因及确认页；不显示任务正文，不提供冒充用户登录。
- 公告支持草稿编辑、平台/版本/指定账号范围、UTC发布时间与到期时间、预览、立即/定时发布和撤回。正文纯文本并自动转义，动作只允许固定枚举，无任意 URL 或脚本。已发布公告不可原地编辑，撤回后可另建草稿；不向桌面投递，没有客户端已读状态或消息入口。
- 管理写入的效果、操作回执和审计在同一事务提交。相同 `operation_id` 和输入重试返回同一回执；输入改变或旧版本返回冲突。返回失败时表单保留非敏感草稿；密码/OTP不回填。
- Cookie 为 HttpOnly、SameSite Strict，HTTPS 时 Secure；所有写入校验精确 Origin 和会话 CSRF，`Referrer-Policy: same-origin` 保持浏览器表单 Origin 可验证。页面无外链脚本/字体/CDN。
- 审计保留180天、幂等回执90天；过期公告按到期时间、撤回公告按最后撤回时间保留90天。每分钟各类最多清理1000行。草稿/保留公告总量最多1000条。审计为数据库逻辑记录，不承诺抵御服务器管理员篡改。

## 客户端统计和数据生命周期

设置→数据与隐私→使用统计，默认关闭，未登录不可开启。不弹首次启动授权窗。开启后须当前账号服务确认许可才采集；不会把自定义服务器的统计另传官方服务。当前内置采集点为已许可启动耗时、使用的主窗/小窗/Mini/球、低频 Windows 私有提交内存区间、更新候选就绪。更新其他结果已有类型契约，但尚未接入所有更新器终态，不应据此计算完整更新成功率。

字段仅固定事件名/版本、随机事件ID、发生时间、应用版本/操作系统及枚举分桶；不含任务、备注、分类、标签、输入、路径、URL或原始错误。接收端按登录账号和安装设备关联假名主体，用于许可隔离、撤回和删除；不是不可关联的匿名统计。

本期使用独立 HTTP JSON，未引入 OpenTelemetry/OTLP、gRPC、Collector 或第三方分析 SDK。事件工厂、队列、传输与仓储各自独立，可在未来明确需求下增加适配器，当前只维护一种传输。

| 项目 | 行为/预算 |
|---|---|
| 许可 | `GET/PUT /telemetry/v1/consent`，操作ID幂等、epoch CAS，服务恢复后明确重新确认 |
| 批次 | `POST /telemetry/v1/batches`，最多32事件/32KiB，202为已提交幂等回执 |
| 本机队列 | 首次主动选择才建独立SQLite，按账号/服务/资料隔离；最多1000事件，保留3天，数据库约1MiB加临时journal预算 |
| 调度 | 复用工作区线程和现有同步调度；通常15分钟批量，内存最多30分钟采样；单次网络3秒无进展/4秒硬限，不为统计阻塞退出 |
| 失败 | 指数退避加抖动，遵守Retry-After；满队列丢旧统计，不丢任务；不把统计错误改写为任务同步失败 |
| 撤回 | 立即停止本机采集/发送并清空事件；持久化撤回意图，离线恢复后优先处理；服务器清理原始事件及主体贡献 |
| 服务存储 | 独立 `metrics/metrics.sqlite` 与有界写者，不争用任务数据库写者；原始事件7天、贡献/汇总90天，每轮有界清理 |
| 配额 | 每主体每日2000事件、每账号最多50统计主体，原始事件总量上限50万；超限拒绝统计，核心任务接口继续可用 |
| 看板 | 7/30/90天，UTC接收日期；细分至少10台参与设备；图表前12项、表格最多500项，截断明确提示 |

账号删除先将跨库清理意图写入业务库，统计清理成功后才报告删除完成；失败可恢复，不假称两个SQLite具有分布式事务。统计库不纳入任务灾备归档。重建统计库/恢复服务后，旧许可不自动复活。管理员页面的请求/错误计数是本进程启动以来的运行计数，重启归零，与自愿产品统计分开。

## 验证入口

在 `server` 目录执行 `go test ./...`、`go vet ./...`。规则/真实SQLite/HTTPS测试覆盖 MFA重放、恢复、权限、CAS、审计回滚、统计许可、内容拒收、去重、撤回、低样本抑制和跨库删除故障。OpenAPI测试编译所有schema并验证实际HTTP输出；核心任务协议保持独立。

Windows 在根目录载入 Qt6.10.2/MinGW 环境后：

```powershell
cmake --build --preset windows-release --parallel 4
ctest --test-dir build/windows-release --output-on-failure
$env:DAYMARK_QT_SYNC_TEST_EXE = (Resolve-Path build/windows-release/bin/test_task_workspace.exe).Path
Push-Location server
try { go test ./internal/httpapi -run '^TestQtDesktopEndToEnd$' -count=1 -v }
finally { Pop-Location; Remove-Item Env:DAYMARK_QT_SYNC_TEST_EXE }
```

macOS/Linux桌面链本轮跳过；可执行Go规则/存储测试，但不能将其冒充桌面平台验收。所有数据为临时合成资料，未自动启用生产管理或采集。
