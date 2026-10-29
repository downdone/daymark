# DOM：业务命令与领域策略详细设计

基线为FR-024～042、[统一任务关系](../task-list-context.md)和当前C++服务，不恢复旧项目/领域/独立事件概念。服务端复核数据不变量，Today等视图仍由C++ QueryService决定。共享fixture不一致时先裁决契约，不以Go实现反向改写产品行为。

## 1. 纯策略接口及Plan

```text
WorkspacePolicy.RequiredReads(commands, initialGraph) -> ReadRequirements | Fault
WorkspacePolicy.Plan(graph, commands, EventContext) -> Plan | Fault
ValidateTask/ValidateCategory/ValidateRule(candidate, graph) -> normalizedValue | Fault
GenerateFixed(rule, dateRange, graph) -> []TaskCandidate | Fault
GenerateSuccessor(rule, completedTask, graph) -> TaskCandidate? | Fault
Plan = {InitialReadSet, ExpectedStructures, EntityBeforeAfter, RelationReplacements,
        StructureChanges, OrderingChanges, AliasChanges, SuccessorEdges, NoOp}
EventContext = {ReceivedAtUTC, PerCommandEventAtUTC}
```

输入图为不可变快照；Plan不执行SQL/随机数/网络。先解析命令需要的闭包（祖先/后代、分类成员、标签成员、循环成员），Store在写事务加载它；RequiredReads发现缺少数据返回“需要哪些键”，用例有界补读，不能悄悄把缺少节点当不存在。命令组内部顺序有意义：可以先创建分类再创建任务；preconditions针对整个组开始时的状态，不是每一步的中间revision。

Plan按命令顺序计算完整候选图，最终统一校验并合并同一实体变化；同组同实体revision只增一次。before==after则不增版本、不造批次。影响实体超过预算时整组拒绝或走大组暂存；不能先应用前512条。

## 2. 业务实体与聚合字段

| 值 | 可编辑事实 | 校验/只读事实 |
|---|---|---|
| Task | title/notes、category/parent、start/deadline、schedule/reminder、flags、tags/links | id/created/status及恢复分组经专用命令；revision只读 |
| TaskSchedule | allDay + 日期起止，或UTC起止/IANA zone | 两种形式互斥，结束严格晚于开始；全天结束排他 |
| ReminderConfig | ABSOLUTE(anchorUTC,repeat,every,zone)或RELATIVE(minutesBefore,zone) | 执行next_due/last_fired/history不属于服务器事实 |
| Category | title/description/color/archived、parent经relocate | 一二级；没有可完成状态或日程 |
| Tag | title/color | 平级；允许重名ID不同，不能按标题自动合并 |
| RecurrenceRule | 新代次ID、source、frequency/interval/anchor/end/zone及typed defaults | 模式/规则更换用replace，新代次保持旧历史 |
| OrderingScope | kind/subject及稳定成员排序 | 独立revision；QueryService决定哪些行当前可见 |

Task标题trim后1～512 Unicode标量值；notes空串合法，clear/null不能替代必填字符串。日期限公历1～9999年、真实月日；start<=deadline。plan状态仅保留INBOX/ACTIVE/SOMEDAY兼容值；安排日期/日程/提醒使新INBOX候选变ACTIVE。evening/today_focus为兼容事实，由客户端统一命令显式产生，服务器不按自己的“今天”重算。

links是有方向的集合，role只能preparation/followup，同任务自连/同target重复拒绝；目标必须同资料存在。tag_ids不重复、目标未删除。父子分类一致，日期/提醒/flags/tags不隐式继承。旧深层树允许现有节点不加深地编辑/移出，新建关系不能制造第三层或环。

Reminder.repeat只允许NONE/DAILY/WEEKLY/MONTHLY/YEARLY，every=1～36500；RELATIVE要求schedule存在、minutes为0/5/15/30/60/1440，repeat=NONE。现有TaskSchedule.alertMinutes映射到此唯一提醒配置，不再在schedule和reminder重复保存。全天相对提醒按配置zone的当天09:00减偏移；定时按startUTC减偏移，绝不使用服务器时区。

