# 账号与同步：实现验证清单

FR-042、AR-01～10；设计编号的输入/oracle仍见[模块测试方案](design/sync-modules/testing-integration.md)。本文建立实际源码入口与风险的对应关系，不把设计中的76 UT/8 CT/22 INT或平台目标统一标成已通过。实际命令、PASS/FAIL、耗时及未验证项只见[PROGRESS](PROGRESS.md)。

| 风险/契约 | 实际测试入口 | 验证边界 |
|---|---|---|
| 严格JSON、106 Schema、规范摘要/集合/三态 | `server/internal/protocol/codec_test.go`；`DataTransferTest::syncSharedContractFixturesAndGoldenCanonicalBytes` | Go与Qt执行77正反样本和16独立黄金向量；生成副本漂移检查。不是未来所有协议升级的自动兼容证明。|
| 纯业务规划、整树状态组/深度、分类标签、排序、日历与DST、重复身份 | `server/internal/domain/policy_test.go`、`restore_test.go`；原Task/Unified/Recurrence/Calendar Qt测试 | 结果图、组/历史保留、失败不修改输入、重复恢复no_op及规则不可变。|
| 文件SQLite、WAL/FULL/FK、单实例锁、迁移/备份、CAS/回执原子性 | `server/internal/storage/store_test.go`、`crash_test.go`；Qt storage/data_transfer | 真实临时文件和子进程kill，覆盖business后、receipt前、commit前及commit后；不以其证明真实断电或存储控制器语义。|
| 登录/刷新重放、会话撤销、密码恢复、提交不确定、并发幂等 | `server/internal/application/application_test.go`、`recovery_test.go`、`httpapi/handler_test.go` | 大多数用例用可控Hasher；另有生产Argon2id真实计算与可信TLS组合测试。|
| 自助注册、重放/并发/关闭/限流 | `application/registration_test.go`、`httpapi/registration_test.go`、`handler_test.go`；Qt双端首账号从注册建立 | 同名并发仅一个dataset；不覆盖密码/恢复删除身份；字符与字节限制、严格JSON、全局/IP限制、旧配置关闭；真实TLS注册后继续push/pull。|
| 所有受保护HTTP入口先认证 | `server/internal/httpapi/authorization_matrix_test.go` | 从OpenAPI枚举35操作中的29受保护操作，未认证不得解析正文/访问业务依赖；Problem Schema及no-store同时检查。|
| 分块/seal/publish、cutoff、lease/fencing/pin、续传/过期 | `server/internal/application/transfer_test.go`；`artifact/store_test.go` | 真实文件哈希、不可变产物和数据库状态；Qt联调另走大命令暂存与大batch下载。|
| 删除账本、防复活、加密备份及恢复到新目录、轮换 | `server/internal/maintenance/maintenance_test.go`；`application/account_test.go`、`recovery_test.go` | 实际临时文件的签名链、head、损坏拒绝、独立复制及READY续传；真实异机故障域和运维值守另验。|
| 本地业务+outbox原子性、冻结重试、receipt/echo、后继依赖、冲突/epoch | `tests/services/DataTransferTest.cpp`内sync用例 | 最后写入故障回滚、冻结不可改、后继不被自己回声覆盖、连续远端候选替换、初始冲突、旧epoch证据保留。|
| 旧随机实例身份、同日AFTER_DAYS/实际日期、完整事实收敛 | 同文件`syncRapidAfterCompletionKeepsDistinctIdentityAndSameActualDay`及`syncLegacyRecurrenceAliasesPreservePhysicalIdentityAndTypedReferences` | 不重命名本地编辑/历史ID，不泛替换其他UUID，确认后采用服务端完整时间。|
| 删除冲突的安全显示 | 同文件`syncJournalRollbackFrozenRetryAndOwnEchoPreserveLaterEdit` | 候选保留、统一查询/搜索隐藏、显式本地选择后恢复；提醒领取读取相同投影。|
| 真实双端、掉响应、离线、账号隔离、完整业务及大组 | `server/internal/httpapi/desktop_integration_test.go`驱动`tests/integration/AccountSyncTest.cpp` | 两个真实Qt工作区、真实Go服务/SQLite/产物、独立TLS CA；1000任务导入、并发rebase、成功提交后丢push/logout响应、批量完成后继、分类标签/层级/日程提醒/整树恢复；最终逐实体完整Value与服务端图比较。|
| 性能基线 | `server/internal/storage/scale_test.go`；`tests/services/PerformanceTest.cpp` | 文件SQLite 10k任务导入/读取/修改及原桌面10k任务/关系基准；没有以单机均值冒充P95、慢网或重连风暴结论。|
| UI与平台 | 原desktop_surfaces/calendar_interaction等CTest；隔离Windows原生查看 | QML加载、共享设置/主题/大字回归与真实原生查看分别记录。账号表单/冲突完整原生键盘、200%、读屏、macOS Keychain/原生交互及生产部署未由核心联调覆盖。|

模块内测试与集成测试共用生产实现；仅时钟、确定性凭据或故障边界使用替身。不能把普通运行时Qt联调的SKIP统计为PASS。双端日志只包含合成数据，不上传真实任务或系统凭据。

补充故障覆盖：RecurrenceServiceTest执行相同完成时间及设备时钟回拨；双端测试把真实staged job改为EXPIRED，验证一致备份、候选/未确认队列保留及显式重新合并，结束时仍逐实体比较完整事实。

可复制命令见[服务指南](../server/README.md)及[测试方法](testing-method.md)。CI的[Account and sync](../.github/workflows/sync.yml)执行三平台Go门禁、Linux race与Windows实际双端；已有Build and test继续承担完整桌面CTest和QML lint。新增CI在推送后才会产生执行证据，本地通过不替代远端检查。

SYNC-07/UX-61将注册表单切片迁至独立AccountWindow，移除旧设置表单，并扩展到错误/连接中/账号概览/冲突/本地资料/窄窗和折叠入口。合成syncState仅证明展示；另观察两种提交经过真实Main保存门/Coordinator的loopback连接失败，校验临时参数和秘密清理。真实成功认证沿Qt/Go双端测试。

注册表单切片仍使用生产QML和真实临时workspace，独立CTest account_access包含98～115阶段，desktop_surfaces恢复到97，不重复运行同一账号切片；调试渲染可显式选择该切片，不替代整套门禁。PowerShell：`$env:DAYMARK_SMOKE_UI_SLICE="registration"` 后运行 `daymark.exe --desktop-surfaces-smoke --screenshot <输出.png>`，执行后移除该环境变量。macOS/Linux：`DAYMARK_SMOKE_UI_SLICE=registration ./daymark --desktop-surfaces-smoke --screenshot <输出.png>`。Qt平台/软件渲染设置沿测试方法；系统认证操作不由这些合成事件证明。
