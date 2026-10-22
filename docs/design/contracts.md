# 模块与行为契约

以下为待实现契约；原始接口见 TDD §3.4，不表示相关类已经存在。

## 1. 公共值与错误（M1/M2）

`Result<T>` 包含成功值或 `Error{code, messageKey, safeParameters}`；无原始用户内容、SQL 全文或设备私有路径。错误码沿用 ValidationError、NotFound、Conflict、PersistenceError、PermissionError、UnsupportedPlatform、Cancelled。

`FieldPatch<T>` 明确 Unspecified/Clear/Set：未指定不改，Clear 写 NULL（非空字段拒绝），Set 写值。`rowVersion` 乐观校验失败为 Conflict；TaskPatch 不允许直接改 createdAt、id 等只读字段。异步适配另有 requestId，服务领域值不绑定 QML 控件。

`IClock` 提供 localToday/nowUtc；可注入 FakeClock。日期持久化 yyyy-MM-dd，UTC 固定 Z；提醒拒绝过去时间。

## 2. TaskService（FR-001/002/007）

| 命令 | 原子结果 | 失败保证 |
|---|---|---|
| createTask | 校验标题/归属/日期 → 插入 → 返回已提交任务 | 无空任务，输入由 UI 保留 |
| updateTask | 三态 patch + rowVersion → 验证完整候选 → 更新 | start>deadline 或归属冲突零修改 |
| complete/cancel/reopen | 修改状态和终态时间；重开清除终态时间 | 已终态重复提交不重复生成后继实例 |
| softDelete/restore | 更新删除时间及版本，不永久删除 | 无级联删除任务 |
| moveToProject/Area | 处理直接 Area/Project 互斥，清除不匹配 Heading | 外键和归属错误整个命令失败 |
| reorder | 上下文内 ID 集合校验，事务写整数序号 | 不接受跨视图任意 ID；失败原顺序不变 |

加入今天且 startOn 在未来时，更新为今天并提示原因。移出 Today 要依据 QueryService 给出的原因选择重排日期或 Someday，不能只清 todayFocus 造成无效动作。离开 Today 清除 evening；跨午夜的 evening 处理由同一服务规则统一执行并测试。

## 3. QueryService（FR-003/004/005）

所有开放视图先排除 deleted、终态和无效项目。有效 ACTIVE 要求任务 ACTIVE，所属项目未删除、OPEN、ACTIVE。

| 视图 | 判定（D 为本地今天） |
|---|---|
| Inbox | INBOX；录入明确计划后由命令转 ACTIVE |
| Today | 有效 ACTIVE 且 focus 或 start<=D 或 deadline<=D |
| Upcoming | 有效 ACTIVE 且 start>D；未来 deadline 提示另分组 |
| Anytime | 有效 ACTIVE 且 start 为空或 <=D；未来 deadline 不排除 |
| Someday | 任务 SOMEDAY 或有效 Someday 项目下开放任务；日期暂停 |
| Logbook | 未删除的 COMPLETED/CANCELLED，按终态时间倒序 |

测试至少 TC-003～013、043～044。相同日期使用稳定次序；Today 自定义排序与项目内排序分开。QueryContext 支持完成态、标签、归属过滤；搜索默认排除 Logbook，支持中文和英文不区分大小写。

## 4. ProjectService（FR-006）

完成项目默认不完成其任务，活动视图通过 JOIN 隐藏；恢复项目恢复可见性。删除项目事务内软删项目/Heading，任务迁往无归属并转 ACTIVE，保留日期；删除 Area 保留项目并清 areaId，直接任务解除 Area 归属。删除和完成的确认由 UI 展示影响数量，服务仍检查当前版本。

## 5. CalendarService（FR-010/011）

`CalendarEntry{taskId, displayOn, kind}` 的 kind 为 Start/Deadline/StartAndDeadline。start/deadline 同日合并；不同日可有两个引用。只显示有效活动任务，完成态由明确开关扩展。

moveCalendarEntry 必须指定要修改的字段；合并条目先由 UI 选择 Start 或 Deadline，服务拒绝含糊移动。新日期违反 start<=deadline 时拒绝并保留原值；不自动推动另一日期。MonthGrid 0-based 在边界转换，模型输出完整 QDate/ISO 日期，测试闰年、跨年和周首日。

## 6. Platform/Widget（FR-012/013）

平台方法返回实际能力/错误，不能仅设置一个 UI bool。原生窗口输入穿透与视觉 alpha 分开；托盘和主窗口共用同一 WidgetService 命令。托盘外部关闭穿透、屏外恢复和退出均为必须验收场景。

QLockFile + QLocalServer 用于本机单实例；仅本机 IPC，不提供网络服务器。第二实例不访问 DB；已有实例激活主窗后回复。托盘不可用时关闭退出且禁用穿透。

## 7. Recurrence/Reminder（FR-014/015）

固定周期滚动生成未来30天、历史补全最近30天，唯一键(templateId, occurrenceOn)，多次 materialize 幂等；每月31日和每年2/29缺失时跳过。完成后 N 天模式最多一个 OPEN 后继。

模板编辑只改明天及以后、未完成且未手动覆盖实例；历史保留。实例完成与后继创建使用同一事务或有测试证明的幂等重试。提醒 UTC 单次调度，终态不触发，通知失败保存应用内回退；错过提醒汇总，不把 OS API 接受请求当作用户已收到。

## 8. Backup/Import（FR-017）

一致快照使用经实际 Kit 验证的 VACUUM INTO 或 backup API；不直接复制 WAL 主文件。迁移备份失败停止。每日首次启动最多一次并保留最近7份。

导入先解析、版本/ID/日期/外键全部校验、预览，然后单事务写；失败零修改。替换模式双重确认且先备份；仅添加遇 ID 冲突跳过并报告。导出版本名和规范化样本由 M2-07 冻结，M7 实现往返验证。
