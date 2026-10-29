# CLI：Qt本地同步客户端详细设计

本模块连接现有TaskWorkspace/TaskWorker、Task/Catalog/RecurrenceService及Repository，保持GUI模型线程和单DB worker。业务视图仍由QueryService决定；平台凭据和通知端口不进入QML或SQL。方法为拟新增C++契约，不是已存在API。

## 1. 类、线程和接口

| 类/端口 | 所属线程 | 方法/信号及责任 |
|---|---|---|
| SyncCoordinator | GUI/事件协调线程 | signIn/signOut/pause/resume/syncNow/onConnectivityChanged；单在途、退避、profile generation |
| HttpSyncTransport | 创建QNetworkAccessManager的线程 | login/refresh/push/pull/getReceipt/job；返回requestId和异步Result，不访问DB |
| SyncRepository | DB worker | capture/prepareNext/markInFlight/acknowledge/applyBatch/saveConflict/loadState |
| LocalMutationCollector | DB worker、现有外层命令事务 | BeginCommand/RecordChangedIDs/FinalizeJournal，命令意图与实际写集同Commit |
| RemoteApplyService | DB worker | applyBatch/applyReceipt/rebasePending，输出已提交ChangeSet |
| ProfileController | GUI协调、worker执行 | switchProfile/attachGuest/beginInitialMerge；保存门、关闭开库、失败恢复 |
| CredentialStore | src/platform宿主端口 | readPair/writePairAtomically/removePair，密钥按server+user+session隔离 |
| SyncStateModel | GUI | authState/transferState/pending/conflict/error/lastSuccess，QML只消费 |

网络方法的返回“已接收”不代表上传成功；使用operationFinished及已提交结果。所有请求/回复带ProfileID、ProfileGeneration、RequestID、MutationID（适用时）；任何回调先验证generation/current request，旧回复只能被丢弃，不能清当前错误或修改新资料。

```text
SyncRepository.prepareNext(ProfileID, Generation, Now) -> Result<optional<FrozenMutation>>
SyncRepository.markInFlight(ProfileID, Generation, MutationID, ExpectedHash) -> Result<void>
RemoteApplyService.applyReceipt(ProfileID, Generation, Receipt) -> Result<CommittedChangeSet>
RemoteApplyService.applyBatch(ProfileID, Generation, ExpectedCursor, CompleteBatch) -> Result<CommittedChangeSet>
RemoteApplyService.resolveConflict(ConflictID, Decision, ExpectedRemoteRevision) -> Result<NewMutationID>
ProfileController.switchProfile(TargetBinding, SavedDraftToken) -> asynchronous Result<ReadyProfile>
```

Generation在GUI收回复时和worker实际写入前都复查；ExpectedCursor在apply事务内CAS，重复seq<=applied可安全跳过，缺号不能跳。每个DeliveredBatch自带after_cursor，本批Commit仅保存该检查点，不能提前保存整页末尾cursor。一个普通同步cycle依次为必要pull→准备/推送→确认→补拉至本轮head→idle；认证刷新和普通传输都由协调器single-flight安排，文件分块的有界并发另受job预算，不让多个窗口各调一条链。强类型及补充裁决见[契约](../sync-contracts/README.md)。

## 2. 本地同步元数据及迁移

在当前schema9之后追加迁移，实际编号由实施时最新状态决定；升级前一致备份，所有自动测试QTemporaryDir。必须新增的逻辑表：

| 表 | 关键字段/约束 |
|---|---|
| sync_profiles | local_profile_id PK；server_instance,user_id,dataset_id,data_epoch可空；generation、auth_state；绑定后owner不可直接改 |
| sync_cursor | profile_id PK/FK、opaque_cursor、applied_seq、observed_head、epoch；仅批次Commit推进 |
| sync_base | profile,type,id复合PK，server_revision、canonical_snapshot、last_origin_mutation；是复制基线，非界面任务副本 |
| sync_outbox | mutation_id PK、local_seq唯一、profile、kind/state、intent_json、preconditions、dependency_ids、frozen_payload/hash可空、attempt/not_before；归属固定 |
| sync_ack | mutation_id PK、batch_seq可空、confirmed_revisions；applied跨过回声后可清理 |
| sync_conflicts | conflict_id PK、profile/group、base/local/remote、remote_revision、dependent_mutations、state；正文私有 |
| sync_incoming | profile/batchseq/chunkindex复合PK、hash/size/bytes、expected manifest；完成前不能驱动UI |
| reminder_execution | task_id+config_fingerprint、next_due/last_fired/history linkage；设备本地，不入outbox |

本地业务表不加server SQL语义，server_revision与原row_version分开。sync_base用规范DTO保留完整事实及兼容字段；Qt Task中的parentTitle/listDepth/projectName等投影不写进协议或base。