本地操作仍可以拒绝过去提醒，但**离线上传不得仅因已过触发时间被拒绝**。服务器保存用户规则，客户端决定错过提醒/投递历史。事件瞬间使用离线命令捕获的EventAt，ReceivedAt只作服务审计/收件时间，设备时钟不决定冲突胜负。

## 3. 命令参数、read set及结果

所有ID/日期/瞬间按协议格式；各行的expected实体/结构版本均放envelope.preconditions，不能同一字段在body写第二份版本。Create ID已被soft-delete或alias保留时也不是“不存在”。

| 命令 | body必需/可选字段 | 读取/保护集合 | 原子结果 |
|---|---|---|---|
| task.create | task_id,title,event_at；可选初始TaskPatch | id不存在；引用分类/父/标签/links；目标task_tree/category_members | OPEN任务及组件；parent存在则继承其分类 |
| task.patch | task_id,patch,event_at；patch仅合法Task字段 | 当前task、改动引用、旧/新父树和归属集合 | 候选合法后整聚合保存；父/分类改变按move_tree语义 |
| task.change_state | task_ids[],action,event_at,group_id | 去重根集合、全部联动成员；task_tree/recurrence_members | 完成/重开/软删/恢复/取消；规则见下节 |
| task.move_tree | task_id,category_id可null,parent_task_id可null,event_at | 源/目标树、分类/成员集合、全部未删除后代 | 全部相关成员移属，必要时脱离原父 |
| category.save | category_id,title,description,color_key,archived,event_at；新建时可选parent_id | 新建不存在或当前及父；category_tree | 内容/归档更新，任务引用保留 |
| category.relocate | category_id,target_id,placement(before/after/inside),event_at | 源/目标分类、子分类及两个同级排序集合 | 改父级及排序；有子分类的一级不能直接降为二级 |
| category.remove | category_id,event_at | category_tree、category_members及所有直接引用任务 | 分类软删；直接任务移收集箱，子分类提升一级，任务不删除 |
| tag.save | tag_id,title,color_key,event_at | 不存在或当前tag | 保存标签 |
| tag.remove | tag_id,event_at | tag、tag_members及所有带该tag的任务 | 标签软删、移除关联，任务保留 |
| recurrence.replace | source_task_id,new_rule_id,rule,cutoff_on,event_at | 源任务、旧规则及其成员、recurrence_members | 旧规则停用，新代次建立，符合条件的旧未来实例软删 |
| recurrence.stop | rule_id,remove_future,cutoff_on,event_at | 规则及其成员结构 | 停用；按选项软删未完成且未覆盖未来实例 |
| recurrence.materialize | rule_id,from_on,through_on,event_at | 当前规则、已有周期键、父任务/分类等seed引用 | 有界日期范围内补齐真实实例，已有覆盖/终态不重建 |
| ordering.replace | scope_id,kind,subject_id,member_ids[],ordered_ids[],event_at | 该ordering_scope版本、listed成员存在/归属 | 对指定子集重排，未列成员位置关系保留 |

TaskPatch可写：title、notes、planning_state、category_id、parent_task_id、start_on、deadline_on、today_focus、evening、pinned、important、schedule、reminder、tag_ids、links。Set schedule/日期时沿用现有synchronizeTime的统一起止关系；客户端发已规范化的完整端点变更，服务器检查日期与schedule本地起止一致，不能自行猜用户想移动哪一个端点。

Category/Tag颜色仅neutral/blue/green/orange/purple；标题同512标量上限。category.save不能暗改parent；relocate的inside只能放一级目标下，before/after继承目标parent。被删分类任务包含已完成/已软删兼容引用，也要安全解属，不能留下恢复后非法引用。

Create默认notes空、status OPEN、flags false、日期/组件/归属空、created/updated=event_at；提供父项/分类/计划则按校验规则归一化。category.save仅创建时可以设置parent，更新时若parent变化必须用relocate。业务事件时间由客户端捕获，服务器审计收件时间单独记录，不在重试时把已完成任务的时间改成重试时刻。

