# 文档导航

## 当前入口（2026-10-26）

任务列表的多选可发现性、父子图标及详情响应复审见[UX-37分析与切片方案](design/task-list-interaction-review.md)；选择、关系及密度的本轮代码映射见文末；详情会话/性能剩余项单独追踪，实际状态见PROGRESS。

用户追加的同屏密度问题扩展为[UX-37D全局桌面密度](design/desktop-density-review.md)：统一侧栏、列表、详情、日历、菜单/选择器、设置及组件的桌面行关系、字级、间距和命中；已作为本轮实现契约，全表面迁移与验证边界见文末和PROGRESS。

[UX-34系统评估与优化方案](design/system-ux-review-2026-10.md)汇总全界面72项评审、证据与纵向实施顺序；[日历v2](design/calendar-workspace-v2.md)定义同任务的日/周时间安排、月分布和年导航；[就近任务面板](design/contextual-task-inspector.md)定义默认详情及展开方式。FR-034/035及[ADR-020](ADR/020-contextual-task-workspace.md)已由用户接受并实施；具体代码与验收映射见[UX-34实现索引](design/ux34-implementation.md)，实际通过范围以PROGRESS为准。

| 当前已接受行为契约 | 入口 |
|---|---|
| 统一任务、分类/标签及两种查看方式 | [产品哲学](product-philosophy.md)、[UI-27](design/unified-task-system.md) |
| 两级分类、拖放/右键及草稿 | [UI-28](design/catalog-and-list-system.md)、[UI-29](design/desktop-task-inspector.md) |
| 智能范围与父子完成/删除/移动联动 | [UI-30](design/task-list-context.md)、[ADR-017](ADR/017-reminders-views-and-task-trees.md) |
| 全窗口风格、两层任务及子项直达 | [UI-31](design/macos-desktop-language.md)、[UI-32](design/compact-navigation-and-subtasks.md) |
| 连续月历、直接时刻新建与图标侧栏 | [UX-35](design/continuous-month-calendar.md) |
| 完整新增浮窗、原子草稿与桌面密度 | [UX-36](design/calendar-creation-and-density.md) |
| 上下文日历、共享就近面板与模块边界 | [UX-34实现索引](design/ux34-implementation.md)、[ADR-020](ADR/020-contextual-task-workspace.md)、[ADR-021](ADR/021-calendar-time-commands.md) |
| 同一起止安排、任务动作与重命名 | [UI-33](design/shared-task-time-and-actions.md)、[ADR-019](ADR/019-shared-task-time.md) |

实际状态和平台证据只见[PROGRESS](PROGRESS.md)。本页后续历史文档用于追溯，不恢复已取消的领域/项目、双日程或清单概念；当前契约优先，目标提案与交付事实分开。

测试层级、删减依据、链路责任及fast/ui/full命令见[测试方法](testing-method.md)。

## 阅读顺序

1. [项目入口](../README.md) → [进度](PROGRESS.md)：先确认实际代码和当前任务。
2. [产品规格](01_PRODUCT_SPEC.md) → [技术基线](02_TECHNICAL_DESIGN.md) → [原始实施计划](03_IMPLEMENTATION_PLAN.md)：需求及验收基线。
   [产品哲学与概念关系](product-philosophy.md)：统一任务、两级分类、标签及列表/日历视图各自解决的问题，后续功能取舍依据。
3. [工程方法](engineering.md) → [工程原则](principles.md)：人与 AI 使用同一开发纪律。
4. [当前详细设计](design/architecture.md) → [模块契约](design/contracts.md) → [界面设计](design/ui.md)。
   UI修改另必读：[官方调研](design/ui-research.md) → [UI/UX设计基准](design/ui-baseline.md) → [验收与迁移](design/ui-acceptance.md)。
5. [里程碑及任务队列](milestones.md)：依赖、下一批切片、阶段门禁。
6. [环境与构建](environment.md) → [验证记录](verification-report.md) → [平台矩阵](platform-test-matrix.md)。

实际使用参阅[使用说明](user-guide.md)、[1.0源码发行说明](release-notes-v1.0.md)与[第三方记录](third-party-notices.md)。

本轮核心能力实现与本地发行见 [v0.1交付设计](design/release-v0.1.md)。实际验证仍以PROGRESS及验证报告为准。

后续UI-03A切片见[共享草稿的响应式详情](design/responsive-details.md)。

当前Windows V1候选交付：[设计](design/v1-candidate.md) → [逐项需求审计](v1-feature-matrix.md) → [发行说明及升级](release-notes-v1-rc.md)。

全界面视觉规范与控件实现见 [UI-07视觉统一](design/visual-unification.md)：主窗、弹窗、菜单、表单、日历和桌面组件共用设计token与DaymarkStyle。

组件降干扰及Apple日历/提醒事项对照见 [UI-08评估](design/widget-calendar-review.md)，包含任务日期与独立事件的边界、关联契约和后续切片。

