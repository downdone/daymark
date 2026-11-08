# 文档导航

本页是项目文档的唯一入口。先读「当前状态」，再按角色选读。所有「实际通过/未通过」以[进度](PROGRESS.md)为准；设计文档描述契约，不代替验证证据。

## 1. 当前状态与产品定义

| 文档 | 内容 |
|---|---|
| [当前状态](STATUS.md) | 版本、schema、CI 与已知问题摘要 |
| [当前进度](PROGRESS.md) | 现行功能状态、收尾队列、真实验证与外部边界 |
| [历史实施日志](PROGRESS-HISTORY.md) | 各轮原始过程/失败/证据，旧“进行中”不重复进入当前队列 |
| [产品哲学](product-philosophy.md) | 统一任务、两级分类、标签与两种视图的概念关系 |
| [产品规格](01_PRODUCT_SPEC.md) | 需求 ID（FR-xxx）定义基线 |
| [技术基线](02_TECHNICAL_DESIGN.md) | 原始 TDD，含 ADR-001～006 原文与历史 schema |
| [原始实施计划](03_IMPLEMENTATION_PLAN.md) | 最初的 M0～M7 拆分（历史） |
| [使用说明](user-guide.md) | 面向用户的功能与操作 |

## 2. 工程纪律

| 文档 | 内容 |
|---|---|
| [环境与构建](environment.md) | Qt Kit、环境变量、Windows/macOS 命令 |
| [工程方法](engineering.md) | 人机协作流程、任务状态、验证层级 |
| [工程原则](principles.md) | P-01～P-22，含 Apple HIG 约束（P-13） |
| [测试方法](testing-method.md) | 测试分层、标签、fast/ui/full、链路责任 |
| [Windows 安装器设计](design/windows-installer.md) | 完整安装向导、SDK事务、修复/卸载与数据保护 |
| [桌面组件指针移动](design/widget-pointer-move.md) | Mini 按钮职责、统一拖动与置顶/锁定状态 |
| [全局日志与底层诊断](design/runtime-diagnostics.md) | 本地日志入口、轮换、脱敏、Qt/TLS错误与请求关联 |
| [对象管理全面审计](design/object-management-audit.md) | MEM-03/04对象所有权、按需呈现实现与原生内存对照 |
| [客户端内存专项](design/client-memory-specialist.md) | MEM-02少量任务/主窗＋小窗的真实短测、资源生命周期与分片优化设计 |
| [性能与内存策略](design/performance-and-memory.md) | 基线、分层分析、逐项优化与内存证据 |
| [性能优化记录](performance-report-2026-11.md) | PERF-01同配置对比、真实Release测量、回归及剩余边界 |
| [里程碑](milestones.md) | 执行队列与阶段门禁 |
| [发布规则](release-policy.md) | 版本、标签、源码与二进制门禁 |
| [第三方说明](third-party-notices.md) | Qt/SQLite/MinGW 许可与再分发义务 |

新增能力先读[设计与评审标准](design/new-capability-standard.md)，使用[设计模板](templates/capability-design.md)。

## 3. 设计契约（现行）

### 3.1 界面与视觉
[品牌接入契约](design/brand-identity-integration.md) · [现行母版](../assets/brand/current/README.md) · [完整VI与PDF](../resource/VI/README.md) · [UI/UX 设计基准](design/ui-baseline.md) · [验收与迁移](design/ui-acceptance.md) · [官方调研](design/ui-research.md) · [全局视觉系统](design/global-visual-system.md) · [排版与比例留白](design/typography-and-proportional-spacing.md) · [空间稳定与滚动槽](design/stable-layout-and-scroll.md) · [提示与垂直节奏](design/tooltip-and-layout-rhythm.md) · [全菜单](design/menu-system-review.md) · [侧栏与字标](design/sidebar-and-wordmark.md) · [两栏设置](design/settings-split-navigation.md) · [详情尺寸](design/inspector-size-control.md)

### 3.2 任务与列表
[智能时间输入](design/task-time-recognition.md) · [统一任务系统](design/unified-task-system.md) · [提醒事项式列表](design/reminders-list-refinement.md) · [列表布局与编辑节奏](design/list-layout-and-editing-rhythm.md) · [双击原行草稿](design/list-double-click-draft.md) · [就近任务面板](design/contextual-task-inspector.md) · [任务列表交互](design/task-list-interaction-review.md) · [桌面密度](design/desktop-density-review.md)

### 3.3 日历与时间
[日历工作区 v2](design/calendar-workspace-v2.md) · [连续月历](design/continuous-month-calendar.md) · [日历创建与密度](design/calendar-creation-and-density.md) · [直接时段选择](design/direct-time-range-picker.md) · [年定位与尺寸](design/year-tags-and-calendar-sizing.md) · [macOS 日历操作](design/macos-calendar-interaction.md) · [重复周期](design/repeat-choice-popover.md)

### 3.4 桌面组件与窗口
[Windows标题栏切换](design/windows-title-bar.md) · [小窗分类快查（设计评审）](design/widget-category-quick-find.md) · [小窗任务操作与提醒弹窗](design/widget-task-actions-and-reminder-popup.md) · [三形态统一与球/小窗动效](design/desktop-widget-modes-proposal.md) · [组件统一绘制事务](design/widget-rendering-transaction.md) · [悬停Mini球与紧凑日历](design/compact-desktop-surfaces.md) · [桌面表面精修](design/desktop-surfaces-polish.md) · [桌面层与年焦点](design/desktop-layer-and-year-focus.md) · [桌面菜单锚点](design/desktop-menu-anchoring.md) · [架构](design/architecture.md) · [模块契约](design/contracts.md)

