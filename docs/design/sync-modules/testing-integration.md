# 模块单元测试、组件测试与集成验收方案

本页定义测试输入、步骤和oracle；当前实际结果只在[PROGRESS](../../PROGRESS.md)。测试设计不表示已有Go/Qt同步实现。各模块UT/CT表是用例定义唯一来源，本页负责组合顺序、基础设施和需求追踪。

## 1. 分层与执行门禁

| 层 | 被测对象/依赖 | 能证明什么 | 下一层准入 |
|---|---|---|---|
| L0设计产物校验 | Markdown/JSON/DDL，临时数据库 | 格式、基础FK/CHECK/事务设计可执行 | 不能代替任何Go驱动或业务测试 |
| L1纯UT | DOM纯函数；用例+可脚本化端口；状态机+FakeClock | 决策、边界、调用顺序、失败保留和错误映射 | 各模块UT全过，故障分支无缺项 |
| L2组件CT | 实际Go驱动文件SQLite、真实ArtifactStore、TLS HTTP handler、Qt临时库 | 事务、锁、FK、codec/文件落地及线程边界 | 对应CT全过，实际引擎/PRAGMA记录 |
| L3模块组合INT | 真实Store+用例+HTTP，再连Qt | 模块间契约和异步时序一致 | 按INT依赖顺序通过，不跳关键层 |
| L4双端/恢复/平台 | 两Qt资料+服务进程+故障代理、真实OS端口 | 同步收敛、崩溃/灾备恢复、OS实际能力 | 全量交付、隐私与平台门禁 |

UT不能断言“调用了某getter所以可靠”；优先结果状态/副作用/错误，必要时Spy验证无SQL/网络在错误路径发生。SQL锁/Commit/FK与进程故障只在真实组件证明；不把FakeHasher、Mock备份或QML offscreen作为安全/灾备/OS证据。

分批实施时每份报告必须声明migration版本和能力范围。CT-STO/INT-01等场景按已引入的表/能力建立明确子场景；未实现的完整业务子场景记录NOT_RUN，不能通过动态忽略断言让测试名字显得全量通过。DEV-01只准入其最小存储范围，CT全量门禁在相关后续DEV批次补齐，最终发布必须覆盖本页全部既定范围。

## 2. 共享fixture和独立oracle

固定FakeClock为2026-10-28T08:00:00.000Z；用户区Asia/Shanghai。业务ID采用固定canonical UUID registry（例user1=`10000000-0000-4000-8000-000000000001`、dataset1=`20000000-0000-4000-8000-000000000001`，其他由测试名确定），不使用真实账号/任务库。密钥/真实token只在测试临时目录或内存生成，不输出。

| fixture | 数据 | 负责的风险 |
|---|---|---|
| FX-EMPTY | 两用户各独立dataset，head0，规则/任务为空 | 归属、首次接入、空分页 |
| FX-TREE | root、OPEN子A、此前独立完成子B、此前独立删除子C、旧深层兼容链 | 分组完成/恢复、批量祖先去重、深度兼容 |
| FX-CATALOG | 一级A/二级A1、一级B、同名不同ID分类/标签、跨资料同名对象 | 移层/删除迁出/跨资料FK/不按标题去重 |
| FX-REPEAT | 月31日、2/29、周掩码、固定规则source例外、AFTER_DAYS前驱；不同旧随机实例ID | 稳定键、alias、历史保留及一个后继 |
| FX-ZONE | Asia/Shanghai、Europe/Berlin，2026-03-29 02:30与2026-10-25 02:30本地时刻 | gap/fold Reject、日历日/UTC分离 |
| FX-PENDING | base revision3、m1修改、m2依赖、已确认回声及另一设备revision4 | 冻结、连续本地修改、三方冲突 |
| FX-LOG | 连续seq1..N、跨多实体batch、no_op receipt、保留floor和活动pin | 完整分页、压缩竞争、receipt保留 |
| FX-10K | 10k合成任务、比例化父子/标签/日程/循环、长短备注；1k离线命令 | 资源预算、索引、重连、WAL及恢复规模 |

