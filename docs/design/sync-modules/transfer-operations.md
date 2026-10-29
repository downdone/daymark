# TRA/OPS：传输作业、备份与恢复详细设计

依赖[FND/STO](foundation-storage.md)、[SYN](replication.md)。作业不改变普通push的提交语义：登记job/上传chunk不等于业务已生效。所有用户作业限定owner/dataset/epoch，系统BACKUP作业由内部AdminJobContext运行。

## 1. 端口与数据结构

```text
CreateJob(ctx, Scope, {Kind, OperationKey, Metadata, ByteBudget}) -> Job
PutChunk(ctx, Scope, JobID, Index, Bytes, SHA256) -> ChunkAck
Seal(ctx, Scope, JobID, Manifest) -> SealedPayload | Fault
PublishImport(ctx, Scope, JobID) -> Receipt | Fault
RequestSnapshot(ctx, Scope, OperationKey) -> Job
GetManifest/GetChunk(ctx, Scope, JobID, Index?) -> immutable value | Fault
AcquireReadLease(ctx, Scope, JobID) -> PayloadReadLease
Renew/Cancel(ctx, Scope, JobID, Attempt) -> Job | Fault
ArtifactStore.PutAtomic(key, reader, limit, expectedHash) -> {Bytes,Hash}
ArtifactStore.Open(key)/Remove(key)/Stat(key) -> value | Fault
JobRunner.Claim(kind, now, leaseOwner) -> {Job,Attempt,Fence}?
```

Job={ID,Owner,Dataset?,Epoch?,Kind,State,OperationKey,Attempt,LeaseOwner,LeaseUntil,ExpiresAt,BytesReserved,Metadata,SafeError}；Chunk={Index,ByteCount,Hash,ArtifactKey}。Metadata按kind解码成具体结构，不允许任意字段控制文件路径或SQL。

OperationKey由owner/dataset/kind/client-request-ID规范编码形成，重试同key同参数返回同job，不同参数409。未登录不能调用用户job；备份永远没有个人下载URL。

ArtifactKey仅由系统生成的job ID/index组成。PutAtomic写受保护临时文件，累计解压后大小/计算摘要、flush及宿主必要目录持久化后原子改名；原存在同hash幂等，不同hash拒绝。DB记录在文件封存之后提交：崩溃可能留下孤儿文件，由到期reconciler清理；不能先标ready再写文件。

## 2. 状态、租期和防旧worker提交

| kind | 正常状态流 | 失败/取消 |
|---|---|---|
| STAGED/IMPORT | UPLOADING→SEALED→PUBLISHING→COMMITTED | FAILED/CANCELLED/EXPIRED |
| SNAPSHOT/EXPORT | BUILDING→READY | FAILED/CANCELLED/EXPIRED |
| BACKUP | BUILDING→READY→COMMITTED | FAILED，READY允许继续异机传输 |
| DELETE_ACCOUNT | BUILDING→PUBLISHING→COMMITTED | 安全保持禁用并重试，不能取消后重新启用已提交删除 |

Claim在writer事务将attempt加一，记录lease_owner/lease_until；worker每20秒按JobID+Attempt续租。所有封存/发布/状态更新WHERE包含attempt/fence和预期state，影响0行即lost_lease，旧worker不得“最后写赢”。重新领取只处理明确可恢复步骤；Ready产物或Committed结果不重新执行。

worker的60秒lease与用户产物下载/续传有效期分开，设计产物默认24小时、未完成上传默认1小时（可配置，有限续期）。取消/过期先写状态并撤销pin，再删除文件；处于ReadLease或PUBLISHING的产物先协调结束，不能在SYN正在读取时清掉。进程重启扫描所有过期lease，不靠内存timer恢复。

ReadLease持久记录在job_read_leases（LeaseID、JobID、ReaderID、创建/到期），不是复用job worker的lease_owner。申请/续期由Store在writer中核验Scope、sealed/ready状态及有效期限，读取者取得固定manifest hash；默认一次20秒、每job最多4个，可按请求进度有界续期且不越产物绝对期限。清理在同一事务检查有效ReadLease后改变删除状态，避免“检查完又来了新读者”；过期lease不能复活已进入清理的产物。