### 3.5 可选账号与同步
[错误码与连接诊断](design/diagnostic-codes.md) · [官方账号与自定义服务](design/account-service-choice.md) · [独立账号窗口与设计评审](design/account-window.md) · [服务运行与恢复](../server/README.md) · [账号与同步](design/account-and-sync-service.md) · [工程架构](design/server-architecture.md) · [协议 v1](api/sync-v1.md) · [模块入口](design/sync-modules/README.md) · [强类型契约](design/sync-contracts/README.md) · [桌面实现契约](design/sync-client-implementation.md)

## 4. 决策记录

[ADR 索引](ADR/README.md)：007～034，含状态（现行/部分取代/已被取代/提案）。ADR-001～006 原文见[技术基线](02_TECHNICAL_DESIGN.md) §1。

## 5. 交付与运维

[交付指南](delivery-guide.md) · [稳定性指南](stability-guide.md) · [平台矩阵](platform-test-matrix.md) · [验证记录](verification-report.md) · [版本与发行索引](releases.md)

在线更新：[总体方案](design/windows-online-update.md) · [详细设计与GitHub准备](design/update/README.md)（Windows发行、真实公网验收及边界见PROGRESS；Linux/macOS本轮跳过）。

## 6. 历史设计（已完成切片）

以下文件记录已交付切片的当时契约，**不再约束当前实现**，仅供追溯。当前行为以第 3 节与[进度](PROGRESS.md)为准。

- 早期骨架：[ui](design/ui.md) · [basic-tasks](design/basic-tasks.md) · [v0.1 交付](design/release-v0.1.md)
- 视觉演进：[visual-unification](design/visual-unification.md) · [visual-system](design/visual-system.md) · [actions-and-color](design/actions-and-color.md) · [hig-redesign](design/hig-redesign.md) · [ui-polish](design/ui-polish.md)
- 详情演进：[ui-detail-refinement](design/ui-detail-refinement.md) · [unified-task-editor](design/unified-task-editor.md) · [task-detail-layout](design/task-detail-layout.md) · [desktop-task-inspector](design/desktop-task-inspector.md) · [responsive-details](design/responsive-details.md) · [ios-grouped-details](design/ios-grouped-details.md) · [inspector-position-and-trash-selection](design/inspector-position-and-trash-selection.md) · [row-insets-and-startup-presentation](design/row-insets-and-startup-presentation.md) · [contextual-popover-polish](design/contextual-popover-polish.md) · [help-tags-and-menu-type](design/help-tags-and-menu-type.md)
- 任务模型演进：[catalog-and-list-system](design/catalog-and-list-system.md) · [task-list-context](design/task-list-context.md) · [shared-task-time-and-actions](design/shared-task-time-and-actions.md) · [compact-navigation-and-subtasks](design/compact-navigation-and-subtasks.md) · [direct-editing](design/direct-editing.md) · [inline-task-experience](design/inline-task-experience.md) · [direct-task-actions](design/direct-task-actions.md) · [macos-desktop-language](design/macos-desktop-language.md)
- 日历演进：[calendar-browsing](design/calendar-browsing.md) · [desktop-calendar](design/desktop-calendar.md) · [event-series](design/event-series.md) · [calendar-visual-refresh](design/calendar-visual-refresh.md) · [navigation-and-pages](design/navigation-and-pages.md)
- 评估与计划：[system-ux-review-2026-10](design/system-ux-review-2026-10.md) · [ux34-implementation](design/ux34-implementation.md) · [product-experience-review](design/product-experience-review.md) · [review-rc2](review-rc2.md) · [v1-candidate](design/v1-candidate.md) · [v1-feature-matrix](v1-feature-matrix.md) · [scroll-interaction](design/scroll-interaction.md) · [widget-calendar-review](design/widget-calendar-review.md) · [windows-delivery](design/windows-delivery.md) · [windows-stability](design/windows-stability.md)
- 旧发行说明：[v0.1](release-notes-v0.1.md) · [1.0.0-rc.1](release-notes-v1-rc.md) · [1.0.0-rc.2](release-notes-v1-rc2.md) · [1.0.0](release-notes-v1.0.md) · [1.0.1](release-notes-v1.0.1.md) · [1.0.2](release-notes-v1.0.2.md)

## 7. 文档归属

| 事实 | 唯一出处 |
|---|---|
| 需求 ID 与产品行为 | [产品规格](01_PRODUCT_SPEC.md) |
| 当前实现/验证状态 | [进度](PROGRESS.md) |
| 界面契约 | 第 3.1～3.4 节 |
| 工程决策与偏差 | [ADR 索引](ADR/README.md) |
| 任务依赖与验收拆分 | [里程碑](milestones.md) |
| 测试分层与责任 | [测试方法](testing-method.md) |
| 许可与再分发 | [第三方说明](third-party-notices.md) |
| 文档结构调整计划 | [梳理方案](design/docs-restructure-plan.md) |
