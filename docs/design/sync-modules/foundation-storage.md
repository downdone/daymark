# FND/STO：基础运行与存储详细设计

模块入口与共享类型见[开发入口](README.md)。实现目标是一个进程一个writer，所有业务事务可隔离测试；数据字典及约束见[DDL设计稿](server-schema-v1.sql)。本文不修改桌面schema001～009。

## 1. FND配置与运行接口

```text
LoadConfig(file, env) -> Config | Fault
ValidateConfig(Config) -> ValidConfig | Fault
Runtime.Start(ctx, ValidConfig) -> RunningServer | Fault
Runtime.Readiness() -> {Ready, ReasonCode, DegradedReasons}
Runtime.Stop(ctx) -> StopReport | Fault
InstanceLock.Acquire(DataDir) -> Lease | already_running
```

环境覆盖配置文件；禁止运行时修改schema/data路径/认证代次。ValidConfig创建后不可变，服务不把密码/备份密钥写入dump。依赖经main显式注入：Clock、TokenSource、Store、ArtifactStore、日志/指标及受控Scheduler。

| 项 | 设计默认/校验 | 生效范围 |
|---|---|---|
| data_dir/listen/public_origin | 必填或明确受保护相对默认目录；生产origin为HTTPS | 启动一次，数据目录与静态目录分开 |
| writer_queue/read_pool | 32 / 4，必须正数，writer连接固定1 | 准入和连接池 |
| http_concurrency/header/idle | 64个活动handler / 5s读header / 60s idle，header最多16KiB | 满额在读body前拒绝；入口连接数也有显式上限 |
| request_timeout/busy_timeout | 10s / 2s；busy不能超请求剩余期限 | 所有数据库调用context |
| push_bytes/commands/inline_entities | 1MiB / 64 / 512 | capabilities和实际限制同源 |
| chunk_bytes/staged_bytes/snapshot_bytes | 256KiB / 64MiB / 256MiB | 解压后累计大小及磁盘预约 |
| job_workers/job_lease | 2 / 60s，heartbeat每20s | 进程重启重领；token fencing见TRA |
| access_ttl/refresh_absolute_ttl | 15min / 30days | 新会话；旧会话不因改配置无限续期 |
| kdf_parallelism | 1，最大按内存校准 | 在进入数据库writer前准入 |
| log_retention/backup schedule | 90days / 30min | 保留须受pin约束；备份见OPS |

期限及大小是设计初值，不代表容量承诺。未知配置键、非法duration、路径重叠、缺少必要密钥明确报错；不静默恢复默认绕过错误。

启动顺序：验证配置→锁→打开连接并核验PRAGMA/引擎→迁移检查与必要备份→完整性→构建服务/worker→监听→ready。停止先ready=false并关闭新任务准入，再Shutdown已接受请求/作业，最后关闭Store并释放锁。超过停止deadline返回未完成报告，由持久回执/job恢复，不写“全部完成”。

Stop不能先取消所有已受理请求的根context再声称优雅排空；先关闭新准入/调度，保留在途ctx直到其完成或总停止期限，再执行强制取消。内存里的排队任务不视为已持久接收，只有本地journal/服务器job或业务receipt有相应承诺。

## 2. STO端口和所有权

```text
Store.Read(ctx, scope, fn(ReadTx) -> error) -> error
UnitOfWork.Write(ctx, scope, fn(WriteTx) -> error) -> error
ReadTx.LoadAccount/LoadSession/LoadDataset(...) -> value | not_found
ReadTx.LoadGraph(keys, structures) -> GraphSnapshot | Fault
ReadTx.FindReceipt(mutationID) -> Receipt? | Fault
ReadTx.ReadBatches(after, through, budget) -> BatchPage | Fault
WriteTx.CheckSessionAndDataset(scope, now) -> AuthorizedScope | Fault
WriteTx.ApplyPlan(scope, Plan) -> RevisionMap | Fault
WriteTx.AppendBatchAndReceipt(scope, FrozenMutation, Result) -> Receipt | Fault
WriteTx.SaveSession/RetireRefresh/SaveJob/SavePin(...) -> error
```

Read/Write的scope参数用于限定SQL，**不自动替代**用例需要的授权复查。登录阶段使用内部AccountScope，尚未有dataset；只有Auth/维护用例可以调用该端口，不对HTTP暴露通用SQL方法。