## 3. 分块上传与Seal

Create验证同资料同kind的活动job/全局worker/磁盘预约上限；一次只允许一个活动snapshot/import，staged按有界个数限制。PutChunk只允许UPLOADING、当前epoch、未过期且属于本人：index连续范围0..maxChunks-1，chunk<=256KiB，总大小<=job预算。写文件后writer事务再次校验状态/lease/已用字节并登记Chunk，失败留下文件可清理，绝不增加可用字节。

Seal输入为按index排序的{Index,Size,Hash}列表、总字节和整体payload摘要。要求没有重复/缺块，文件和metadata一致、完整内容符合kind schema；所有校验受限，文件读取不占writer。最终CAS至SEALED，记录规范payload hash、manifest、绑定的mutation/epoch及当前租期；不再接受修改块。

对已封存相同manifest重试返回原结果，其他manifest冲突。需要换内容新建job。未知chunk缺失、坏hash、磁盘满和取消都不改变用户业务事实。

## 4. Snapshot算法与日志pin

1. Create在writer登记BUILDING job及pin，pin.after取当时head，保证之后清理不能越过它。
2. worker取得短期有界一致ReadTx，读取**同一视图**的dataset head作为cutoff和全部该资料实体/关系/结构/排序/alias/删除事实；不导出users/sessions或其他资料。分块流式写ArtifactStore，超时/超字节终止，不无限持读事务。
3. 在同一个读视图内记录实体数量/DTO版本；完整manifest摘要、每块摘要都来自实际输出，不在开始前猜测。
4. 完成后writer复查job attempt、Scope/data epoch，把pin.after从保守初值推进到cutoff，登记manifest并标READY。数据变化后的新head不影响本快照，客户端从cutoff继续pull。
5. 用户确认已安全使用快照或租期到期后释放pin；release和Compact经writer串行，不能先删日志再登记pin。pin即使保守保留多一些也不能过早推进。

GetManifest/GetChunk每次鉴权并复查READY/租期/当前epoch；只有manifest中的index可读取。下载不因可猜job ID获得权限。取消/失败job不会提供部分manifest。日志/响应引用的BatchRef同样要有ReadLease或不可变副本保护。

## 5. 首次导入/合并算法

IMPORT元数据包括target dataset、data epoch、base_head、DTO版本、client origin及独立mutation ID。客户端在本地先备份并保留持续编辑队列；服务端分块只暂存。

Seal做全量解析、ID/日期/引用/旧兼容字段/配额验证，构造候选合并Plan。不同ID重名对象保留；同ID已一致可no_op，明显版本竞争进入冲突；不能按标题去重或把缺失对象当删除。旧循环同key不同ID由DOM alias规则对账，任何双方内容差异都保存冲突，不静默舍弃。

Publish由同一UnitOfWork执行：先查该job/mutation已成功回执→检查epoch/owner/session及当前head/read set→使用冻结的导入内容与**当前图**重新验证Plan→一次写业务+batch+receipt+job COMMITTED。base_head改变时不得直接覆盖，只返回import_rebase_required并保留SEALED内容供重新预览/对账；更改合并决定使用新mutation。Publish响应丢失，重试按receipt恢复。

小体量导入也坚持原子发布。超过受支持单事务/结果预算时明确拒绝并保留原资料；不得为了进度分段可见后仍宣称“导入失败零修改”。用户主动替换/清空另行确认，不作为自动首次合并策略。

## 6. OPS接口及作业

```text
Backup(ctx, AdminJobContext, OperationKey) -> BackupReport
VerifyBackup(ctx, ArtifactKey) -> {Schema,Counts,Integrity,Hash}
TransferBackup(ctx, BackupJob, ProtectedTarget) -> RemoteReceipt
Restore(ctx, AdminCapability, VerifiedBackup, NewDataDir) -> RestoreReport
PrepareDeletion(ctx, Principal, Password, OperationKey) -> DeletionIntent
ConfirmDeletion(ctx, DeletionCapability, OperationID) -> DeletionStatus
GetDeletionStatus(ctx, DeletionCapability, OperationID) -> DeletionStatus
ReconcileArtifacts(ctx, AdminJobContext) -> CleanupReport
```

