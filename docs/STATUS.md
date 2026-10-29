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
| 测试数量 | 25 个 CTest（分层见[测试方法](testing-method.md)） |
| Windows MinGW Release | 当前同步工作树25/25通过（本机实测，见PROGRESS） |
| Windows MinGW Debug | 之前基线25/25；当前同步切片未复验 |
| Windows MSVC（CI） | 历史基线通过；e4303c8的contextual_workspace失败，见下方run |
| macOS（CI，`macos-15`） | 历史基线通过；e4303c8的contextual_workspace/recurrence_service失败 |
| QML lint | `daymark_qmllint`、`daymark_style_qmllint` 通过 |

上述桌面CI是同步改动前的已验证基线：[run 40684920204](https://github.com/downdone/daymark/actions/runs/40684920204)（2026-10-28）。新增同步workflow及当前源码须以新一次GitHub检查为准，不沿用旧run宣称通过。

账号/同步实现包含本地队列、完整批次应用、冲突、首次合并/大组、系统凭据、账号隔离与备份恢复。Go包测试/vet/race及真实两个Qt工作区↔Go TLS联调已有本机PASS；具体范围、最后修改后的门禁及未验证项见[PROGRESS](PROGRESS.md)与[实现验证清单](sync-verification.md)。源码实现不表示服务已部署。

e4303c8的[Account and sync](https://github.com/downdone/daymark/actions/runs/40800033839)三平台Go与Windows双端已通过；同提交的[桌面流水线](https://github.com/downdone/daymark/actions/runs/40800033857)仍有UX探针/重复任务测试失败。后续提交以其各自新run为准，不能把同步workflow通过称为全部CI通过。

## 已知问题

- 平台专属门禁仍未验收：macOS 物理输入、读屏、通知、穿透、多屏/睡眠、干净机部署与签名。CI 通过不等于这些门禁通过。
- 同步专项尚需原生平台验收：macOS Keychain、账号表单/冲突的完整原生键盘/200%/读屏、真实异机备份与断电恢复；未对私人资料或生产账号做测试。

## 报告规则

- 「通过」只代表所列命令在该环境真实执行成功；不代表其他平台。
- 未执行的平台与门禁标 UNVERIFIED，不用版本号或合成截图代替。
- 逐轮证据（命令、日志名、SHA、耗时）保留在[进度日志](PROGRESS.md)与[验证记录](verification-report.md)。
