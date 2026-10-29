# ADR-028：同步接口契约与RPC选型

日期：2026-10-28；2026-10-29按用户“根据架构完整实现”指令采用并实施。状态：**现行**，HTTP/JSON强契约，暂不引入gRPC。延续[ADR-026](026-go-sqlite-sync-service.md)的Go单体+SQLite与离线优先决定；验证状态只见[PROGRESS](../PROGRESS.md)。

## 背景与目标

用户提出：是否采用RPC，防止后续业务变化破坏接口兼容。需要保护的是已发布桌面客户端、离线冻结队列、长期回执和旧快照，不仅是在线函数能否调用。业务命令已有明显RPC语义：Push、Pull、GetReceipt、PublishImport；不把领域对象直接暴露为任意CRUD。

RPC是调用方式，gRPC是具体框架，Protobuf是IDL/序列化格式；OpenAPI同样可以为HTTP调用定义强契约并生成类型。更换框架不自动保证默认值、业务动作、枚举、事务边界或旧客户端语义兼容。

## 选项比较

| 方案 | 契约与演进能力 | Daymark当前成本/适配 | 结论 |
|---|---|---|---|
| HTTP/JSON命令接口 + OpenAPI 3.1 | 类型、可选/可空、oneOf、版本、SDK与差异检查；兼容规则需显式执行 | Qt Network可接入；Go标准HTTP；调试/分块下载简单 | **当前推荐** |
| gRPC + binary Protobuf | 字段号、IDL生成、未知字段保留与服务方法；仍需语义版本规则 | 增加protoc/插件/Qt客户端模块或gRPC C++运行时、构建与代理验证 | 出现明确收益时新增适配器 |
| Protobuf IDL + 自定义HTTP/ProtoJSON | 可共享IDL，但ProtoJSON的兼容保证不同于binary，未知字段/名称变动仍有要求 | 增加生成链，且要重新表达三态patch；没有自动获得gRPC全部收益 | 当前不引入第二套权威IDL |
| JSON-RPC 2.0万能method/params端点 | 调用封装统一，但没有自带业务schema或演进保证 | 仍需自定义IDL、认证/错误与大文件端口；收益有限 | 不采用 |

这是基于当前小体量、低性能需求和Qt桌面接入作出的工程判断，不是吞吐量实测结论。Qt GRPC客户端能力确实存在，但Qt 6.10系列官方文档列出protoc/生成插件，且模块许可为商业或GPLv3；不能直接沿用核心Qt模块的LGPL假设。若选它，须针对固定6.10.2 kit及项目分发方式另作依赖/许可评审；第三方gRPC C++路径亦需固定版本、许可及两平台构建验证。[Qt官方6.10文档](https://doc.qt.io/qt-6.10/qtgrpc-index.html)

## 建议决定

外部接口先使用**有强类型契约的HTTP/JSON命令接口**，业务层定义普通Go接口，进程内直接调用。OpenAPI是wire契约唯一来源；领域结构、SQL schema不作为网络协议。不同传输适配器复用同一用例、事务与回执，不为预留RPC拆微服务或增加内部网络调用。

```text
Qt SyncCoordinator → ISyncTransport → HTTP/JSON v1 adapter
                                     ↓ strict codec
                           Auth / Sync / Transfer use cases
                                     ↓
                              domain + SQLite ports
```

后续gRPC适配器只接到同一用例入口。上线期间相同mutation经两种传输只能产生同一回执/批次；鉴权、配额、deadline、取消及commit_unknown的含义相同。大块文件可继续走受鉴权HTTP下载；是否采用流式RPC另做失败恢复测试。

## 必须实现的兼容规则

1. `/v1`为协议主版本，固定DTO1的语义；capabilities发布supported_protocol_versions、supported_dto_versions、command_types及feature。业务请求协商`X-Daymark-DTO-Version`，响应回显。协商失败在读写事实前返回需升级错误。
2. 已有字段的名称、类型、单位、null/absent/default语义和已有命令行为不得在v1内改变。新增可选请求字段的默认必须严格等价于旧行为；新客户端只在服务端声明支持后发送，旧服务器严格拒绝未知字段。
3. 仅诊断/显示用的响应元数据可忽略；放在显式`metadata`对象内，客户端不得将其回写或纳入业务摘要。业务实体新字段、命令/枚举新值不能当元数据跳过。旧客户端要么收到保证无损语义的旧DTO投影，要么明确停止同步并保留本地资料；不能读不懂后继续推进cursor。
4. 调整任务状态、日期端点、循环身份、冲突/删除语义等属于行为变化，即使JSON/Protobuf仍能解码，也需新命令版本或主版本。新增枚举值先发布读端，确认能处理再启用写端；未知值不能默认为OPEN/false。
5. 已冻结的mutation保留原协议和规范编码规则。服务端维护旧版本解码/摘要/回执查询，不能部署后对同ID按新默认重算摘要。停用旧写端须提前公告，先做队列迁移/导出方案；不能把已有队列自动换ID重发。
6. schema migration、protocol、DTO、data_epoch、auth_epoch分别管理。数据库迁移不自动升协议，协议变化不自动重置epoch，epoch不拿来代替接口版本。

## 兼容CI门禁设计

首个v1发布时固化不可变`contracts/releases/v1.0.0`基线及合成fixtures；当前尚未发布，不伪造历史基线。以后每次改契约运行：

| 门禁 | 必须拒绝/验证 |
|---|---|
| Schema差异 | 删除/重命名字段或方法；可选变必填；可空变不可空；类型/单位变化；限制收紧；旧枚举值删除 |
| 旧请求→新服务 | 历史fixture、新服务codec/handler；旧默认/错误/CAS与日期规则一致 |
| 新响应→旧客户端 | 旧codec处理当前DTO1；仅metadata扩展可忽略；新业务版本必须明确升级而非半应用 |
| 跨版本行为 | 历史命令图与独立expected图、状态/循环/排序/删除副作用相同 |
| 离线重放 | 升级前冻结payload在升级后同ID返回原receipt；规范hash不漂移；支持期内receipt仍可查询 |
| 部署矩阵 | 前一受支持客户端×新服务、新客户端×前一受支持服务；feature关时不发送新命令 |

差异工具只能发现结构风险，行为fixture是独立门禁。SDK生成器必须固定版本并通过三态/union/大整数样本；不能将生成成功视为codec正确。未来若采用Protobuf，再增加buf breaking/lint及生成运行时版本矩阵，并遵守字段号不复用、删除字段reserved、显式presence及clear/set oneof规则。

Protobuf官方明确区分wire-safe与业务/应用兼容；ProtoJSON没有binary相同的演进保证。其deterministic serialization也不等于跨语言规范编码，因此幂等摘要继续使用本项目的版本化语义编码，不能换成直接hash protobuf bytes。[二进制演进](https://protobuf.dev/programming-guides/proto3/#updating)、[ProtoJSON](https://protobuf.dev/programming-guides/json/)、[规范编码说明](https://protobuf.dev/programming-guides/serialization-not-canonical/)

## 重评触发与迁移条件

出现多个独立客户端团队需要共享IDL、确有持续流式通信需求、或测量证明JSON处理成为瓶颈时，先做独立gRPC纵向POC：Qt6.10.2 Windows/macOS构建、三态/64位/日期、断线重试、代理TLS、认证、同mutation跨传输重放和许可全部过门，再决定采用。此时单体内部依然直接调用，服务拆分是另一个决定。
