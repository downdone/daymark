# SYN：push、回执、pull与保留详细设计

网络格式以[v1协议](../../api/sync-v1.md)为准，业务命令以[DOM](workspace.md)为准。本模块只编排授权/版本/事务/日志，不再实现第二套树与循环规则。

## 1. 接口和依赖

```text
Push(ctx, Principal, DatasetID, ValidatedEnvelope) -> Receipt | Fault
GetReceipt(ctx, Principal, DatasetID, MutationID, DataEpoch) -> Receipt? | Fault
Pull(ctx, Principal, DatasetID, VerifiedCursor, PageBudget) -> PullPage | Fault
ReportProgress(ctx, Scope, InstallationID, AppliedSeq) -> void | Fault
Compact(ctx, AdminJobContext, DatasetID, RetentionCutoff) -> CompactReport | Fault
```

依赖UnitOfWork/SyncReader、WorkspacePolicy、CanonicalDigest、CursorCodec、Clock、受控PayloadResolver、资源预算和指标。用例不能持HTTP ResponseWriter，receipt成功后由适配器序列化；文件/网络I/O不在写事务中执行。

FrozenMutation={DatasetID,DataEpoch,MutationID,InstallationID,ClientSeq,Preconditions,Commands,RequestHash}；Receipt={MutationID,Epoch,Result,BatchSeq?,ObservedHead,EntityRevisions,StructureRevisions,CommittedAt}。禁止把本地rowVersion作为ExpectedRevision。

## 2. push算法及事务划分

### P0：锁外有限预处理

验证Principal及请求基本预算；inline做严格解析/摘要。payload_ref先以短读查询已有receipt：同原引用身份及外层摘要匹配可直接返回，不能先因文件过期失败；不匹配/无回执则向TRA申请有期限ReadLease，读取封存payload并验证摘要/归属/epoch/mutation。lease保持到写事务结束或期限到，清理不可删除被有效使用的文件。

### P1：单写事务

1. CheckSessionAndDataset，校验DataEpoch；ctx已取消则不开始业务。
2. FindReceipt。inline同RequestHash返回原回执；大组按规范展开摘要相同返回；不同摘要报mutation_payload_mismatch。该步骤发生在任何实体CAS之前。
3. 若用暂存，复查封存状态/lease/摘要/配额；metadata改变或失效拒绝，不使用预处理时的旧授权。
4. LoadGraph并执行RequiredReads闭包，核验客户端preconditions覆盖全部需CAS实体与结构。不存在的创建对象要求0，缺少precondition报read_set_incomplete；实际revision不同报revision_conflict/structure_conflict，零写入。
5. DOM.Plan，验证完整结果、关系及输入/输出资源预算。结果要能被服务端当前能力完整拉取；不支持batch_ref时，不能Commit客户端以后无法接收的大批次。
6. NoOp：写no_op receipt，batch_seq为空，head不动。其余ApplyPlan后增加head一次、编码完整不可变Batch、AppendBatchAndReceipt；read set中被改实体每组revision只增一次。若引用STAGED job，同时CAS该job至COMMITTED并记录receipt关联，不能业务成功但job仍显示未发布；PUBLISHING可仅为本事务内部状态。
7. Commit成功后返回receipt；释放ReadLease。普通业务错误回调返回error回滚，不能留下半个回执或失败占用的seq。

### P2：结果不确定

Write返回commit_unknown时，在独立只读连接/请求预算内查询原mutation；找到相同摘要回执即返回成功，找不到不能断言未提交。返回503 commit_result_unknown，客户端同ID查/重传。**SYN不在内部换ID或盲目再执行领域命令**，context超时也按此规则处理。

若HTTP序列化/写响应失败发生在Commit后，记录安全传输错误；数据库不得回滚已经成功的事务，客户端靠receipt恢复。重放本身不产生第二批次/第二后继/第二分组。

## 3. pull精确算法

在同一Store.Read回调内：验证账号/资料及epoch→读取retained_after/head→校验cursor After/Through范围→tail时取through=head，bounded时沿用原through→按seq读取`after < seq <= through`的完整不可变批次，按条数和字节预算分页。元数据和批次必须来自同一个SQLite读快照。

ReadBatches不得返回半个Batch。首个批次超页预算时，支持的batch_ref包含seq、总长、摘要、scope/epoch保护的读取引用；先完整下载再apply。无此能力则返回明确限制错误，不给跳过该seq的next_cursor。batch_ref底层日志保留需短期读pin/lease，或导出不可变受租期保护副本，不能给引用后立刻压缩源日志。

next cursor：仍有批次则After=最后完整seq、Through固定；本页完成through则After=through、Through为空进入tail。空数据且after==through合法，仍发tail cursor；下一轮看到新head。客户端不得以ObservedHead代替真正应用的seq。

