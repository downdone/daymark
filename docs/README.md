# 文档导航

## 阅读顺序

1. [项目入口](../README.md) → [进度](PROGRESS.md)：先确认实际代码和当前任务。
2. [产品规格](01_PRODUCT_SPEC.md) → [技术基线](02_TECHNICAL_DESIGN.md) → [原始实施计划](03_IMPLEMENTATION_PLAN.md)：需求及验收基线。
3. [工程方法](engineering.md) → [工程原则](principles.md)：人与 AI 使用同一开发纪律。
4. [当前详细设计](design/architecture.md) → [模块契约](design/contracts.md) → [界面设计](design/ui.md)。
   UI修改另必读：[官方调研](design/ui-research.md) → [UI/UX设计基准](design/ui-baseline.md) → [验收与迁移](design/ui-acceptance.md)。
5. [里程碑及任务队列](milestones.md)：依赖、下一批切片、阶段门禁。
6. [环境与构建](environment.md) → [验证记录](verification-report.md) → [平台矩阵](platform-test-matrix.md)。

实际使用参阅[v0.1使用说明](user-guide.md)、[发行说明](release-notes-v0.1.md)与[第三方记录](third-party-notices.md)。

本轮核心能力实现与本地发行见 [v0.1交付设计](design/release-v0.1.md)。实际验证仍以PROGRESS及验证报告为准。

后续UI-03A切片见[共享草稿的响应式详情](design/responsive-details.md)。

当前Windows V1候选交付：[设计](design/v1-candidate.md) → [逐项需求审计](v1-feature-matrix.md) → [发行说明及升级](release-notes-v1-rc.md)。

全界面视觉规范与控件实现见 [UI-07视觉统一](design/visual-unification.md)：主窗、弹窗、菜单、表单、日历和桌面组件共用设计token与DaymarkStyle。

组件降干扰及Apple日历/提醒事项对照见 [UI-08评估](design/widget-calendar-review.md)，包含任务日期与独立事件的边界、关联契约和后续切片。

CAL-01/02的追加行为见 [FR-019](01_PRODUCT_SPEC.md)、[ADR-013](ADR/013-local-events.md)和[RC2说明](release-notes-v1-rc2.md)。

任务详情、Today工具区、可编辑日期及组件菜单精修见 [UI-09设计](design/ui-detail-refinement.md)。

统一任务编辑面板及单一草稿/滚动契约见 [UI-10设计](design/unified-task-editor.md)。

Windows稳定性与恢复切片见[设计](design/windows-stability.md)及[连续运行验收指南](stability-guide.md)。短跑与8小时、真实睡眠、多屏验收分别记录。

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
