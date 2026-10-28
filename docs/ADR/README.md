# 架构决策索引

ADR-001～006 的原文在[原始 TDD](../02_TECHNICAL_DESIGN.md) §1。后续决策按顺序追加，不覆盖历史；被取代的条目保留，供追溯。

状态说明：**现行** = 仍然约束实现；**部分取代** = 部分条款被更晚的 ADR 覆盖；**已被取代** = 仅作历史记录；**提案** = 尚未接受。

| ID | 决策 | 状态 |
|---|---|---|
| 007 | [固定 Qt 6.10.2 与原生工具链](007-qt-toolchain.md) | 现行；发布构建须用开源 Kit，见 027 |
| 008 | [骨架与完整 M0/M1 分开验收](008-incremental-bootstrap.md) | 历史（已完成） |
| 009 | [本地任务闭环、线程与显式保存](009-basic-task-slice.md) | 部分取代：保存体验由 020 覆盖 |
| 010 | [Things/Apple 参考下的独立 UI/UX 目标](010-ui-ux-target.md) | 现行 |
| 011 | [本地 v0.1 交付、周期身份与预备 CI](011-release-and-recurrence.md) | 历史；发布许可待办已由 027 落实 |
| 012 | [Windows V1 候选范围与领域归档迁移](012-v1-candidate-and-area-archive.md) | 历史 |
| 013 | [本地事件、任务议程与关系/时区边界](013-local-events.md) | 已被取代：016 取消独立事件实体 |
| 014 | [事件系列、当地时间投影与提醒去重](014-event-series.md) | 已被取代：016 取消独立事件实体 |
| 015 | [任务日期范围投影与导航移动](015-task-ranges-and-navigation.md) | 部分取代：时间定义由 019 覆盖 |
| 016 | [两级分类与统一任务系统](016-unified-task-system.md) | 部分取代：17/18/19 分别覆盖父子、深度与时段 |
| 017 | [提醒事项式视图与父子联动](017-reminders-views-and-task-trees.md) | 现行 |
| 018 | [任务父子限制两层](018-two-level-tasks.md) | 现行 |
| 019 | [列表与日历统一起止安排](019-shared-task-time.md) | 现行 |
| 020 | [上下文任务工作区与就近面板](020-contextual-task-workspace.md) | 现行 |
| 021 | [小时轴投影与改期命令](021-calendar-time-commands.md) | 现行 |
| 022 | [测试分层与仅源码发布](022-test-layers-and-source-release.md) | 测试分层现行；仅源码发布的许可理由由 027 更新 |
| 023 | [macOS 日历核心操作](023-macos-calendar-interaction.md) | 现行 |
| 024 | [本地预览包范围](024-local-preview-package.md) | 部分取代：包内容由 025 覆盖 |
| 025 | [最小便携运行包白名单](025-minimal-portable-payload.md) | 现行 |
| 026 | [可选登录同步与单体 Go + SQLite 服务](026-go-sqlite-sync-service.md) | 已接受方向；待实施 |
| 027 | [CI 使用开源 Qt（LGPLv3）与依赖合规](027-open-source-qt-ci.md) | 现行 |
| 028 | [同步接口契约、RPC 选型与兼容性门禁](028-sync-api-contract-and-rpc.md) | 提案，待评审 |
