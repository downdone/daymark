# wire、领域、存储和Qt字段映射

以[OpenAPI](../../api/sync-v1.openapi.json)为wire结构、[共享类型](go/model.go)为领域值、[两套DDL](../sync-modules/README.md)为存储设计。下面的跨字段校验由严格codec+领域执行，JSON Schema不能自动证明所有业务不变量。

## 1. 通用边界

| wire | Go候选/领域 | 服务端SQLite | Qt客户端 |
|---|---|---|---|
| 小写规范UUID | ID；实体非nil，结构空分类可nil UUID | TEXT，同dataset复合FK | QUuid；出站小写无花括号 |
| revision/seq十进制字符串 | Revision/Seq int64 | INTEGER>=0，实体>=1 | ServerRevision/ServerSeq包装；不与rowVersion/local_seq混用 |
| YYYY-MM-DD | 已验证Date | TEXT，无时区 | QDate，不经UTC转换 |
| 毫秒UTC-Z | Instant Unix ms | INTEGER *_ms | UTC QDateTime；不得把无zone本地文本当UTC |
| IANA zone | Zone，加载有效tzdb | TEXT | QTimeZone验证；不得回退服务器zone |
| hash 64小写hex | Digest [32]byte | BLOB length32 | QByteArray length32，传输时toHex |
| null完整组件/引用 | nil / *T | NULL或无组件子表行 | std::optional；空QString不是任意null替身 |
| metadata | wire-only | 不存业务表 | 仅允许忽略的诊断，不回写 |

所有FK受dataset约束。SQL保基础非空/唯一/引用，domain补树深/环、时区、Unicode trim/上限、状态等。字段校验错误field_path指向实际wire路径，例如`/commands/0/patch/deadline_on`。命令引用同组刚创建对象时，以组开始precondition=0及候选图解析，不要求先提交单独create。

### 1.1 规范输入和默认值

hash针对经过类型/格式验证的**命令输入**，不针对domain执行后归一化的结果。正文不做NFC；title的trim是业务结果规范化，不在hash前偷偷改文本。task.create.initial缺省唯一补为{}，使它与显式空初始值等价；其他可选patch/CategorySave.parent_id保留存在性，不把缺失补null。日期/瞬间必须已是规定格式，revision字符串不转JSON number。

集合排序穷举为：preconditions.entities按type/id，structures按kind/id；tag_ids按ID，links按target_id/role；task.change_state.task_ids与ordering.member_ids按ID。所有这些集合重复拒绝；commands和ordered_ids的用户顺序保留。template中的tags/links沿同一规则。引用payload展开后才编码，不把job ID写进语义摘要。

IMPORT使用独立的规范ImportMutation，不伪装13类普通命令：对象含protocol_version、kind固定import、dataset_id/data_epoch/mutation_id/installation_id/client_seq、source_export_version及payload（dto_version/base_head_seq/entities/aliases/successor_edges）。entities按type/id，alias按alias_id，edges按rule_id/predecessor_id排序；实体内部集合依同一规则，源revision提示值保留。采用同一类型字节编码后SHA256，raw payload_sha256仍单独校验实际上传字节；两者用途不混淆。正式freeze前用跨Go/Qt黄金fixture补全此编码和缺省归一化测试。

## 2. Task完整聚合