传输包装每个不可变batch为DeliveredBatch={batch,after_cursor}，after_cursor只覆盖该完整批次；末项等于next_cursor。中间项保持本轮through，末项完成全轮时发tail。这样Qt逐批Commit不会提前保存整页末尾检查点；空页单独CAS游标、不改变applied。checkpoint属于传输，不写入不可变日志摘要。

读取seq缺号（在retained floor之后）或payload摘要错误属于存储一致性故障，ready写保护/告警，不当成普通空页。复制日志压缩只删连续前缀，因此合法未压缩区间必须连续。

## 4. receipt、进度与压缩

GetReceipt每次限定owner/dataset/epoch；未知mutation返回not_found，跨账号不泄漏差异。receipt成功内容不可变；仅可增加压缩标记/安全传输元数据，不能重新计算“最新实体版本”覆盖原确认。

ReportProgress只接受同资料已认证会话及`0<=applied<=head`，同epoch单调更新该installation的最大值；它是设备声明，不是授权和服务器替设备已应用的证明。未报告不自动清其业务，安装ID也不能绕过会话撤销。

Compact持writer事务重新检查所有活跃pin、保留窗口及待发布job。设`candidate`为满足年龄窗口的最大连续seq，`pinnedFloor`为所有未过期pin.after最小值，删除上限=min(candidate,pinnedFloor)；无pin则只受candidate限制。删除`seq<=limit`和更新retained_after同事务；receipt和用户软删事实不随日志删除。查到作业heartbeat进行中的pin不能先按旧Clock快照判过期，使用事务内统一Now。

初期保留本epoch最小receipt记录，不自行按90天删除。需要回收receipt必须新增可拒绝旧请求的代次/序号契约，再经迁移和兼容测试；不能为了“清日志”降低幂等保证。

## 5. 错误、取消及并发线性化点

| 情况 | 机器码/行为 | 事务/后续 |
|---|---|---|
| Scope或会话无效 | auth_expired/device_revoked/not_found | Check前后零业务写，保留客户端队列 |
| epoch不同 | epoch_changed | 无新receipt，转安全重建 |
| 已成功相同mutation | 原成功receipt | 不再检查已变化的实体base |
| 同ID不同payload | mutation_payload_mismatch | 无业务修改，客户端修复不得改原ID |
| precondition遗漏/版本竞争 | read_set_incomplete/revision_conflict/structure_conflict | 整组回滚；客户端补基线后新mutation |
| before Begin/排队时取消 | cancelled/busy | 不执行Plan |
| Commit前可确定回滚 | 暂时错误/业务错误 | 原mutation可按类别处理 |
| Commit阶段不明 | commit_result_unknown | 独立查receipt，不声称肯定未写入 |

同mutation并发线性化于writer中第一次成功receipt Commit；另一请求等待后返回相同结果。不同设备同base并发：最多一个版本CAS成功，另一个冲突。取消不能在已Commit后倒扣head或删除receipt。

## 6. 单元测试契约

| 用例ID | 输入/屏障 | 必须断言 |
|---|---|---|
| UT-SYN-01 | 相同mutation重复/同ID不同摘要 | 前者只Apply一次且原receipt；后者零Apply |
| UT-SYN-02 | 首次成功后实体已再改 | 重试仍原receipt，不因旧base报冲突 |
| UT-SYN-03 | 最后组件/日志/receipt写失败 | 整组回滚，head不发布，错误不吞 |
| UT-SYN-04 | Commit返回未知，查receipt有/无/查询失败 | 有则成功，其余unknown，不换ID/不重跑Plan |
| UT-SYN-05 | 缺少实体/结构precondition或并发成员 | 精确拒绝，无部分CAS应用 |
| UT-SYN-06 | NoOp、多个命令改同实体 | no_op不增head，真实变化revision仅一次 |
| UT-SYN-07 | tail捕获head后新增事务、多页/空页 | through稳定，完成后新tail读得到新增 |
| UT-SYN-08 | 首批超预算/引用后日志准备清理 | 完整Batch或受保护ref，无半批次/跳序 |
| UT-SYN-09 | 过期cursor、缺号、摘要错、越权 | 分别恢复错误/一致性故障/拒绝，游标不跳 |
| UT-SYN-10 | pin创建/续租与Compact竞争 | 删除limit不跨有效pin，floor和删除同事务 |
| UT-SYN-11 | 大组已提交后块过期、同语义新ref | 成功receipt仍可确认，摘要不受传输引用变化影响 |
| UT-SYN-12 | client_seq较小但未提交的独立离线命令 | 不仅凭seq丢弃；按真实base/receipt决定 |

这些UT用SpyUoW证明编排；INT-06～09必须以真实SQLite、真实HTTP和可控并发验证上述线性化，尤其不能Mock Commit成功充当可靠性证据。