提醒迁移从现有anchor/repeat/zone与fired history恢复配置/执行游标：一次性无anchor时可用原remind_at作为锚点，循环缺少可恢复锚点则报迁移错误并保留旧库，不能凭空重置成现在。当前TaskSchedule.alertMinutes映射唯一RELATIVE配置；远端配置更改按fingerprint重算本地cursor，已展示历史不误当新任务重新提醒。

## 3. 本地写入捕获及入口清单

Collector参与最外层业务事务/savepoint，不在单条Repository信号里另开Commit。FinalizeJournal记录稳定event/group ID、规范命令意图、初始base/依赖、实际改变实体和结构；失败回滚业务和journal，成功后GUI才更新。

| 写入口 | 捕获命令 | 必须覆盖的实际副作用 |
|---|---|---|
| create/createWithDetails/createSubtask | task.create | 属性、schedule、tags/links、父/分类结构 |
| edit/saveTaskWithDetails/时间拖动/导航动作 | task.patch或move_tree | 统一日期端点、整个任务聚合、移属后代 |
| changeState/batch | task.change_state | 全树成员、完成/删除组、AFTER_DAYS后继 |
| Catalog save/drop/remove | category/tag命令 | 子分类提升、任务解属、标签关联批删 |
| TaskRepository.reorder/分类排序 | ordering.replace/relocate | 稳定排序scope与原上下文成员 |
| Recurrence save/stop/generate/writeRepeat | recurrence命令 | 规则代次、失效实例、稳定生成键和alias |
| JSON导入/恢复 | 初次IMPORT或显式导入组 | 全部业务及关系，不能只标tasks脏 |
| claimDueReminders | 不入业务outbox | 迁移后只写本机execution；用户改配置仍捕获 |

纯未绑定访客资料无需长期记录服务器命令历史，首次登录用一致快照导入；开始首次合并时在同一worker屏障启用后续变更捕获并记录cutoff。已绑定账号退出后仍记录该资料待传修改，重新登录相同账号续传；网络调用保持为0。

后台生成实例也是业务事实；进入Capture前已有Origin=Local/RemoteApply/Migration的明确上下文，RemoteApply不能回声入outbox。迁移重放和本机投递不能借“模型有变化”被当作用户命令上传。

## 4. 队列准备、冻结和确认

```text
pending --prepare+persist frozen payload--> in_flight
in_flight --network/5xx--> retry_wait --deadline--> in_flight (same bytes/ID)
in_flight --receipt--> acknowledged --> compact after own echo applied
pending/in_flight --conflict--> blocked_conflict
pending/in_flight --deterministic validation--> blocked_validation
```

prepareNext在worker事务中选择依赖已确认且相关echo已落base、未被冲突阻塞的最早命令（no_op不等echo），绑定目前可用的server基线版本、规范hash并冻结。前序未获确认时后继可保存意图，但不能伪造其server_revision。只允许尚未发送、无结构副作用的同任务普通文本patch合并；保留最初base，保留clear/set语义；状态/循环/树操作不做盲目“只保留最后值”压缩。

同profile最多一个push在途；401刷新single-flight后重发原业务mutation。若认证结果更新但响应使用旧access，优先检查是否已有更高凭据代次，避免重复refresh导致重放撤销。网络超时后先查回执或重传同冻结payload，不能生成新ID。

acknowledge在一笔本地事务中写sync_ack的版本义务、标outbox已确认；receipt没有完整实体，不能推测正文写base。相关echo落库才更新base；若任务在发送后又编辑，当前本地after值/rowVersion保留，后继意图在echo后相对新base重绑定。ack不推进pull cursor。sync_ack的mutation/batch身份保留到applied cursor越过其回声，no_op可直接压缩。

alias/canonical ID变化只可直接重写尚未发送的意图及本地引用。已经冻结的mutation不得就地改ID/hash；先查receipt确定旧请求结果，再按需要以新ID命令重建依赖，旧mutation保留审计/确认关联，防止同ID不同payload。

## 5. 拉取合并与冲突算法

applyBatch先确认epoch、完整manifest/摘要、seq连续性与支持的DTO类型，解析不支持则回滚整组。一个worker事务处理全部关联结果：

1. 对无待传目标，按完整远端聚合替换业务事实、更新base并增加本机rowVersion。
2. origin匹配sync_ack/在途mutation时确认自己的结果；把未发送后继叠回该base，保留更晚本地值；不能建伪冲突。
3. 远端其他mutation与本地未确认改动相交，保存三方及完整关联组冲突，更新远端base；本地候选保留，相关outbox阻塞。远端删除在主视图保持删除，本地内容在conflict恢复。
4. 更新结构/alias/排序映射和applied cursor，Commit后发ChangeSet、再调本机提醒维护；失败事实/冲突/cursor都不改变。