期望值存手工审阅的typed JSON/结构：before、command、expected after、expected changed IDs/structures、expected Fault。不要用被测Plan/encoder生成expected。跨语言比较task/category/tag/rule/关系/分组/日期/配置/排序/alias的规范业务图，排除本机rowVersion、投递历史、GUI滚动及server收件时间；同时比较服务器seq/receipt数量和本地pending/conflict，不能只看列表文本一致。

规范摘要fixture包括基础独立字节向量、对象键顺序、null/未指定、补充平面字符、非法surrogate、set数组和有序commands。Qt和Go分别实现解析/编码，再读取相同期望字节；一致并不足够，必须都等于独立oracle。

当前已落盘的[16个编码/摘要基准](../../api/fixtures/canonical-v1.json)可直接供未来单元测试读取；其余validator/领域/网络fixture按上述FX表实现。当前落盘不等于未来codec已通过，设计校验使用的独立Python参考只用于作者核对，不替代Go/Qt测试。

## 3. 测试基础设施接口

```text
ServerHarness.Start(tempDataDir, config, testHooks?) -> ServerProcess
ServerHarness.Restart/KillAt(barrier)/QueryCanonicalGraph/QueryStorageFacts
ClientHarness.Open(QTemporaryDir)/ExecuteExistingServiceCommand/ExportCanonicalGraph
ClientHarness.PumpUntil(predicate, deadline)/Restart/SwitchProfile
FaultProxy.DropReplyAfterUpstreamCommit/Disconnect/Delay/Reorder/FailNext
Barrier.WaitReached(name)/Release(name)        // IPC或受控通道，不固定sleep
ArtifactProbe.FailAt(write/fsync/rename/register/remote-verify)
```

ServerHarness的存储查询只经测试构建端口或直接只读检查已知临时库，生产不暴露“任意查库”HTTP。服务端BeforeCommit/AfterCommit故障钩子仅测试helper编译可用，不能靠外部请求头/生产环境变量开启。FaultProxy丢失成功响应时实际上游先Commit，再断下游，避免只测“还没发出去”。

