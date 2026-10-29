# 同步系统：类型、接口与数据契约

日期：2026-10-28，2026-10-29实现基线。本文把[模块设计](../sync-modules/README.md)收敛为结构和方法；本目录Go/Qt声明仍是设计参考，不作为生产依赖。运行类型、事务端口和适配器分别位于`server/internal/domain`、`server/internal/ports`、`server/internal/protocol`及`src/sync`，入口见[server](../../../server/README.md)。用户授权完整实现后采用未登录本地、登录后同步、Go单体+SQLite及HTTP/JSON v1。[当前证据](../../PROGRESS.md)。

## 1. 权威文件与使用方法

| 产物 | 定义内容 | 使用边界 |
|---|---|---|
| [OpenAPI 3.1](../../api/sync-v1.openapi.json) | 路径、认证、请求/响应、字段必需/可空、13命令、实体、作业、错误 | wire唯一事实来源；默认仅支持DTO1 |
| [Go领域类型](go/model.go)、[命令](go/commands.go) | 强类型值、三态、完整实体、Graph/Plan、版本与复制消息 | 经过严格codec后的可信候选；仍须领域校验 |
| [Go wire类型](go/wire.go) | OpenAPI对应的具体JSON结构和封闭union | 设计期导出声明；**默认json.Marshal/Unmarshal不能替代Codec** |
| [Go端口](go/ports.go) | 用例、存储事务、鉴权、策略、产物、作业与运维依赖 | main注入；端口方向沿架构，不把该设计包作为生产大包 |
| [基础运行类型/端口](go/foundation.go) | 配置/限额/期限、迁移、实例锁、启动停止、管理员建号 | 默认及校验沿FND设计；秘密只通过SecretRef引用 |
| [Qt接口头文件](qt/SyncContracts.h) | 版本身份、异步上下文、传输/Repository/凭据接口 | C++20/Qt6.10.2语法契约；不是已接入QObject/worker |
| [服务器DDL](../sync-modules/server-schema-v1.sql) | 29个服务端表及约束 | 不是已交付migration；实施按DEV批次拆追加迁移 |
| [客户端元数据DDL](../sync-modules/client-sync-schema.sql) | 12个本地同步表、冻结/依赖/单在途约束 | 追加至当时桌面schema；升级前一致备份，不能直接对私人库执行 |
| [字段映射](field-mapping.md) | wire↔Go↔SQL↔现有Qt语义及跨字段约束 | 不序列化整个现有Task/查询投影 |
| [RPC与兼容决策](../../ADR/028-sync-api-contract-and-rpc.md) | HTTP/JSON命令接口选型、版本演进及兼容门禁 | 当前采用HTTP/JSON，不引入gRPC依赖 |

OpenAPI约束结构，字段映射/领域规则约束语义。遇到冲突先修契约和fixture，不让Go、Qt各自“合理解释”。`docs/`内可编译类型用于检查设计自洽；生产按domain/application/protocol/adapter职责独立实现，不依赖这个声明包。设计稿的Go1.23最低语法不代替生产工具链；实际实现固定Go1.27.0及ADR-029依赖。

## 2. 网络接口分组与调用边界

完整requestBody、parameters、response和security见OpenAPI；以下补充每类接口的前后置条件。每个带数据epoch的操作在实际读/写事务内核验它；job ID及mutation ID均不能代替授权。

