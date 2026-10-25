# 架构决策索引

ADR-001～006 已记录在 [原始 TDD](../02_TECHNICAL_DESIGN.md)，继续有效。

| ID | 决策 | 状态 |
|---|---|---|
| [007](007-qt-toolchain.md) | 固定 Qt 6.10.2，使用匹配 Kit 与环境变量 | 已接受：用户指定 |
| [008](008-incremental-bootstrap.md) | 骨架与完整 M0/M1 分开验收 | 已接受：实施顺序细化 |
| [009](009-basic-task-slice.md) | 本地任务闭环、线程和显式保存 | 已接受：用户本轮基本功能指令 |
| [010](010-ui-ux-target.md) | Things/Apple参考下的独立UI/UX目标与分批验收 | 已接受：用户指定设计目标 |
| [011](011-release-and-recurrence.md) | 本地v0.1交付、周期身份边界和预备CI | 实施；远程发布等待用户后续安排 |
| [012](012-v1-candidate-and-area-archive.md) | Windows V1候选范围、领域归档迁移与JSON兼容 | 实施；正式双平台门禁保留 |

| [013](013-local-events.md) | 本地事件、任务议程、schema003及关系/时区边界 | 已接受：用户授权CAL-01/02 |

| [014](014-event-series.md) | 事件系列、当地时间投影、schema004与提醒去重 | 已接受：用户授权CAL-04 |

| [015](015-task-ranges-and-navigation.md) | 任务日期范围投影、导航移动与明确安排意图 | 已接受：用户截图反馈 |

- [ADR-016：两级分类与统一任务系统](016-unified-task-system.md)

- [ADR-017：提醒事项式视图与父子联动](017-reminders-views-and-task-trees.md)

- [ADR-018：任务父子限制两层](018-two-level-tasks.md)

- [ADR-019：列表与日历统一起止安排](019-shared-task-time.md)