Write在固定writer上使用驱动已验证的IMMEDIATE事务配置：入队等待也受ctx控制→BeginTx→执行fn→fn错误Rollback→fn成功Commit。所有动作经同一个sql.Tx，禁止在fn里用全局sql.DB。释放Rows/statement后才能Commit；错误必须包含稳定类别，不能只比较本地化错误字符串。现代驱动参数以[其官方接口](https://pkg.go.dev/modernc.org/sqlite#Driver.Open)和实际版本测试为准。

fn返回错误一定回滚。存在“必须提交安全状态、随后向调用方返回拒绝”的用例，例如refresh重放撤销：回调保存撤销并返回nil，把拒绝放在类型化Outcome；外层Write成功后才返回refresh_replayed。禁止靠“可提交错误”隐式改变通用事务约定。

## 3. SQL策略与数据字典

完整字段在DDL，重要映射如下：

- 所有瞬间存UTC毫秒INTEGER，日期存规范ISO TEXT；wire转换由codec完成。正文为UTF-8 TEXT；token摘要为32-byte BLOB。日期合法性/IANA时区及口令格式不能仅靠SQL字符串CHECK证明。
- datasets的owner指向users；业务复合主键为(dataset_id,id)，关系复合外键阻止跨资料引用。每次SELECT/UPDATE显式加dataset和owner授权存在性条件。
- task的revision覆盖task及其schedule/reminder/tag/link聚合；循环规则/分类/标签/排序范围有独立revision；结构版本由最终Plan精确增加。每组同一实体最多增一次revision。
- ApplyPlan按引用拓扑操作或使用DEFERRABLE约束，同一事务验证最终图；不得先删旧关系并Commit后补新关系。新版本=旧版本+1，SQL WHERE包含expected revision，受影响行数不是1即Conflict。
- head_seq在AppendBatchAndReceipt里普通整数加一，与batch/receipt同事务。no_op只写receipt，batch_seq为空，head不变。压缩日志不能级联删除receipt。
- job/chunk数据文件与数据库两阶段提交规则见TRA；数据库不存客户端文件路径。SQLite文件不是个人导出格式。

DDL中的CHECK/复合FK是第二层保护；循环/层级/状态和完整read set由领域/用例保证。SQLite不默认开启外键，必须每连接配置并读回，参考[官方外键说明](https://sqlite.org/foreignkeys.html)。

## 4. 迁移、锁和异常

schema_migrations记录version/checksum/applied_at，启动校验已应用文件不可改写。新版本仅追加；非空库升级先一致备份并校验，备份失败零迁移。所有新表/变换在一个受控事务批次应用；需要事务外步骤的迁移必须独立评审并维护可恢复状态，不能直接混入自动启动链。

read_pool连接只读/query_only，不开shared cache/read_uncommitted；writer的foreign_keys、synchronous等在连接hook设置并验证。BUSY/LOCKED在有限期限内映射busy，FULL/IOERR/CORRUPT触发写保护和ready降级；约束错误映射需区分业务Conflict/Validation与内部错误，不能让开发漏列FK变成“服务器成功”。

Commit有错误时，不把结果值交给调用者当成功；SYN以独立连接查询回执恢复。如果无法确定则commit_unknown，Store不自行重跑fn，避免重复随机数/副作用。连接恢复与原mutation重试由用例协调。

## 5. FND/STO单元与组件验收

| 用例ID | 输入/故障 | 必须断言 |
|---|---|---|
| UT-FND-01 | 未知键、负队列、busy大于请求期限 | Validate失败，尚未打开DB或监听 |
| UT-FND-02 | Start各步骤依赖失败 | ready始终false，已获资源逆序释放，库不删除 |
| UT-FND-03 | Shutdown中有在途请求 | 拒绝新写，先等待已有写，再关Store/锁；超时报告不伪成功 |
| UT-STO-01 | SpyTx中fn错误/成功/Commit错误 | Rollback/Commit调用和错误传播正确，fn不隐式重跑 |
| UT-STO-02 | writer队列满/排队取消 | 有界拒绝，无fn执行/连接泄漏 |
| CT-STO-01 | 真文件执行DDL并开全部连接 | PRAGMA真实值、复合FK及索引存在；跨dataset关系拒绝 |
| CT-STO-02 | ApplyPlan最后一条SQL失败 | 实体、关系、revision、head、日志、receipt全回滚 |
| CT-STO-03 | 独立连接持写锁 | 另一写者在busy预算内返回busy；释放后可重试 |
| CT-STO-04 | 已应用migration checksum不同/新schema | 拒绝启动，原文件字节与版本保持 |
| CT-STO-05 | 备份失败/DDL变换失败 | 原库可读、版本未推进、可重试，无半迁移 |

CT属于真实存储组件测试，不算纯单元测试。只有UT/CT通过后，按[INT-01/02](testing-integration.md)接入其他模块。