| 接口/方法族 | 前置条件 | 成功后置条件 | 失败/重试 |
|---|---|---|---|
| capabilities | 无认证，只公开最小配置 | 返回实例/版本/命令/限制/feature，部署配置同源 | 未宣布feature不能调用；禁用功能无副作用拒绝 |
| login、refresh | 登录限流/KDF准入；refresh token真实有效 | 新会话或原子轮换pair；哈希持久化完成后返回token | refresh重放撤销必须先Commit再返回错误；不是callback返回error回滚 |
| logout、revokeDevice | 有效本人会话；禁止跨owner | 当前/指定会话撤销；同owner重复撤销成功 | 当前会话撤销后不能再用旧token请求；本地仍可编辑 |
| changePassword | 旧密码验证后，写事务重查credential_version | 修改hash、增加版本、撤销全部会话/未确认删除意图，204后重新登录 | 丢响应不能盲目反复旧密码；重登确定结果；无旧密码回显 |
| me、devices | 本人已认证 | 返回有权访问资料/会话；不返回hash/token | 不存在与无权对象统一安全404 |
| push | Scope/版本有效；冻结payload；完整read set；引用已seal且有效（若使用） | 业务/结构/head/完整batch/receipt一次Commit；200=持久成功 | 先查receipt再检查旧CAS/引用到期；未知Commit独立查receipt；同ID不得变语义 |
| getReceipt | 相同dataset/epoch，本人 | 200为原成功；404为当前未发现 | 404不证明在途未提交；重试同ID，不创造第二个动作 |
| pull | 有效签名cursor，after未被保留策略淘汰 | 同读视图head/floor/批次；每项给自己的after_cursor | 未知实体/DTO整批拒绝，游标不推进；末页给tail cursor |
| reportProgress | session的installation，applied<=当前head | 仅该安装的保留进度单调增加 | 不是确认本地已落库的替代；不能声明其他installation |
| create snapshot/staged/import | 配额、当前epoch、operation_id；参数摘要绑定key | 只持久接受job及空间预约 | 同key同参数返回同job，不同参数409；202不是业务已生效 |
| putChunk | UPLOADING，index/bytes/hash一致 | 文件先原子封存，DB再登记；重复同块幂等 | 文件/DB之间失败由reconciler清孤儿；业务零修改 |
| seal | 所有块完整、有序、无重复；payload满足专属schema | 冻结manifest、规范摘要与身份，转SEALED | 同manifest返回原结果；变内容新job |
| publishImport | SEALED、base_head未变、完整当前图重新验证 | 同事务业务+batch+receipt+job COMMITTED | head变返回import_rebase_required；内容/决定改变新mutation/job |
| manifest/chunk | 本人、同epoch、READY及有效租期 | 只暴露完整封存的清单/块；持久ReadLease保护读取期间 | 过期410；下载摘要错不推进cursor/切换资料 |
| renew/cancel/release | 当前job状态允许；不能越产物绝对期限 | 续期有上限；取消先停可用性；release在安全保存cutoff后释放pin | PUBLISHING/COMMITTED不能撤销业务；release不能冒充applied |
| account export | 本人/operation key，有限作业/空间 | 只导出业务及必要版本信息，排除凭据/执行历史 | GET状态→READY manifest→完整下载；保留期限有限 |
| deletion prepare/confirm/status | 见第5节双阶段、独立凭据 | 确认后立即禁用账号；账本持久化后可恢复清除 | 状态查询不依赖已撤销access token；未COMPLETED不显示删除完成 |

GET查询参数、Content-Length、摘要header均是解析的一部分，不因body已校验就信任。公网反向代理不暴露内部备份/恢复端口；health仅给安全状态，ready失败HTTP503，live仍活着可200。

## 3. 类型与Codec规则

`Revision/Seq`在Go是int64、Qt是不同包装类型；wire为规范十进制字符串，最大9223372036854775807。实体0只表示尚不存在，完整实体revision必须>0。wire日期和瞬间解析成功后转Date/Instant，不让JSON float、系统本地时区或空日期进入domain。

`Field<T>`满足三态：Unspecified没有值，Clear没有值，Set必须有合法值。wire字段缺失/JSON null/具体值分别转换；非可空字段收到null拒绝。tag_ids/links的null清空与[]空集合归一为相同领域结果，但冻结规范请求保留原明确输入；不能重试时改编码。TaskCreate只有外层title，initial不能再带title。CategorySave.parent_id只对新建有意义，更新不能借它移层。