Go用t.TempDir，Qt用QTemporaryDir；共享集成root显式传给两个进程/客户端。限时等待predicate/屏障并在失败导出安全状态，禁止无限等待和靠扩大全局watchdog消除失败。TLS测试使用测试证书及显式测试信任，不增加生产忽略TLS开关。[Go testing](https://pkg.go.dev/testing)、[httptest](https://pkg.go.dev/net/http/httptest)

## 4. 集成测试步骤与断言

每项先从fixture建立干净独立环境，验证前置层结果；无前置证据则该项为NOT_RUN/UNVERIFIED，不自动记SKIP=PASS。

| ID / 依赖 | 操作步骤 | 独立关键断言 |
|---|---|---|
| INT-01 / FND/STO UT | 真文件启动，读PRAGMA，第二实例尝试相同dataDir；停止再启动 | 单实例拒绝、writer1、正确引擎/外键/WAL/FULL，重启业务保留 |
| INT-02 / INT-01 | seed旧schema；备份失败、最后迁移失败，再正常迁移 | 失败原表/版本不变；成功checksum/version正确；过新schema拒绝 |
| INT-03 / HTTP/DOM UT | TLS handler接真实DTO；发送摘要向量、重复键、null/缺失、非法数值/体积 | 正确响应；错误未进入业务；Qt/Go摘要各等独立oracle；无凭据日志 |
| INT-04 / AUTH UT、INT-01/03 | 创建合成账号；login→authenticate→refresh；并发相同refresh；revoke/logout | 最多一次刷新，重放撤销持久；新请求旧token失败；绝对期限不延长 |
| INT-05 / INT-04 | user2猜user1任务/dataset/job/cursor/session；跨资料建parent/link/tag | 读写/文件均拒绝；两图/seq不变，错误不泄密；不能换device_id绕过撤销 |
| INT-06 / SYN UT、INT-04 | 同mutation并发push；上游Commit后FaultProxy丢响应；重试/查receipt | 任务1份、seq增1、receipt1份、后继/关系不重复；原成功回执稳定 |
| INT-07 / INT-06、DOM UT | 两端读同base；写A/B竞争；另一端在树操作前新增child | 一个成功另冲突；关系集合竞争被捕获，无半树变更 |
| INT-08 / INT-06 | 子进程停在业务写后/日志前/Commit前分别kill；再在Commit后kill并重启 | Commit前全部回滚；Commit后全部存在且同ID可查；无仅日志/仅任务 |
| INT-09 / INT-06 | FX-LOG多页；页内第一批Commit后杀进程；第一页后新增事务；no_op；活动pin下Compact；最后页后再pull | 每批after_cursor不越过尚未应用批次；单轮through固定，完成回tail可见新写；压缩不越pin，receipt保留 |
| INT-10 / CLI UT、INT-01 | Qt现有命令本地提交，在journal最后写故障，断网重启再联网 | 失败业务/queue全回滚；成功queue耐重启；未登录网络0且本地可用 |
| INT-11 / INT-06/09/10 | Qt A发送m1后继续m2，延迟ack；随后ack、pull自己的回声，再发送m2 | 本地始终保留m2内容；server版本正确绑定；无伪冲突；applied只随pull |
| INT-12 / INT-11 | Qt A离线改task，B在线改/删同task；A重连后解决，解决期间B再改 | 三方/删除冲突持久，原输入可找回；只阻塞依赖；新解决mutation仍受CAS |
| INT-13 / INT-10/12 | A账号请求在途时logout/登录B、旧回复到达；切库失败；保存草稿失败 | B无A数据、旧回复不改新状态；旧库/草稿/队列保留；无假成功 |
| INT-14 / DOM UT、INT-07/11 | FX-TREE/CATALOG完成→重开→删除→恢复；移父/子、删分类/tag、部分排序；各中点故障 | 与共享oracle逐字段相同；旧独立终态保留；排序不改全部内容revision；失败整体回滚 |
| INT-15 / INT-14 | FX-REPEAT/ZONE双端同周期生成、AFTER_DAYS并发完成；提醒本机触发后再同步 | 稳定实例/至多一个后继，DST拒绝正确；source/旧ID安全；投递不改远端配置/不回声 |
| INT-16 / TRA UT、INT-09 | snapshot pin登记后并发写/Compact；逐块下载，续租/过期，取消后清理 | cutoff来自同视图；随后增量无缺失；无半manifest；有效lease文件保留 |
| INT-17 / INT-16 | staged大组分块，错hash/缺块/超额；文件成功DB失败；worker lease过期被接管 | 无业务修改、孤儿可清；旧attempt不能发布；同manifest幂等；已成功回执不因块过期丢失 |
| INT-18 / INT-14～17 | 本地/服务器双方有数据首次合并；期间继续编辑；同ID冲突；Publish后丢响应 | 保留双方不同ID和原本地备份；真实冲突可解决；后续编辑不丢；一次原子发布 |
| INT-19 / OPS UT、INT-18 | 真一致备份；损坏、满盘、远端失败/摘要错，随后正常重传及轮换 | 坏备份不ready、不提前清最后恢复点；远端验证才算RPO合格；实际恢复完整 |
| INT-20 / INT-19 | prepare/confirm响应丢失及Bearer撤销；删除账本/DB中点失败；恢复较旧备份到新目录；客户端持旧token/cursor重连 | INTENT轮换/独立能力重试查询正确；账本阻止已完成删除复活；新epoch使旧凭据/游标失效；本地队列对账、损失窗口报告 |
| INT-21 / INT-02/19/20 | 升级前备份，迁移失败及启动失败；升级后已新增写入再尝试旧版 | 失败保持原库；schema兼容才二进制回滚；不得直接旧库覆盖新增写入 |
| INT-22 / 全部前置 | FX-10K + 两端网络分区/重连/重启；慢下载+KDF+备份竞争；目标Windows/macOS实机 | 最终规范图/receipt/pending收敛；资源/队列/WAL有界；平台凭据/托盘/200%/键盘及通知实际结果分列 |

INT-08至少在真实Go SQLite驱动和目标Linux文件系统运行；Python SQLite设计校验不计此项通过。INT-22资源与平台可以拆子场景分别报告，不能一个Linux性能结果冒充Windows/macOS已验证。

CT-HTTP和备份传输的本机TLS目标只证明传输组件；RPO/异机故障域目标达标必须在真实独立目标完成并保留证据。缺少授权或基础设施时该项为UNVERIFIED，不能把本机另一个目录或Mock当成异机备份。

## 5. 需求/架构到测试追踪

| 需求或不变量 | 单元/组件责任 | 集成责任 |
|---|---|---|
| R01/R04离线、本地和自动同步；AR-01/04 | CLI-01～05、CT-CLI-01、STO-01/CT-STO-02 | INT-10/11/13 |
| R02/R09账号会话；AR-05 | AUTH-01～08、HTTP-01/02、CT-STO-01 | INT-04/05 |
| R03/R06首次/幂等/恢复；AR-02/03 | SYN-01～04/06～09、TRA-01～08 | INT-06/08/09/16～18 |
| R05完整业务；AR-06 | DOM-01～14、SYN-05、CT-STO-02 | INT-07/14/15 |
| R07冲突；AR-04/07 | CLI-06/07/12、DOM-08、SYN-05 | INT-07/12/18 |
| R08账号隔离；AR-05 | AUTH-06、HTTP-07、TRA-09、CLI-08 | INT-05/13 |
| R10状态与错误；AR-07/09 | HTTP-03、FND-03、CLI-09/10/13 | INT-03/11～13/22 |
| R11保护/备份；AR-08/09/10 | FND/STO、OPS全部、SYN-10 | INT-01/02/19～22 |
| R12兼容/游标；AR-03/08 | HTTP-04～07、SYN-07～12、CLI-07/12 | INT-03/09/16/20 |
| R13导出/删除 | TRA-09/10、OPS-03～06 | INT-05/19/20 |
| R14排序 | DOM-13、CT-STO-02 | INT-14 |
| R15变更及时性（首期轮询） | HTTP-06、SYN-07、CLI-10 | INT-09/11/22；不据此宣称SSE已实现 |

R前缀是SYNC-R提案追踪，UT缩写对应各模块定义；未接受产品项不因有测试设计自动纳入实现。AR不变量是当前详细设计的工程验收目标。

## 6. 运行方式与报告

实际实现按包内描述性`Test...`名称组织，不使用尚不存在的`TestUT_`/`TestINT_`过滤器或空integration目录。普通Go测试包含真实临时SQLite、文件和子进程故障用例；双端Qt依赖显式环境变量，缺少时SKIP。Windows PowerShell与macOS/Linux shell在server目录同样运行：

```text
go vet ./...
go test ./... -count=1 -timeout=5m
go test -race ./... -count=1 -timeout=5m
go test ./internal/httpapi -run '^TestQtDesktopEndToEnd$' -v -count=1 -timeout=5m
```

双端命令前须设置DAYMARK_QT_SYNC_TEST_EXE为已构建test_task_workspace的绝对路径，并配置Qt/编译器运行库PATH；具体Windows/macOS差异见[运行指南](../../../server/README.md)。race在支持的实际runner运行并记录平台/编译器；缺工具/平台应报NOT_RUN/UNVERIFIED。标准Qt验证仍使用原build/test脚本。新增同步Qt测试复用既有TaskWorkspaceTest及DataTransferTest的隔离fixture，保留既有watchdog预算，不扩大历史UI长链。实现测试对应关系及实际范围见[实现验证清单](../../sync-verification.md)。

报告最少包含用例ID、源码/协议/迁移版本、Go/SQLite/Qt版本、OS/文件系统、fixture种子、具体命令、耗时、实际断言、PASS/FAIL/UNVERIFIED、脱敏artifact路径。失败保留临时资料及安全状态以便复现，不输出真实用户数据/口令。功能实现状态与测试状态分别记，设计检查、unit、真实集成、平台、发布门禁互不替代。