Backup：创建真SQLite一致副本→integrity_check/foreign_key_check→记录schema/快照内server及head摘要→加密封存→写READY记录→事务外异机传输和目标摘要验证→写COMMITTED及remote_verified。只有后两项成功才计入RPO目标；READY可重传，不能先轮换删除最后一份可恢复备份。

Restore要求服务停服并取得独占实例锁；解析清单/校验摘要与解密→在新目录恢复和完整性检查→匹配兼容schema→读取独立删除账本并重放→轮换auth epoch和各dataset data epoch、撤销所有session、取消旧传输lease/job→验证→产出待切换报告。原目录始终保留，切换是独立显式动作；缺账本/密钥/有效备份时停在维护态。恢复旧快照的丢失窗口需要报告，不宣称总可从客户端重建已压缩确认的变更。

删除分为INTENT及confirm：Bearer+密码先取得绑定operation的有限能力token，尚不禁用；持有该token确认后事务将users置DELETING、撤销session并登记account_deletions PENDING_LEDGER；外部受保护账本按OperationID幂等持久化并验证→DB记LEDGER_CONFIRMED→按同库事务/可恢复清理计划清除私密业务，保留足以禁止恢复复活的最小身份→COMPLETED。确认响应丢失后仍以独立token幂等重试/查询，不能依赖已经撤销的Bearer。外部账本失败时保持禁用和pending，不返回删除完成。跨文件不伪装单事务；恢复必须先核对账本。token轮换/期限/credential_version的完整约束见[强类型契约](../sync-contracts/README.md)。

Reconcile只清：已取消/过期无有效ReadLease的job文件，metadata从未成功登记且超过保护窗口的孤儿临时文件，轮换策略允许且已有更近异机可恢复备份的旧文件。路径先解析到受控ArtifactStore根内，维护端口不接受任意删除路径。

## 7. 单元与组件测试契约

| 用例ID | 故障/竞争 | 必须断言 |
|---|---|---|
| UT-TRA-01 | 同OperationKey同/不同参数 | 返回同job或409，无双重空间预约 |
| UT-TRA-02 | 重复chunk同/不同hash、缺块/超额 | 幂等/拒绝，SEALED前不改变业务 |
| UT-TRA-03 | 文件完成后DB登记失败 | 不标READY；孤儿可识别，原数据不变 |
| UT-TRA-04 | Seal后篡改/再上传 | 拒绝，manifest及规范摘要不变 |
| UT-TRA-05 | 旧worker封存时lease已被接管 | CAS失效，旧attempt不能写READY/COMMITTED |
| UT-TRA-06 | snapshot捕获期间远端写、Compact竞争 | cutoff一致且pin不缺口，后续增量保留 |
| UT-TRA-07 | download/ReadLease同时过期清理 | 文件在有效lease期间不删除；释放后可清 |
| UT-TRA-08 | Publish期间head变化/最后SQL失败/丢响应 | 重对账/全回滚/查回执，不公开半导入 |
| UT-TRA-09 | 跨账号job ID/路径穿越index | 无文件读取/写入，安全错误无路径泄漏 |
| UT-TRA-10 | 完成job重启、未完成job重领 | 结果不重做，pending按正确step恢复 |
| UT-OPS-01 | backup副本损坏或外键错 | 不READY/不上传，不删除旧备份 |
| UT-OPS-02 | 异机传输失败/目标摘要错 | 不COMMITTED，备份年龄指标违约，允许重传 |
| UT-OPS-03 | restore缺key/账本/错误schema | 原目录不动，无服务ready |
| UT-OPS-04 | restore成功 | 全旧token/游标失效，业务/删除/receipt对账，报告损失窗口 |
| UT-OPS-05 | prepare丢响应/confirm撤销会话且丢响应；账本写成功但DB标记失败 | INTENT安全轮换能力；confirm用独立token幂等查询；重启同OperationID恢复，不复活账号/不重复删除 |
| UT-OPS-06 | 清理遇有效lease/越界路径/只有一份备份 | 拒绝删除，保留恢复能力 |

ArtifactStore真实文件原子性、SQLite一致快照和恢复必须由INT-16～22证明；FakeArtifactStore仅用于故障编排UT。