三方base必须是同一个远端revision的已确认快照；不能从当前UI投影拼造。冲突解决按钮产出新的规范意图和最新expected revision，原冲突历史保留到新命令确认；用户选择远端也要处理依赖本地create的子项/关系，不能只丢父项outbox制造悬空引用。

用户尚未提交的草稿不入outbox。RemoteApply只改持久事实/版本并发模型通知，TaskEditorSession仍保存原草稿版本；离开/保存按已有CAS失败保留输入。主窗/小窗/Mini复用同一事实，不能各起一个同步器。

## 6. 协调器、退出与资料切换

协调器分认证logged_out/authorizing/authenticated/reauth_required与传输local_only/idle/syncing/paused/offline/error，冲突数量独立。写入后1～2秒合并触发；前台30秒、托盘2分钟兜底；网络失败使用有上限指数退避+full jitter（初值1s、上限5min），429至少等待Retry-After。Scheduler/RandomJitter可测试，生产不依赖固定轮询sleep。

syncNow不越过认证/暂停/只读保护。成功状态要求当前epoch、无未确认/阻塞命令、无未完批次，applied>=本轮observed head；文案只表示截至该截点与服务器一致，不声称其他离线设备已收到。

signOut先取消网络/定时器并增加generation，尝试撤销服务会话、清凭据；本地事实/outbox不删，离线无法撤销远端时如实记录。退出应用停止新网络请求并等待已接受本地事务，未确认mutation由持久队列恢复。

switchProfile：现有草稿离开保存门成功→暂停旧请求、代次加一→drain本地写队列→worker关闭旧连接并打开目标独立库→完整性/必要迁移→新模型就绪。失败重新打开旧资料并返回可操作错误；模型在成功之前不清空为假新资料。已属于A的资料不能自动绑定B，访客首次连接才执行自动合并。

epoch变化/游标过期：先本地一致备份当前事实/队列/冲突，下载并验证临时快照，再对本地当前值（含已确认但服务器灾备可能丢失的事实）与新base对账。不能直接覆盖原库或把所有旧create重发。无法判断保留冲突，备份损失窗口显式展示。

## 7. 单元与组件测试契约

| 用例ID | 事件/故障 | 必须断言 |
|---|---|---|
| UT-CLI-01 | 未登录访客/已绑定退出后编辑 | 网络调用0，本地成功；绑定资料待传保持原owner |
| UT-CLI-02 | 写业务后FinalizeJournal失败 | 数据/关系/版本/队列全回滚，草稿保留 |
| UT-CLI-03 | m1发送中又编辑m2，然后ack m1再应用echo | ack仅记版本义务；echo后base推进、当前m2值不变，m2只在新base就绪后冻结 |
| UT-CLI-04 | 进程重启发现in_flight/timeout | 原mutation/hash恢复，不因发过而删除 |
| UT-CLI-05 | ack后pull自己的回声，再pull他端修改 | 自己无伪冲突，其他端按真实三方比较 |
| UT-CLI-06 | 同任务远端修改/删除与本地pending | 冲突组持久化、相关命令阻塞、其他任务继续 |
| UT-CLI-07 | 一组apply最后关系写失败/未知DTO | 事实/base/冲突/cursor整体不变 |
| UT-CLI-08 | 登出/切号后旧响应晚到 | 不修改新库、状态或凭据；无跨账号串传 |
| UT-CLI-09 | 并发401/凭据已刷新/安全存储失败 | 单刷新，无旧refresh盲重放；失败保持本地资料 |
| UT-CLI-10 | clock/scheduler推进、429/5xx | 退避上限/jitter/Retry-After正确，无忙轮询 |
| UT-CLI-11 | 提醒已触发后同步/配置改变 | 无服务器执行状态写入，不重放已显示提醒 |
| UT-CLI-12 | 快照中继续本地编辑/epoch恢复 | cutoff后编辑不丢，旧库/队列可恢复，不全量盲覆盖 |
| UT-CLI-13 | 草稿保存CAS失败/切资料失败 | 保留输入/焦点/原资料，失败不显示成功 |
| UT-CLI-14 | alias变化时有已冻结请求、重复/乱序batch | 冻结payload不改，先查回执；重复不重放，缺号游标不动 |
| CT-CLI-01 | 真QTemporaryDir库捕获所有入口 | 业务写集完整，RemoteApply/Migration/投递不回声入队 |

状态机纯UT用ScriptedTransport/FakeClock和受控worker；本地事务组件测试用真实Qt SQL。INT-10～15再连真实Go服务、网络和两份资料；OS凭据/通知另跑Windows/macOS实机，不用Fake证明。