Task/Category完整结果DTO带可空legacy_metadata（SQL列legacy_json），未设置时显式null。LegacyMetadataV1仅由已校验的本地JSON1～9兼容导入产生，带format_version和source_export_version，保留历史只读字段的规范JSON；普通patch不允许写该字段。接收端必须识别该兼容版本并验证对应导入字段，不能执行其中内容或将其当当前查询规则；不能映射的旧对象返回legacy_format_unsupported并保留原资料/导出，不默默删字段后称完整同步。兼容数据的往返保留是DOM/import测试的独立oracle。

## 4. 状态命令精确规则

先对原始task_ids全部CAS、去重/按祖先优先归约，再对图执行；不能先变父项导致被选子项版本变化，再用旧版本误报内部冲突。

| action | 成员与字段变换 |
|---|---|
| complete | 根及未完成、未删除后代→COMPLETED，completed=event_at、cancelled=null、completion_group=group_id；此前独立完成项不改组 |
| reopen | 根→OPEN并清终态；后代只打开completion_group等于根原组的成员；原组为空时只开根，不把所有NULL组当同次完成 |
| delete | 根及当前未删除后代写deleted/event_at和deletion_group；已独立删除项不改；循环实例标instance_overridden |
| restore | 根及deletion_group等于根原组的后代清删除；独立恢复子项而父仍删时detach为根；已删分类清归属 |
| cancel | 目标本身→CANCELLED，cancelled=event_at、completed/group=null；不新增父子自动取消规则 |

已处于同一终态的单项命令可no_op；用户选择中的重复ID拒绝，包含父子重叠按原始集合预检后只处理一次。group_id为本地该用户动作预先分配的稳定UUID，不用相同时间戳推断组；新动作不能冒用其他未关联操作的组ID。后继OPEN实例存在时重开AFTER_DAYS旧实例返回recurrence_open_instance，不制造两个OPEN后继。

## 5. 结构版本的写集合

| StructureKind / ID | 何时增加 |
|---|---|
| task_tree / 当前根ID | 新增/移出/移入后代、状态联动/分组变化；普通独立标题修改由实体revision保护即可 |
| category_tree / datasetID | 分类新增/删除/移层/归档等影响结构的操作 |
| category_members / 分类ID；无归属用固定nil UUID | 任务创建/归属移动/影响成员可用性的删除恢复 |
| tag_members / tagID | 挂上/移除该tag，包括标签删除批操作 |
| recurrence_members / ruleID | 物化/终态/删除恢复/覆盖标记/规则替换等影响循环集合 |
| ordering_scope / scopeID | 对应排序最终有变化，或其成员变化需要调整排序表 |

结构不存在时expected=0，首次变更建为1；不能以空集合无需版本为由忽略并发新增。旧根被移走后其版本记录保留，防止晚到命令把新树当旧树。RequiredReads返回遗漏结构/实体需求；客户端没有相应基线时先拉取或阻塞该命令，服务器不能接受漏列以“尽力应用”。

## 6. 循环算法与稳定身份

固定namespace=`43e1f2a4-8d21-4a8c-bb70-16b813d3f5ab`，UUIDv5输入为`rule_id + ":" + occurrence_key`的UTF-8；不包含server/dataset，保证未登录本地生成后首次登录不用改身份。固定周期键`D:YYYY-MM-DD`；完成后后继键`A:<predecessorTaskID>`。源任务已存在时规则anchor对应实例保留source_task_id，作为明确canonical例外；其他新实例用派生UUID。历史不同ID通过alias/import对账处理，不批量重写主键。

FIXED：DAILY按anchor日差%interval，WEEKLY以anchor所在周一及weekdays_mask（周一bit0），MONTHLY/YEARLY按合法固定月日跳过缺失日期。end为包含端点，发生DST不存在/歧义本地时刻时遵循当前Reject策略跳过，不挪到下一小时。时区从规则/日程读取，不用服务器zone。范围最大366天/每次512候选为设计初值，超限分多个**独立生成组**，同一周期唯一键保证幂等。