CAL-01/02的追加行为见 [FR-019](01_PRODUCT_SPEC.md)、[ADR-013](ADR/013-local-events.md)和[RC2说明](release-notes-v1-rc2.md)。

任务详情、Today工具区、可编辑日期及组件菜单精修见 [UI-09设计](design/ui-detail-refinement.md)。

统一任务编辑面板及单一草稿/滚动契约见 [UI-10设计](design/unified-task-editor.md)。

Windows稳定性与恢复切片见[设计](design/windows-stability.md)及[连续运行验收指南](stability-guide.md)。短跑与8小时、真实睡眠、多屏验收分别记录。

Windows离线交付的只读清单、原生运行及ZIP验证见[设计](design/windows-delivery.md)和[交付校验指南](delivery-guide.md)。

## 文档归属

| 事实 | 唯一归属 |
|---|---|
| 用户行为、功能范围、需求 ID | PRD |
| 原始工程约束、SQL 建议和接口基线 | TDD |
| 当前模块、线程与接口细节 | design/ |
| 原则及开发流程 | principles.md、engineering.md |
| 任务依赖与验收拆分 | milestones.md |
| 当前状态与进行中项 | PROGRESS.md |
| 实际执行结果 | verification-report.md、platform-test-matrix.md |
| 工程决策及已接受偏差 | [ADR 索引](ADR/README.md) |
| 单项任务记录模板 | [任务模板](templates/task.md) |
| W-01实施及人工验收步骤 | [窗口控制任务](tasks/W-01.md) |
| 基础任务纵向切片设计 | [BASIC-01](design/basic-tasks.md) |
| 基础功能交付与用例索引 | [BASIC-01记录](tasks/BASIC-01.md) |
| Things/Apple参考事实与来源 | [UI调研](design/ui-research.md) |
| 项目UI目标、布局、token、交互 | [UI/UX设计基准](design/ui-baseline.md) |
| UX-01～14验收、UI实施顺序 | [UI验收](design/ui-acceptance.md) |

原始三个编号文档保留，不为迎合实现删除需求；用户已指定的 Qt 版本选择见 ADR-007。冲突按 PRD 产品行为、TDD 工程约束、Plan 顺序裁决。未解决的冲突不得自动当作需求放弃。

统一滚动边界、惯性及备注单一滚动轴见[UI-11设计](design/scroll-interaction.md)。

布局密度、图标及程序文件标识精修见[UI-12设计](design/ui-polish.md)。

Apple HIG为项目UI/UX强制依据，见[原则P-13](principles.md)；完整视觉关系及月格标题契约见[UI-13设计](design/visual-system.md)。

全部页面的信息架构、分类设置、统一表单、空态及窗口操作链见[UI-14全界面重设计](design/hig-redesign.md)。

独立桌面日历的查询、窗口、透明度、草稿与验收契约见[CAL-03设计](design/desktop-calendar.md)。

循环事件、事件提醒、schema4升级与iOS参考的月历层级见[CAL-04](design/event-series.md)、[ADR-014](ADR/014-event-series.md)。

当前版本系统评估和后续路线见[评估](review-rc2.md)，CAL-05契约见[日程浏览定位](design/calendar-browsing.md)。

全界面按钮文字/图文/纯图标决策及新浅深色彩体系见[UI-15](design/actions-and-color.md)。

任务详情的行关系、单个互斥归属选择与条件分组见[UI-16布局](design/task-detail-layout.md)。

参考iOS的圆角分组、滑动开关与拖放伪按钮处置见[UI-17](design/ios-grouped-details.md)。

UI-18按[就近编辑设计](design/direct-editing.md)推进共享日期/时间、快速编辑与全页直接选择，保留原子草稿和平台门禁。

UI-19按[原行编辑及即时反馈](design/inline-task-experience.md)修订快速卡形式、整理与创建、详情动作、列表/完成反馈和菜单响应。

UI-20 / CAL-06A按[连续日程与视觉修订](design/calendar-visual-refresh.md)推进跨日色条、长按排序、循环日程、全局颜色/行关系及提醒单层提交。

UI-21按[导航与主内容页面](design/navigation-and-pages.md)区分长期目的地、就近创建与短时模态操作，统一重复安排、组织管理和提醒历史的页面导航。

全产品功能与UI/UX按[体验复审与实施方案](design/product-experience-review.md)审视，落实[产品哲学](product-philosophy.md)及P-14“简约而不简单”：逐页面取舍、完整操作链、状态与验收、UI-22～26切片。设计目标与当前实现状态分别记录。

用户截图追加的详情直达、六视图导航拖放、安排生效与任务日期范围横条见[UI-23A](design/direct-task-actions.md)及[ADR-015](ADR/015-task-ranges-and-navigation.md)。

2026-10-25新需求系统分析与目标架构见[UI-27统一任务系统](design/unified-task-system.md)及[ADR-016](ADR/016-unified-task-system.md)，明确两级分类、真实子任务、定时/循环任务及迁移；新定义优先于此前对象划分，状态见PROGRESS。