Wire类型中的指针表示必填但可空值，`WireField`表示可缺失的输入字段；这两者不能互换。普通JSON默认解析会丢失字段存在性、忽略未知键，必须由Codec逐对象严格解析。union按type/kind映射具体结构；未知分支明确错误，不能落到`map[string]any`继续应用。只在协议边界/兼容胶囊保留RawMessage，领域用封闭Command/Schedule/Reminder/EntityValue。

所有response完整实体字段必须出现，空关系为[]而非null。metadata为可选诊断对象，仅允许无副作用的额外标量；不保存为业务base、不回写、不进入幂等hash。不能把未来业务事实塞进metadata绕过版本门禁。

同一错误只能由一个映射层转换：domain/storage error→Fault→Problem，HTTP状态与OpenAPI `x-error-status-map`一致。UI按code本地化；字段错误用JSON Pointer field_path。HTTP405/框架404也转换为安全Problem。冲突没有远端正文时先pull取得一致revision事实，不拼接多个时刻的对象。

## 4. 事务端口、取消与测试接缝

| 接缝 | 精确责任 | 测试观察点 |
|---|---|---|
| UnitOfWork.Read | 创建短读事务；回调结束关闭Rows/Tx | head/floor/batches来自同视图；不逃逸Tx |
| UnitOfWork.Write | 有界writer准入；BEGIN IMMEDIATE语义；callback error全回滚；返回提交分类 | NotCommitted/Committed/CommitUnknown区别；context已取消不能推断未Commit |
| WriteTx.ApplyPlan | 仅写当前Scope、已验证Plan；关联组件完整替换 | 最后一项SQL失败连同receipt/head全回滚 |
| AuthUnitOfWork | 账号/会话存储，KDF/随机数不占writer | refresh重放需持久撤销，再在事务外返回auth错误 |
| WorkspacePolicy | 纯读图→候选Plan，无时钟/SQL/随机副作用 | before图不变；同组同实体revision只加一次；NoOp无batch |
| Canonicalizer/CursorCodec | 固定版本语义编码/摘要、签名与绑定校验 | 16基准向量，tamper/错scope/expiry全拒绝 |
| TransferService/ArtifactStore/JobStore | 文件原子封存与DB状态分离；attempt/fence保护 | 文件后DB前崩溃、旧worker、读租期/清理竞争 |
| Qt Completion/Repository | 网络异步值不接触DB；worker写前再次核验generation | A账号旧回复不影响B；GUI仅Commit后收到ChangeSet |

当前Go设计端口以error表达错误，生产必须提供支持errors.Is/As的类型；不能把所有失败转换成一个bool。CommitOutcome本身不能携带未持久成功的Receipt；只有Commit成功或独立查到既有receipt才可向HTTP返回200。

应用服务使用构造函数显式注入依赖；单元替身应只实现该用例实际使用的窄接口。设计稿AuthTx/WriteTx是职责全集，实现时可拆组合接口；不让HTTP依赖sql.DB或domain依赖完整Store。对象输出值复制，禁止跨goroutine共享Tx、可变Graph或临时解码buffer。

### 4.1 回执、回声和分页检查点的补充裁决

**Receipt只有revision，没有完整业务事实。** acknowledge只持久记录sync_ack、版本义务并标outbox ACKNOWLEDGED，不能凭receipt更新sync_base正文或覆盖当前任务。依赖该动作的后继命令，必须等自己的echo完整批次已应用、或快照覆盖了对应seq并完成安全对账，才可基于新base冻结；no_op无需等待不存在的echo。这样服务器trim/循环副作用也能准确进入base。

自己的echo应用时：base更新为远端完整事实；如有更晚本地编辑，保留当前编辑与依赖意图，仅更新base。ack/echo身份直到applied越过相应batch才可清理。receipt.observed_head只是提示，不能直接写applied。原模块中“ack更新base”应按此裁决收窄，不推测没有返回的字段。