rule.defaults为有版本的typed Template：title/notes、planning、category/parent、flags、tag_ids/links、相对日期偏移、schedule模板、ReminderConfig；禁止包含Task ID/status/版本/任意SQL或嵌套repeat。父已终态/删除时跳过子实例，分类删除清归属，已删标签移除，日期与提醒按日历日/DST计算。已有终态/手工覆盖/用户删除实例不恢复；仅明确的自动失效且instance_overridden=false实例可按规则复建同ID。

replace的cutoff_on由客户端根据该用户动作捕获，服务器不拿当前日期改写离线动作；CAS/read set保护当时目标。只软删cutoff及以后OPEN且未覆盖的旧实例，历史/已完成保持。改规则产生新ID，旧规则保留inactive。

AFTER_COMPLETION：完成前驱时在同一Plan创建至多一个后继。日期=前驱completed instant转规则zone的日历日+interval；新后继键A:前驱ID，edges表唯一前驱/后继。两端完成时间不同靠CAS冲突，不能合并成两个后继。旧日期型occurrence_on保留兼容，不当不可变周期身份。批次中所有rule/instance/edge一并Commit，取消/回滚不留下后继。

## 7. 排序算法

ordered_ids必须是member_ids的无重复排列；成员必须存在且属于声明scope。读取完整当前order向量，缺少的listed成员先按ID稳定追加；找到所有listed成员原占槽位，将ordered_ids依次填回这些槽，未listed成员相对次序保持。最终重编号0..N-1，只增scope revision，不把每条任务title revision都增加。SQL可在同一事务先删除该scope条目再完整插入，避免中间唯一position冲突；失败整体回滚。

today scope只是共享的稳定排序事实；服务器不按今天过滤成员。Qt通过QueryService生成实际可操作子集，服务端验证该资料成员身份及scope类型，显示时仍按本机日期投影。原有未显示/完成成员可保留位置，恢复不会凭空丢顺序。

## 8. 领域单元测试契约

| 用例ID | 输入 | 必须断言 |
|---|---|---|
| UT-DOM-01 | 512/513标量标题、补充平面字、空/非法日期 | 正确计数/拒绝，输入图不变 |
| UT-DOM-02 | Unspecified/Clear/Set及只读字段 | 仅允许字段变更，Clear可空性正确 |
| UT-DOM-03 | task.create引用跨资料/删除tag/self link | 拒绝；无半个新task/关系Plan |
| UT-DOM-04 | 父完成，子一项已独立完成 | 只本次成员标组；reopen保留此前完成 |
| UT-DOM-05 | 父删除/恢复，子先前已删；单独恢复子 | 组恢复精确，子detach正确 |
| UT-DOM-06 | 移父/移子、第三层、旧深层不加深编辑 | 全树归属一致，新深度拒绝，兼容旧资料不丢 |
| UT-DOM-07 | 分类删除有子分类/完成任务、tag删除多任务 | 迁出/提升/去关联集合完整；版本写集合正确 |
| UT-DOM-08 | 预检后并发新增子项/分类成员 | RequiredReads/结构校验捕获，不仅检查旧行 |
| UT-DOM-09 | 月31日/闰日/周掩码/跨DST固定实例 | 跳过规则及稳定ID正确，重复物化NoOp |
| UT-DOM-10 | AFTER_DAYS跨时区完成/双端不同完成时间/重开 | 一个确定后继；竞争冲突，禁止第二OPEN |
| UT-DOM-11 | 旧随机实例/source例外/已手工覆盖实例 | identity映射稳定，不覆盖用户内容 |
| UT-DOM-12 | RELATIVE/ABSOLUTE、过期离线上传 | 唯一配置，服务器不改本机投递进度/不因已过期拒传 |
| UT-DOM-13 | 部分排序含隐藏项、新成员、重复ID | 槽位算法结果精确、scope只增一次版本 |
| UT-DOM-14 | 同组先create后patch、重复改同实体、最后命令失败 | 最终revision一次、失败无Plan、无顺序依赖漏检 |

纯策略UT使用小型显式Graph及独立期望图，不调用实现生成expected。再以INT-14/15证明与C++规则及真实SQL相符。
