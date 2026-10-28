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
| macOS（CI） | 构建通过；测试见下「已知问题」 |
| QML lint | `daymark_qmllint`、`daymark_style_qmllint` 通过 |

## 已知问题

- macOS CI 的 CTest 仍有失败项。CI 会把失败测试名以 workflow 注解公开（`scripts/report-ctest-failures.py`），修复后更新本页。
- macOS 物理输入、读屏、通知、穿透、多屏/睡眠、干净机部署与签名未验收。

## 报告规则

- 「通过」只代表所列命令在该环境真实执行成功；不代表其他平台。
- 未执行的平台与门禁标 UNVERIFIED，不用版本号或合成截图代替。
- 逐轮证据（命令、日志名、SHA、耗时）保留在[进度日志](PROGRESS.md)与[验证记录](verification-report.md)。