一页含多批时不能在落第一批后保存整页末尾游标。Pull的每项为`{batch,after_cursor}`；该检查点绑定本批seq和同一through。完整下载/验证引用批次后，本批事实/base/冲突/ack识别及它的after_cursor一次Commit。最后一项after_cursor等于next_cursor；空页单独CAS当前cursor并保持applied不变。崩溃只重放未完成批次，不会跳过页内后续变更。

## 5. 账号删除的独立能力凭据

1. `POST /v1/account/deletion-intents`：Bearer+旧密码重新认证，operation_id由客户端预分配。写INTENT、token SHA256、当前auth_epoch/credential_version、confirm_until=5min、状态凭据期限=30days。返回43字符随机token；token只在安全凭据存储，不进日志/任务库。此时不禁用账号。
2. 用户确认后，`DELETE /v1/account`带`Authorization: Deletion <token>`及operation_id。验证token/操作绑定、当前epoch/凭据版本、INTENT未到confirm_until；同库事务置DELETING、撤销全部会话、登记PENDING_LEDGER和删除job。API仅202及当前状态。
3. 确认响应丢失：客户端已有能力token，原operation重试返回当前状态；无需已撤销Bearer。`GET /v1/account/deletions/{operation_id}`只返回该操作状态，无资料下载能力。PENDING_LEDGER之后即使过确认窗口仍可幂等查询/重试；token超过状态期限不可用，后台账本/清理继续。
4. 独立删除账本按operation幂等写入且验证→LEDGER_CONFIRMED→可恢复清理→COMPLETED。账本失败保持账号禁用，不声称已经完成。灾备恢复先重放账本，再轮换epoch；旧token不能重新确认INTENT。
5. Prepare响应丢失时，同operation且仍INTENT、重新密码验证后可**轮换**token_hash并返回新token，旧token作废；不能声称从单向hash还原旧token。已开始删除则prepare不轮换，客户端用已保存token查询；没有token走运维身份核验流程，不开放通过ID查询私密信息。

UI确认/删除保留政策为后续产品切片，设计此接口不授权实际删除任何账号。密码变更/账号禁用取消尚未确认意图；status capability绝不接受为普通Bearer。30days到期可清除能力凭据，但删除防复活的最小账本记录按政策保留。

## 6. 开发与验收顺序

仍按[DEV-01～08](../sync-modules/README.md)及[76UT/8CT/22INT设计](../sync-modules/testing-integration.md)。先实现严格codec/领域测试和真实SQLite组件，再接HTTP、客户端；Go类型能编译只证明声明自洽。补充的契约检查不是服务运行测试：

| 静态/样本门禁 | 必须覆盖 |
|---|---|
| OpenAPI与JSON Schema | 引用、路径参数/operationId、所有13命令及响应必需字段；202/204/200语义 |
| 正例/反例 | 三态、空集合、64位边界、日期、union、只读字段、缺失event_at、跨资料字段、完整批次、删除能力 |
| SQL设计检查 | 临时库DDL/FK/NULL、冻结字段、依赖先后、单在途、epoch/资料隔离；不能冒充实际Go驱动验证 |
| 跨版本门禁 | ADR-028的旧客户端×新服务、冻结队列/receipt与行为fixture；发布后有真实基线 |
| Qt声明 | Qt6.10.2头文件语法、不同revision类型不可混用；线程/信号与OS凭据仍须真实切片测试 |

设计类型检查命令（在本目录`go/`运行；Windows/macOS均相同）：`go test ./...`、`go vet ./...`，无业务测试时必须说明`[no test files]`。Windows标准基线为`scripts/build-windows.ps1 -Preset windows-release`、`scripts/test-windows.ps1 -Preset windows-release -Suite fast`，先按工程文档配置kit运行路径；macOS为`bash scripts/build-macos.sh`及相应CTest，未运行标UNVERIFIED。详细设计不等于允许跳过已有Qt门禁或提前上线。
