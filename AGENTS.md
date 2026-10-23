# AGENTS.md — Daymark 仓库 Codex 行为约束

> 仓库根目录保留此文件，供 Codex / 其他代码代理每次工作前读取。

## 当前工程入口（2026-10-22）

- 用户指定 **Qt 6.10.2**，实现以该版本固定构建；原规格6.8+作为历史最低基线，见 `docs/ADR/007-qt-toolchain.md`。
- 每轮先读 `docs/README.md`、`docs/PROGRESS.md`、`docs/engineering.md`、`docs/principles.md`，再读任务涉及的 `docs/design/` 契约。
- 执行队列见 `docs/milestones.md`；先设计、单切片实现、构建测试、记录真实证据。当前状态只维护在 PROGRESS。
- Windows入口：`scripts/build-windows.ps1`、`scripts/test-windows.ps1`、`scripts/run-windows.ps1`；macOS入口：`bash scripts/build-macos.sh`。
- Kit路径通过 DAYMARK_QT / DAYMARK_MINGW 注入，禁止提交本机绝对路径。只读参考相邻项目。
- 默认单代理；子代理/并行委派须用户明确授权。未经授权不自动推送、上传或发布。
- UI变更须先读 `docs/design/ui-baseline.md` 与 `docs/design/ui-acceptance.md`，引用受影响UX目标及状态/键盘/缩放验收；来源事实见ui-research.md。**Apple HIG是项目UI/UX的强制设计与评审依据**，遵守 `docs/principles.md` 的P-13：先映射HIG原则、定义完整视觉/交互关系，再实现和验证。Things仅作交互机制参考；遵守运行平台习惯，视觉与资源独立实现，不得将目标当成现有界面已通过。

## 基本任务

在此仓库实施 Daymark：跨平台 C++20/Qt Quick 任务管理与日历软件，Windows/macOS，SQLite 本地优先，透明桌面组件。所有功能与验收以 `docs/01_PRODUCT_SPEC.md`、`docs/02_TECHNICAL_DESIGN.md`、`docs/03_IMPLEMENTATION_PLAN.md` 为准；冲突时 PRD 的产品行为优先、TDD 的工程约束次之、Plan 的顺序再次之，并通过 ADR 澄清无法满足的地方。

## 工作纪律

1. **先查现状**：开始前检查项目结构、编译环境、现有测试和 `docs/PROGRESS.md`；不要假设仓库为空。
2. **小批次纵向交付**：逐步实现最小功能闭环，保持编译通过；优先 M0 的窗口 POC 和 M1 工程骨架。不得一次性凭空生成整套 UI/业务然后不编译。
3. **严禁默认引入云和账号**：不要新增服务器、后台服务、OAuth、远程遥测。V1 所有核心操作必须离线可用。
4. **不要复制 Things 3 专有资产**：只借鉴功能和交互原则；UI、图标、设计资源须独立或持有可用许可。
5. **守住层次**：QML 只处理 UI，任务判定集中在 C++ QueryService，SQLite 访问集中 Repository；平台原生调用隔离于 `src/platform`。
6. **重视时间语义**：start_on/deadline_on 为 QDate/本地日历日；remind_at 为 UTC 瞬间；MonthGrid 0-based 月份与 QDate 1-based 月份须在边界处理。
7. **更改数据模式须迁移**：为任何已交付 schema 变化创建新 migration，不覆盖历史迁移；先备份、再迁移。
8. **不丢数据**：删除任务软删；删除项目默认移出所属任务；导入失败零修改；备份必须是 WAL 一致快照。
9. **穿透须可恢复**：悬浮窗鼠标穿透启用后，托盘/主程序设置始终提供退出穿透的通路。
10. **验证真实可重复**：每轮必须尝试构建、运行 Qt Test/CTest；环境缺失要准确说明和提供命令，不得谎称成功；跨平台未测试就明确标记 UNVERIFIED。
11. **错误要有处理**：返回 Result/error code，QML 显示可操作提示，不吞错误、不能只在 stdout 打印失败。
12. **性能适度**：优先数据正确和 UI 可用，使用 model 增量刷新，不在 QML 高频全表扫描；针对 10k 数据做基准。
13. **提交前审查**：禁止提交个人任务、真实用户数据、绝对路径、Qt 本地 SDK 二进制、构建缓存、证书、隐私日志。
14. **第三方依赖**：优先 Qt 内置能力，如需引入写 ADR，注明必要性、许可、版本和替代方案。
15. **语言与文档**：代码标识英文，开发文档/用户说明优先中文；命令示例同时标明 Windows 和 macOS 差异。

## 每轮实施程序

1. 阅读 specs，列出本轮涉及需求 ID、设计接口、测试清单。
2. 更新 `docs/PROGRESS.md` 本轮计划、已知风险。
3. 编写/调整对应测试，实施最小代码变更。
4. 执行 build/test，修复当前轮引入的回归问题。
5. 更新功能文档/测试矩阵，汇报实际 PASS/FAIL/UNVERIFIED。
6. 源文件变更不应把系统用户目录作为测试 DB；所有自动测试使用 `QTemporaryDir`。

## 绝对禁止

- 在没有备份/确认的情况下删除或覆盖用户现有数据库。
- 把 Mock 成功当成真实 OS 窗口/系统通知功能已验证。
- UI 自行编写 Today/Upcoming 的筛选语义，形成多个逻辑版本。
- 用一个浮点 `opacity` 当作鼠标穿透功能。
- 未经用户授权执行账号登录、Git 推送、上传用户数据、商业软件数据抓取。
- 假定 macOS WidgetKit 与 QML Window 是同一个系统机制。

## 当前第一阶段

先按 `START_CODEX_PROMPT.md` 执行 M0 及可用工程基础，不提前做云同步/AI/移动端。
