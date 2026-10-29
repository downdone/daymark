# 当前状态

本页是**唯一的状态摘要**，每次版本/门禁变化时更新。逐轮详细记录见[进度日志](PROGRESS.md)。

## 版本与数据

| 项 | 值 |
|---|---|
| 应用版本 | 1.1.0（`CMakeLists.txt` 与 `DAYMARK_VERSION`） |
| 数据库 schema | 10（追加账号/同步元数据，历史1～9未覆盖） |
| 平台目标 | Windows 10/11 x64、macOS 13+ |
| 技术栈 | C++20、Qt 6.10.2（LGPLv3 动态链接）、SQLite（QSQLITE） |
| 可选服务 | Go1.27.0单体 + SQLite3.54.1；HTTP/JSON v1，见[服务指南](../server/README.md) |

## 构建与测试

| 项 | 状态 |
|---|---|
| 测试数量 | Windows 26个CTest，新增account_access；其他平台不含Windows包校验（分层见[测试方法](testing-method.md)） |
| Windows MinGW Release | 44661df 完整26/26 PASS（275.06秒），新版便携包隔离验证通过 |
| Windows MinGW Debug | 之前基线25/25；当前同步切片未复验 |
| Windows MSVC（CI） | 6cfc6d7同步双端通过；完整桌面新run执行中 |
| macOS（CI，`macos-15`） | 6cfc6d7服务端通过；完整桌面contextual_workspace失败 |
| Linux（CI，`ubuntu-24.04`） | 6cfc6d7服务端/race通过；完整桌面3个UX探针失败 |
| QML lint | `daymark_qmllint`、`daymark_style_qmllint` 通过 |

上述桌面CI是同步改动前的已验证基线：[run 40684920204](https://github.com/downdone/daymark/actions/runs/40684920204)（2026-10-28）。新增同步workflow及当前源码须以新一次GitHub检查为准，不沿用旧run宣称通过。

账号/同步实现包含本地队列、完整批次应用、冲突、首次合并/大组、系统凭据、账号隔离与备份恢复。Go包测试/vet/race及真实两个Qt工作区↔Go TLS联调已有本机PASS；具体范围、最后修改后的门禁及未验证项见[PROGRESS](PROGRESS.md)与[实现验证清单](sync-verification.md)。源码实现不表示服务已部署。

业务源码6cfc6d7已推送并核对origin/main；其[Account and sync](https://github.com/downdone/daymark/actions/runs/40802576795)三平台Go（含Linux race）与Windows MSVC真实双端全部通过。[完整桌面run](https://github.com/downdone/daymark/actions/runs/40802576805)的macOS仍失败于UX34；Linux仍失败于UX34/UX38/UX39（contextual_workspace/desktop_surfaces/calendar_interaction），失败清单不再包含recurrence_service。Windows完整测试尚未完成；该workflow不能称为通过。此后文档提交不改变已验证业务源码。

## 自助注册（SYNC-06，2026-10-29）

提供POST /v1/auth/register及独立账号窗口中的“注册并同步”；旧服务器配置默认关闭，示例registration_enabled=true。用户名/密码注册不表示邮箱验证。Go全包test/vet、注册相关race、真实Qt注册后双端同步已通过；Windows Release曾25/25；后续完整探针在既有UX38 stage76失败。注册独立切片连续3轮、浅色/深色200%渲染检查通过，最终针对性回归及证据见PROGRESS。前述6cfc6d7远端基线不包含本轮注册，注册提交的CI另行核验；macOS原生和生产部署未验证。

## 独立账号窗口（SYNC-07/UX-61，2026-10-29）

主窗侧栏及折叠态提供独立账号入口，设置移除账号内容；[设计、实现前评审与实现后复核](design/account-window.md)已记录。Windows Release构建/lint、18段账号状态及接线、真实Qt/Go TLS联调PASS。完整回归曾24/25，修复样式预览裁剪定位后相关套件复验通过；最终desktop_surfaces/account_access 2/2 PASS，不将其称为单轮26/26。原生/平台边界见下方及PROGRESS，新提交CI需单独核验。

## Mini悬停球与紧凑日历（UX-62，2026-10-29）

设置→桌面组件选择Mini后可开启闲置收球；悬停恢复条形，菜单/拖动/键盘保持展开，尺寸持久化与平台失败回滚有临时设置单测。独立日历默认560×540、最小360×380，正文/辅助/标题12/11/15及月格、时间轴、年视图统一收紧；已有手动窗口尺寸保留。设计与复核见[UX-62](design/compact-desktop-surfaces.md)。完整26项、Windows合成输入与实际渲染、SDK隔离原生包验证PASS；物理平台边界仍见下方。

本地新包Daymark-1.1.0-windows-x64-local-20261029-144244.zip（39,466,619字节），源码44661df，未签名；路径、校验和、原始失败与最终证据见PROGRESS，不代表正式远端二进制发布。

## 已知问题

- 平台专属门禁仍未验收：macOS 物理输入、读屏、通知、穿透、多屏/睡眠、干净机部署与签名。CI 通过不等于这些门禁通过。
- 同步专项尚需原生平台验收：macOS Keychain、账号表单/冲突的完整原生键盘/200%/读屏、真实异机备份与断电恢复；未对私人资料或生产账号做测试。

## 报告规则

- 「通过」只代表所列命令在该环境真实执行成功；不代表其他平台。
- 未执行的平台与门禁标 UNVERIFIED，不用版本号或合成截图代替。
- 逐轮证据（命令、日志名、SHA、耗时）保留在[进度日志](PROGRESS.md)与[验证记录](verification-report.md)。
