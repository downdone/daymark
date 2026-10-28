# 当前状态

本页是**唯一的状态摘要**，每次版本/门禁变化时更新。逐轮详细记录见[进度日志](PROGRESS.md)。

## 版本与数据

| 项 | 值 |
|---|---|
| 应用版本 | 1.1.0（`CMakeLists.txt` 与 `DAYMARK_VERSION`） |
| 数据库 schema | 9 |
| 平台目标 | Windows 10/11 x64、macOS 13+ |
| 技术栈 | C++20、Qt 6.10.2（LGPLv3 动态链接）、SQLite（QSQLITE） |

## 构建与测试

| 项 | 状态 |
|---|---|
| 测试数量 | 25 个 CTest（分层见[测试方法](testing-method.md)） |
| Windows MinGW Release/Debug | 25/25 通过（本机实测） |
| Windows MSVC（CI） | 构建、测试、QML lint 通过 |
| macOS（CI，`macos-15`） | 构建、测试、QML lint 通过 |
| QML lint | `daymark_qmllint`、`daymark_style_qmllint` 通过 |

CI 双平台全绿：[run 40675789819](https://github.com/downdone/daymark/actions/runs/40675789819)（2026-10-28）。

## 已知问题

- 平台专属门禁仍未验收：macOS 物理输入、读屏、通知、穿透、多屏/睡眠、干净机部署与签名。CI 通过不等于这些门禁通过。

## 报告规则

- 「通过」只代表所列命令在该环境真实执行成功；不代表其他平台。
- 未执行的平台与门禁标 UNVERIFIED，不用版本号或合成截图代替。
- 逐轮证据（命令、日志名、SHA、耗时）保留在[进度日志](PROGRESS.md)与[验证记录](verification-report.md)。