| wire字段 | Go Task | SQL | Qt映射/校验 |
|---|---|---|---|
| id、外层change.revision | ID/Revision | tasks.id/revision | 业务ID/server base；现有Task.rowVersion仍本机并发版本 |
| title/notes | Title/Notes | tasks.title/notes | title.trim后1..512 Unicode标量；notes允许空串 |
| planning_state/status | PlanningState/Status | 同名列 | INBOX/ACTIVE/SOMEDAY与OPEN/COMPLETED/CANCELLED；Today不是状态 |
| today_focus/evening/pinned/important | Flags | 对应0/1列 | 复制事实；不让服务器按自己的日期重算 |
| category_id/parent_task_id | *ID | 复合FK | 父子同分类；移动整树；已删引用解属规则见DOM |
| start_on/deadline_on | *Date | TEXT可空 | 端点一致，start<=deadline；无schedule时可独立为空 |
| created_at_utc/updated_at_utc | CreatedAt/UpdatedAt | created_ms/updated_ms | 来自捕获event_at；服务接收时刻另记审计，不作为冲突赢家 |
| completed/cancelled/deleted_at_utc | 可空Instant | *_ms | OPEN无终态时间；COMPLETED只completed；CANCELLED只cancelled；软删独立于status |
| completion/deletion_group_id | 可空ID | 对应列 | 动作预分配稳定ID；不能按相同时间或NULL组联动 |
| schedule | Schedule union或nil | task_schedules 0或1行 | ALL_DAY/TIMED互斥；alertMinutes移到唯一reminder配置 |
| reminder | Reminder union或nil | reminder_configs 0或1行 | 配置参与同步；next_due/last_fired/history在本机execution |
| tag_ids/links | []ID/[]TaskLink | task_tags/task_links完整集合 | []明确空；不可重复/自连；角色preparation/followup；所有目标同资料存在 |
| recurrence_id/occurrence_key | *ID/*string | 复合FK+唯一key | 二者同空或同非空；固定D:date、后继A:前驱ID |
| occurrence_on | *Date | occurrence_on | Qt以sync_occurrence_on存实际日期；旧occurrence_on保留本机历史唯一槽。同日多个后继的真实日期相同，身份由不可变key决定。|
| instance_overridden | bool | 0/1 | 手工改/删实例保护；不因重物化清除 |
| legacy_metadata | *LegacyMetadata | legacy_json | 仅兼容导入写，当前patch只读；不执行、不作查询语义 |

change.id必须等于value.id；change.type必须等于value具体类型；change.revision是唯一server revision，value不另带第二份revision。同组实体最终只写一次版本；create必须不存在（软删/alias仍算存在）。完整replace保留软删对象，不用缺失代替删除。

### 2.1 日期与日程唯一关系

依据现有`src/services/TaskTime.cpp`的withUnifiedDates/synchronizeTime：

- ALL_DAY：start_on=schedule.start_on，deadline_on=schedule.end_on减1日；schedule结束日排他。2030-01-01至2030-01-02代表一天，任务两端均2030-01-01。
- TIMED：start_on与deadline_on分别等于start/end UTC瞬间转换到**schedule.time_zone**后的日历日期。结束刚好午夜依现有规则取该结束日期，不自行减1日。
- 拖起点保持跨度、改终点、清日期导致移除schedule等用户意图先由现有TaskService统一规范化；collector发送完整改动端点。服务器验证候选最终关系，不猜移动哪个端点。显式清schedule后可保留日期；RELATIVE reminder随schedule清除，或同命令替换为合法ABSOLUTE，不能留悬空相对提醒。
- 相对提醒定时锚点=startUTC；全天锚点=配置zone当天09:00，再减minutes_before。该zone随配置同步；旧代码使用系统zone的来源迁移时捕获，不在其他设备重新按其系统zone解释。
- 所有DST本地→瞬间使用Reject策略，歧义/不存在不自动挪时。过期离线上传可接受配置；执行/补发政策仍由客户端负责。

## 3. Category、Tag、Rule、Ordering

| 聚合 | 精确映射/约束 |
|---|---|
| Category | parent_id/title/description/color_key/archived、created/updated/deleted→categories同义列；legacy_metadata→legacy_json。颜色固定5值。只允许两层新关系；旧深层不加深编辑/移出可兼容。archived不是完成；remove移出任务、提升子分类，不删任务。|
| Tag | id/title/color及时间→tags；平级可重名但不按名合并。remove移除所有关联、保留任务；结构版本保护并发新增关联。|
| RecurrenceRule | id/source_task_id、rule.*、active及时间→recurrence_rules；defaults→defaults_json。SQL遗留title/notes为defaults.title/notes的冗余提取，写入必须相等，读取不另形成一份权威。|
| TaskTemplate | template_version=1；title/notes/planning/flags、category/parent、日期偏移、typed schedule/reminder、tags/links。无id/status/revision/legacy/嵌套repeat。偏移相对occurrence日历日计算，范围±36500日。|
| ScheduleTemplate | ALL_DAY仅两个日期偏移且end>start；TIMED另有本地时刻/zone，按相对日与墙钟分别解UTC，end瞬间>start。不能把固定UTC间隔冒充跨DST相同本地时刻。|
| ReminderTemplate | ABSOLUTE为日偏移+本地时刻/zone，再变配置anchor；RELATIVE同任务配置。独立重复允许NONE/DAILY/WEEKLY/MONTHLY/YEARLY；不反向生成任务周期。|
| OrderingScope | id/kind/subject_id/ordered_ids→ordering_scopes+order_entries；revision来自change。scope与structure ordering_scope使用同一个计数，不能分别递增。无界浮点rank不入协议。|

RuleSpec模式为AFTER_COMPLETION iff frequency=AFTER_DAYS，其余FIXED。WEEKLY mask非0且仅7bit；其他frequency mask=0。MONTHLY/YEARLY month_day确定目标日；YEARLY year_month确定月。其余不适用month_day/year_month固定为anchor的日/月，避免同一规则有多种无意义编码；anchor的起点/有效日期按DOM。end包含端点且>=anchor。每次物化范围<=366天、候选<=512，超限显式分组，不拆关联事务。

冲突恢复可以显式保存同一已软删Category/Tag身份，保留原创建时间、清除删除标记，但不隐式恢复此前移除的成员关系；关系由同组后续命令决定。recurrence.replace可恢复同一source及完全相同RuleSpec的旧generation，改变规则仍须新ID；恢复与重复恢复分别是显式生命周期转换和no_op。服务端确认的完整实例时间覆盖客户端预生成时间，本机rowVersion仍独立递增。

Ordering kind/subject：category_siblings为父category（顶层null）；task_siblings为父task（根任务null，实际范围由scope ID固定）；today为null。scope ID由初次创建后持久保存并随快照复制，不按本机日期生成。成员类型category_siblings只能category，其余只能task；member_ids与ordered_ids为同一无重复集合。子集排序保留隐藏成员槽位，完整算法见DOM。同一个scope ID不能改kind/subject。

## 4. 作业、快照和大批次

| DTO | 规则 |
|---|---|
| Job | owner只由认证确定；public DTO不暴露lease_owner/文件路径/operation hash。dataset/epoch对SNAPSHOT/STAGED/IMPORT/BATCH非空，EXPORT可空；expires_at为产物期限，不是worker lease。|
| Chunk | index从0连续；内容为确定的UTF-8 JSON payload连续字节切分，单块<=256KiB，不要求每块单独合法JSON；不做平台换行转换。byte_count为原始字节，hash为SHA256该块。|
| Seal | total_bytes为各块原始字节之和，payload_sha256为按index串接后的SHA256；hash不是对每块hash再hash。JSON完整解析再校验专属payload（InlinePayload/ImportPayload）；压缩默认不接受，若启用另外限制解压后预算。|
| Manifest | manifest_sha256为规范descriptor的SHA256：只含protocol_version/job_id/dataset_id/data_epoch/kind/dto_version/cutoff_seq/entity_count/total_bytes/chunks/payload_sha256。排除自身hash、expires_at、resume_cursor及metadata；采用同一版本化类型编码。产物固定、租期变化不使内容摘要变化。|
| SnapshotPayload | entities为完整Change格式、含server revisions，及structures/aliases/successors；cutoff从同一读视图取得；与manifest的scope/cutoff/计数相符。空库也输出一个合法非空JSON块。|
| ImportPayload | entities承载源完整事实；源revision仅对账提示，不能直接写server revision；base_head由create和seal内payload一致约束。scope来自作业，不允许payload指定owner。|
| BatchRef | seq与引用内完整ChangeBatch.seq一致；job为只读BATCH，数据scope非空。服务按dataset/epoch/seq幂等生成immutable副本并封存再返回引用；未READY不能发悬空ref。|
| Export | 单账号业务包按dataset分别带各自scope/epoch/cutoff；不同dataset之间不承诺同一业务事务。外层ExportBundle包含格式版本及有序SnapshotPayload列表，manifest dataset/epoch/cutoff/resume_cursor为null，不泄漏users/sessions/credential/execution。|

BATCH作业只允许BUILDING→READY或FAILED/EXPIRED，用户不能上传/改其内容；生成副本期间保守pin源日志，READY后副本由自己的有限租期/ReadLease保护，释放源pin。客户端完整验证并暂存后才应用其after_cursor；若下载到期410，保留当前cursor重新pull，不能用失效引用推进。

Job的operation key按owner/dataset/kind/operation_id组合，stored input hash防同key不同参数。publish中的operation_id必须等于创建作业时的值，不是再次创建另一个动作。客户端可续有效租期但不能复活EXPIRED；PUBLISHING只允许查询、不可cancel。GET chunk依权限制定路径，不接受文件名/目录。

## 5. 客户端持久化与生命周期

| 数据 | 生命周期及原子边界 |
|---|---|
| sync_profiles | 访客所有远端绑定为空；账号绑定固定owner/dataset。signOut保留绑定、增generation、停网络、移除凭据。明确切新访客资料才生成新profile，不能把A数据标成B。|
| sync_cursor/base/structures | 只含当前epoch的已落远端基线；初始来自已校验快照。大批次暂存完成后一次应用；表内完整事实仍有现有业务表用于UI。|
| sync_outbox | 本机动作同业务Tx捕获意图/actual writes，local_seq单调。首次发送前冻结全部字节/摘要/前置条件；之后只能变状态/重试时间等控制字段。|
| sync_dependencies/dirty_entities | 同profile依赖必须指向更小local_seq；脏实体保存捕获rowVersion，以区分发送后的再次编辑。集合成员变化也记录结构依赖。|
| sync_ack | 保留成功身份/所需revision直到自己echo已落；不单靠ACKNOWLEDGED状态释放所有依赖。尚未知完整结果的base不提前更新。|
| sync_conflicts | base可null（创建），remote可null（不存在），local必须有；远端soft-delete仍是完整对象。解决决策+新mutation及状态同Commit，原冻结请求不改。|
| sync_incoming* | 同profile/current epoch，chunks验证总hash才VERIFIED；partial不更新UI/cursor。空间耗尽保留原资料并暂停。|
| reminder_execution | 当前profile业务库内仅本机投递游标，fingerprint明确配置版本；远端配置变化重算计划，不传已展示记录，不入普通outbox。|

epoch变化先一致备份/归档旧base/cursor及待传事实，再清当前epoch FK子表、变epoch、把旧outbox标RECONCILE_REQUIRED，最后安全快照对账。不能cascade删掉旧待传意图/冲突。账号绑定后登出仍捕获离线修改；纯guest在首次合并cutoff前不积累无限网络历史。资料切换必须通过保存门/DB worker，迟到网络回调在GUI及worker两处核验generation。
