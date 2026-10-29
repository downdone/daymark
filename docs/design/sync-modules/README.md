# 同步系统模块详细设计与开发入口

日期：2026-10-28。基于[工程架构](../server-architecture.md)、[FR-042/ADR-026](../../ADR/026-go-sqlite-sync-service.md)，把模块细化到输入输出、算法、事务、错误及可执行测试断言。本文档组定义开发契约；实现位于[server](../../../server/README.md)、`src/sync`及现有桌面适配器，运行入口、具体包结构与限制见服务README。测试设计不等于全部场景/平台已经通过，当前事实只见[PROGRESS](../../PROGRESS.md)。

## 1. 文档及唯一责任

| 模块ID | 详细设计 | 开发者应产出的东西 | 独立测试入口建议 |
|---|---|---|---|
| FND/STO | [基础运行与存储](foundation-storage.md)及[DDL设计稿](server-schema-v1.sql) | 配置/时钟/进程锁、UoW、限定Scope的Store、迁移 | application/foundation、storage/sqlite |
| AUTH/HTTP | [账号与传输](identity-http.md) | 登录/刷新/撤销、严格codec、规范摘要/游标、HTTP映射 | application/auth、protocol/v1、transport/httpapi |
| DOM | [业务命令与领域策略](workspace.md) | 值对象、全部命令参数、read set、Plan、循环身份 | domain，纯函数表驱动 |
| SYN | [服务端复制用例](replication.md) | push/receipt/pull、提交不确定、保留/pin协调 | application/sync，后接真实Store |
| TRA/OPS | [传输作业与运维](transfer-operations.md) | 快照/暂存/导入、作业恢复、备份/恢复/清理 | application/transfer、maintenance |
| CLI | [Qt同步客户端](client.md) | 捕获/outbox/base、协调器、远端合并、凭据端口 | Qt Test，独立临时资料 |
| INT | [测试及集成方案](testing-integration.md) | 共享fixture、模块门禁、组合/双端/故障测试 | 从INT-01顺序集成 |

目录为逻辑职责，不要求一模块一Go包；实际包继续遵循架构的`application → domain`、适配器实现端口方向。不要把上述“独立测试入口建议”解释为现在创建多套空工程。

运行schema以[服务器migration 1](../../../server/internal/storage/migrations/0001_initial.sql)、[migration 2](../../../server/internal/storage/migrations/0002_ledger_identity.sql)及[桌面migration 10](../../../src/persistence/migrations/010_account_sync.h)为准；本目录DDL保留为原设计模型，不可覆盖已交付migration。当前新增的初次合并、身份映射和后继完整快照元数据见[桌面实现契约](../sync-client-implementation.md)。

网络路径/机器结构以[OpenAPI](../../api/sync-v1.openapi.json)为准，[v1协议](../../api/sync-v1.md)解释语义；模块文档定义算法。[强类型契约与字段映射](../sync-contracts/README.md)提供可编译Go/Qt声明、本地元数据DDL及回执/逐批检查点等补充裁决。存储字段及约束以DDL为准；单元断言靠各模块UT表，跨模块流程靠INT表，其他文档链接引用而不复制第二套。

## 2. 共享类型与约定

以下是语言无关签名记法，Go可实现为结构/接口，Qt实现为值对象/Result；不是可直接编译的源码：

```text
ID = canonical UUID string
Revision, Seq = int64 >= 0             // wire为十进制字符串
Date = validated YYYY-MM-DD           // 不转换时区
Instant = UTC Unix milliseconds       // wire为固定毫秒Z字符串
Field<T> = Unspecified | Clear | Set(T)
Principal = {UserID, SessionID, AuthEpoch, CredentialVersion, ExpiresAt}
DatasetScope = {Principal, DatasetID, DataEpoch}
EntityKey = {Type, ID}
StructureKey = {Kind, ID}
Fault = {Code, FieldPath?, RetryAfter?, SafeArgs?}
Clock.NowUTC() -> Instant
TokenSource.NewBytes(n) -> bytes | Fault
```

Principal只能由认证适配器/测试组装产生；dataset归属在Store内复查，不能从JSON直接反序列化为可信Principal。DataEpoch在每次写/pull/作业操作核验；AuthEpoch保护会话恢复，不混入业务实体revision。测试Clock手动推进，重试期限使用可控Scheduler；生产TokenSource使用安全随机源。

返回错误分为：校验/冲突/认证等确定业务错误，busy/timeout等暂时错误，commit_unknown等结果不确定错误，corrupt/io等写保护错误。SQL原文、用户正文、token及本机路径不进入Fault.SafeArgs。context取消不等于Commit必然失败。

模块端口只暴露业务动作。所有tx对象/Rows在回调结束前关闭，不能交给goroutine或保留到下一个请求；返回的值必须复制所需数据，不引用可被复用的字节缓冲区。

## 3. 测试替身与真实能力边界

单元测试允许FakeClock、ScriptedTokenSource、FakeHasher、SpyUnitOfWork、ScriptedTransport、FakeArtifactStore。FakeStore用于证明用例决策、调用顺序和错误传播；不能用它证明SQL外键、锁、Commit、WAL或进程崩溃恢复。

存储组件测试必须使用Go `t.TempDir()`内的文件SQLite；Qt使用QTemporaryDir。HTTP handler单元用Recorder检查接线，HTTP组合用真实TLS测试服务器。进程故障测试使用子进程/屏障通知控制时序，不以固定sleep或随机等待制造竞争。

模块交付先满足其UT表，再接真实依赖完成对应INT门禁。不得仅因unit绿灯就合并全部模块后宣布端到端可用；每个接口至少验证成功、业务拒绝、依赖失败及取消/超时。

## 4. 开发批次与完成定义

| 批次 | 模块与范围 | 前置依赖 | 通过门禁 |
|---|---|---|---|
| DEV-01 | FND/STO最小schema、单writer、事务 | 具体驱动/引擎版本及许可记录 | FND/STO UT + INT-01/02 |
| DEV-02 | HTTP codec/摘要/游标与纯DOM普通任务 | DEV-01不作为纯函数测试前提 | HTTP/DOM UT + INT-03 |
| DEV-03 | AUTH账号、会话与Scope | DEV-01/02 | AUTH UT + INT-04/05 |
| DEV-04 | SYN普通任务push/receipt/pull | DEV-01～03 | SYN UT + INT-06～09 |
| DEV-05 | CLI本地捕获/队列/确认/拉取 | DEV-04及Qt迁移备份设计 | CLI UT + INT-10～13 |
| DEV-06 | DOM树/分类/排序/循环/提醒配置 | DEV-04/05，共享旧语义fixture | DOM剩余UT + INT-14/15 |
| DEV-07 | TRA首次合并/快照/暂存/保留 | DEV-04～06 | TRA UT + INT-16～18 |
| DEV-08 | OPS恢复、删除账本、升级/发布 | 前述全量业务 | OPS UT + INT-19～22 |

每批只创建本批使用的类/表迁移。DDL是完整目标模型，不要求DEV-01立即上线全部表；实施时按批次拆成追加migration。完成需要接口/字段映射落地、UT与对应INT有真实证据、原Qt回归、错误路径/隐私审查和文档同步。公开注册、E2EE、第三方登录及移动端仍不在本次开发基线。
