# Daymark同步协议v1

日期：2026-10-28，2026-10-29实现基线。服务入口及运维见[server](../../server/README.md)；v1路径/字段由共享样本、规范编码向量及真实双端测试保护。[架构与事务责任](../design/server-architecture.md)、[需求和产品取舍](../design/account-and-sync-service.md)、[当前证据](../PROGRESS.md)分别维护，不以JSON示例证明服务可用。示例ID、token和文本均为合成数据。

路径/结构的权威机器契约为[OpenAPI 3.1](sync-v1.openapi.json)，详细字段/类型及补充裁决见[强类型契约](../design/sync-contracts/README.md)。[77个Schema正反例](fixtures/contract-v1.json)及16个规范编码向量由Go/Qt分别执行；[ADR-028](../ADR/028-sync-api-contract-and-rpc.md)解释RPC取舍和兼容门禁。修改机器契约后用`scripts/generate-sync-contract.py`生成副本，CI通过`--check`拒绝漂移。

## 1. 传输和兼容规则

- 生产HTTPS，认证为`Authorization: Bearer <opaque_access_token>`；账号/会话由服务器验证，客户端不得指定owner。dataset路径仍需检查归属。
- 业务响应JSON使用`application/json`；错误使用`application/problem+json`。凭据/个人数据响应`Cache-Control: no-store`；访问日志不记录body、token或游标值。
- `/v1`是兼容主版本；JSON请求带`protocol_version: 1`，capabilities之外业务请求带`X-Daymark-DTO-Version: 1`，服务成功响应回显。未知版本在业务操作前拒绝。新写字段/未知命令/重复JSON键/无效UTF-8不得静默忽略；仅显式metadata中的诊断扩展允许旧客户端忽略，未知业务字段/类型停止推进游标并请求升级，或使用已保证语义的旧DTO投影。
- UUID为规范字符串；revision/seq/client_seq为非负十进制整数字符串，范围不超过signed int64；`0`只用于不存在的实体/初始游标位置。版本不经过浮点数。
- 日期`YYYY-MM-DD`；瞬间使用UTC `Z`并规范化毫秒精度；时区为IANA ID。全天结束日排他，用户日历日期不经UTC偏移。不能解析的时间/超范围数字明确拒绝。
- patch中缺失字段=不改，`null`=清空允许为空的字段，其他值=设定；必填字符串清空用合法空串或拒绝规则决定，不能把`null`与缺失合并。未知字段/非法关系零修改。
- 一个push只提交一个原子命令组，成功是业务已Commit；后台job接受响应仅表示作业已登记，不等于资料已导入或同步完成。

## 2. 接口职责

| 方法/路径 | 权限与语义 |
|---|---|
| `GET /v1/capabilities` | 最小公开能力、server instance及协议/大小限制；不公开账号与数据规模 |
| `POST /v1/auth/login` | 第一方认证候选，建立服务端会话；限流、不缓存、不记body |
| `POST /v1/auth/refresh`、`POST /v1/auth/logout` | 刷新轮换/当前会话撤销；不复用业务mutation回执 |
| `GET /v1/me`、`GET /v1/devices` | 本人账号/资料及会话，设备标签不是凭据 |
| `DELETE /v1/devices/{session_id}` | 撤销本人指定实际会话，重复撤销幂等 |
| `POST /v1/datasets/{dataset_id}/sync/push` | 已认证、当前epoch、完整命令组原子提交 |
| `GET /v1/datasets/{dataset_id}/sync/receipts/{mutation_id}?data_epoch=…` | 本资料回执查询；不可用他人mutation探测业务内容 |
| `GET /v1/datasets/{dataset_id}/sync/pull?cursor=…` | 完整已提交批次分页，固定截点 |
| `POST /v1/datasets/{dataset_id}/sync/progress` | body为data_epoch/applied_seq；installation由当前session取得，只作保留进度声明 |
| `POST /v1/datasets/{dataset_id}/snapshots` | 登记一致快照作业，202 + job_id；ready后才能消费manifest |
| `GET /v1/datasets/{dataset_id}/snapshots/{job_id}`及`GET /v1/datasets/{dataset_id}/jobs/{job_id}/chunks/{index}` | 同账号/资料、租期/摘要/大小校验；只导出该资料 |
| `POST /v1/datasets/{dataset_id}/staged-mutations` | 登记受配额限制的大组payload暂存；chunks上传并seal后返回不可变payload_ref |
| `POST /v1/datasets/{dataset_id}/imports` | 首次合并暂存作业；完整上传、seal校验并publish后才生效 |
| `GET /v1/datasets/{dataset_id}/jobs/{job_id}` | 本人作业状态/可操作错误；不因文件已生成就显示业务完成 |
| `POST /v1/account/exports`及相应状态/manifest/chunks | 个人业务导出，不包含凭据；完整路径见OpenAPI |
| `POST /v1/account/deletion-intents` | Bearer+密码准备操作并获得独立有限能力token，尚不删除 |
| `DELETE /v1/account`、`GET /v1/account/deletions/{operation_id}` | Deletion能力token确认/查询该操作；撤销Bearer后仍可幂等恢复结果，遵循产品确认和备份保留政策 |

健康检查是运维端口`/health/live`、`/health/ready`，不属于用户业务版本API。分块PUT/GET、seal、publish、renew、cancel、release完整路径及结构现见OpenAPI；未具备相应实现/门禁的服务不得在capabilities中宣称支持。

capabilities示例（限制是部署初值，非永久产品容量）：

```json
{
  "protocol_version": 1,
  "server_instance_id": "00000001-0000-4000-8000-000000000001",
  "supported_protocol_versions": [
    1
  ],
  "supported_dto_versions": [
    1
  ],
  "dto_version": 1,
  "command_types": [
    "task.create",
    "task.patch",
    "task.change_state",
    "task.move_tree",
    "category.save",
    "category.relocate",
    "category.remove",
    "tag.save",
    "tag.remove",
    "recurrence.replace",
    "recurrence.stop",
    "recurrence.materialize",
    "ordering.replace"
  ],
  "max_inline_bytes": 1048576,
  "max_inline_commands": 64,
  "max_inline_entities": 512,
  "max_chunk_bytes": 262144,
  "max_job_bytes": "67108864",
  "max_batch_reassembly_bytes": "268435456",
  "features": {
    "staged_payloads": false,
    "snapshots": false,
    "imports": false,
    "account_export": false,
    "account_deletion": false
  }
}
```

## 3. push请求

以下是修改一条既有任务的完整命令组示例。dataset由URL给出；请求中的data_epoch必须与该资料一致。client_seq只用于本机排序/诊断，不作为认证凭据或幂等键。

```json
{
  "protocol_version": 1,
  "data_epoch": "22222222-2222-4222-8222-222222222222",
  "mutation_id": "33333333-3333-4333-8333-333333333333",
  "installation_id": "44444444-4444-4444-8444-444444444444",
  "client_seq": "18",
  "preconditions": {
    "entities": [
      {
        "type": "task",
        "id": "55555555-5555-4555-8555-555555555555",
        "revision": "3"
      }
    ],
    "structures": []
  },
  "commands": [
    {
      "type": "task.patch",
      "task_id": "55555555-5555-4555-8555-555555555555",
      "patch": {
        "title": "设计样例任务",
        "deadline_on": null
      },
      "event_at_utc": "2026-10-28T08:00:00.000Z"
    }
  ]
}
```

规则：

1. mutation ID在dataset内唯一，与登录会话解耦；同账号重新登录仍可查询旧回执。账号切换和data epoch变化不能复用旧Scope直接推送。
2. preconditions列出被修改/读取的不变量相关实体及结构版本；创建实体用revision `0`要求当前不存在。软删对象仍算存在，不允许用create复活。服务器验证完整read set，不盲信客户端漏列的成员。
3. 普通patch只改允许编辑的内容。状态、整树动作、分类移属、循环规则变化和排序用各自类型命令；服务器加载当前图计算完整效果，并验证树成员/集合结构版本。
4. 不接受客户端写server_revision/head/owner；创建/导入允许的原始时间按专门字段校验，客户端updated_at不能决定竞争胜负。
5. 规范请求摘要包含协议、dataset、epoch、mutation、installation/client_seq、preconditions和完整命令语义；排除认证token、request ID及传输压缩差异。采用类型化确定性编码、固定字段/集合规范及SHA-256；按字段定义保留有意义的命令顺序和用户文本，不自动NFC改写正文。Qt/Go必须有相同摘要样本。
6. 首次发送后冻结全部语义字段。同ID不同摘要返回409 `mutation_payload_mismatch`，即使base revision正好可用也不能继续执行。
7. 409后改变patch/base或解决冲突属于新命令，使用新mutation ID；5xx/丢响应的重试保持原ID及payload。

最小命令族：task.create/patch/change_state、task.move_tree、category.save/remove/relocate、tag.save/remove、recurrence.replace/stop/materialize、ordering.replace。每条带冻结event_at_utc；当前完整schema已在OpenAPI给出，read set及派生结果见领域模块，正式发布前完成跨语言/行为切片后冻结。不能用万能entity.upsert绕过契约。

可指导开发的方法/参数/read set/业务效果现统一细化于[领域模块](../design/sync-modules/workspace.md)，严格解析、确定性摘要和cursor算法见[账号/HTTP模块](../design/sync-modules/identity-http.md)。机器schema已提供正反例；不能用框架默认反序列化代替三态字段及完整关系契约。

固定循环新实例身份与系列/occurrence key一致；AFTER_DAYS完成及后继是同组；schedule/tag/link是任务聚合组件。服务器以完整结果生成日志，不能让另一设备重新执行副作用命令来推算变化。

大组不适合inline时，上传`{preconditions, commands}`的不可变暂存payload；seal校验完整摘要和配额后，push用`payload_ref`**替代**这两个inline字段，外层mutation/epoch/installation/client_seq保持固定。引用必须绑定本人dataset/epoch/mutation且未过期，服务器展开后使用同一规范请求摘要和事务路径。一个暂存payload不能被替换后复用相同引用；不支持该功能时明确413并保留本地队列。

已提交大组的重试先匹配成功回执，不能因暂存块后来过期而把成功改成410。回执保留引用身份、规范摘要和外层请求摘要，足以校验重试；未提交且引用过期时，重新暂存相同冻结语义允许更换传输引用，完整规范摘要保持不变，不生成第二次业务命令。

## 4. 成功回执与提交不确定

```json
{
  "protocol_version": 1,
  "mutation_id": "33333333-3333-4333-8333-333333333333",
  "data_epoch": "22222222-2222-4222-8222-222222222222",
  "result": "committed",
  "batch_seq": "28",
  "observed_head_seq": "28",
  "entity_revisions": [
    {
      "type": "task",
      "id": "55555555-5555-4555-8555-555555555555",
      "revision": "4"
    }
  ],
  "structure_revisions": [],
  "committed_at_utc": "2026-10-28T08:00:00.000Z"
}
```

成功HTTP 200只在Commit完成或查到同请求成功回执时返回；重试返回相同语义回执，传输request ID可变化。合法no_op也写幂等回执，`result=no_op`、`batch_seq=null`，不产生虚假日志。该回执可确认对应outbox，但**observed_head_seq绝不直接推进applied cursor**。receipt只含revision，不能凭它写完整base；相关后继等自己的echo落库后再冻结，no_op除外。

回执查询404仅表示此刻未读到本epoch的成功记录，可能仍有在途请求，不能据此换一个ID。客户端可重试原ID；writer会串行检查去重。Commit异常且无法查清时返回503 `commit_result_unknown`，不能承诺未写入。epoch改变则返回410并转快照/恢复对账，不把旧请求重新当成全新操作。

## 5. pull分页及实体结果

请求必须提供本资料cursor；首次建库从ready快照的cutoff取得起点，不用空游标请求“只返回最新”。cursor是服务器签名或等价防篡改的opaque值，绑定server/dataset/data epoch/after_seq/through_seq/版本，客户端不解析它来决定业务；cursor不是授权凭据，每次仍验证会话及归属。

下面示例含一个普通任务的完整结果候选。replace不能只给修改字段；每种类型的必需事实/可空组件在schema冻结时逐一列全。旧兼容元数据需显式映射并在导入样本中验证，不能因DTO未列出就默默丢弃。

```json
{
  "protocol_version": 1,
  "data_epoch": "22222222-2222-4222-8222-222222222222",
  "through_seq": "28",
  "batches": [
    {
      "batch": {
        "seq": "28",
        "origin_mutation_id": "33333333-3333-4333-8333-333333333333",
        "origin_installation_id": "44444444-4444-4444-8444-444444444444",
        "changes": [
          {
            "type": "task",
            "id": "55555555-5555-4555-8555-555555555555",
            "revision": "4",
            "operation": "replace",
            "value": {
              "id": "55555555-5555-4555-8555-555555555555",
              "title": "设计样例任务",
              "notes": "",
              "planning_state": "ACTIVE",
              "status": "OPEN",
              "today_focus": false,
              "evening": false,
              "pinned": false,
              "important": false,
              "category_id": null,
              "parent_task_id": null,
              "start_on": null,
              "deadline_on": null,
              "created_at_utc": "2026-10-28T07:00:00.000Z",
              "updated_at_utc": "2026-10-28T08:00:00.000Z",
              "completed_at_utc": null,
              "cancelled_at_utc": null,
              "deleted_at_utc": null,
              "deletion_group_id": null,
              "completion_group_id": null,
              "schedule": null,
              "reminder": null,
              "recurrence_id": null,
              "occurrence_on": null,
              "occurrence_key": null,
              "instance_overridden": false,
              "legacy_metadata": null,
              "tag_ids": [],
              "links": []
            }
          }
        ],
        "structure_revisions": [],
        "committed_at_utc": "2026-10-28T08:00:00.000Z",
        "aliases": [],
        "successor_edges": []
      },
      "after_cursor": "example-opaque-cursor"
    }
  ],
  "next_cursor": "example-opaque-cursor",
  "has_more": false
}
```

- 同页读取head/retained floor和批次处于同一短读快照，后续页固定through_seq。读到了其他设备更新head提示，仍需按批次补齐。
- 每个batches元素为`{batch,after_cursor}`；batch为完整不可变ChangeBatch或BatchRef，after_cursor仅指向本批。逐批Commit各自检查点，不能将整页next_cursor提前写在第一页内批次上；末项after_cursor等于next_cursor，空页单独CAS游标而applied不变。
- has_more=true时next cursor保持固定through；完成一轮后返回解除through上限的tail cursor，下一轮才可读取后续head，不能永久固定在旧截止点。
- 批次seq严格递增，按协议声明的连续事务序号消费；重复批次可识别并跳过，缺号/乱序/摘要错误不可推进游标。空页也必须给出合法next_cursor。
- change是不可变提交结果；replace是完整类型DTO，删除状态也随实体传输以支持最近删除/恢复。将来永久清除只剩身份tombstone时另有显式类型，不能把“页面未出现”当删除。
- 一页不能公开半个关联事务。超大批次可返回受作用域保护的batch_ref，先完整下载、校验、暂存，再一次落库；需要时由capabilities声明最大重组大小，无法处理则保持游标并提示升级/限制。
- 对每个完整批次，事实变化或持久冲突、base、自己的回执识别及applied cursor在同一SQLite事务完成。GUI模型通知只在Commit后发出。
- 未支持的实体类型/高版本业务结果不能静默跳过；回滚本批次，保留原队列，返回需升级状态。

## 6. 冲突与错误格式

```json
{
  "type": "urn:daymark:problem:revision-conflict",
  "title": "Revision conflict",
  "status": 409,
  "instance": "urn:daymark:request:66666666-6666-4666-8666-666666666666",
  "code": "revision_conflict",
  "retryable": false,
  "conflicts": [
    {
      "type": "task",
      "id": "55555555-5555-4555-8555-555555555555",
      "expected_revision": "3",
      "actual_revision": "4"
    }
  ]
}
```

遵循[RFC 9457](https://www.rfc-editor.org/rfc/rfc9457.html)的Problem Details及扩展字段；本地化以code/安全参数为依据，不把英文title当作UI契约。冲突回复只给当前用户有权访问的信息；正文/SQL不进入日志。获取remote实体时必须有相同epoch和revision的一致版本，版本又变则继续拉取，不能拼三份不同版本内容冒充三方比较。

| HTTP/机器码 | 客户端动作 |
|---|---|
| 400 malformed_request、422 validation_failed | 保留输入/队列，显示可修复问题；不定时盲重试 |
| 401 auth_expired | 最多一次串行刷新；失败重新登录 |
| 403 device_revoked/account_disabled | 停止网络同步，保留本地；不自动另建会话绕过撤销 |
| 409 revision_conflict/structure_conflict | 持久冲突，解决后新mutation |
| 409 mutation_payload_mismatch | 协议/客户端错误，停止该命令，不能改原payload强推 |
| 410 cursor_expired/epoch_changed/snapshot_expired | 走安全快照/重建；原outbox不删 |
| 413 payload_too_large | 暂存大组或明确限制；禁止半组提交 |
| 426 protocol_upgrade_required | 停止相关同步并提示升级，离线仍可用 |
| 429 rate_limited | 遵守Retry-After并抖动 |
| 503 busy/temporarily_unavailable/commit_result_unknown | 原mutation有界退避/查回执，未获确认不显示成功 |

冲突中base/local/remote保存在本地私有库。远端删除不能被旧离线update静默复活；恢复和另存是新命令。只阻塞冲突实体及依赖命令，其他命令继续。用服务端真实revision解决，不能使用本机rowVersion代替。

## 7. 快照、暂存与恢复兼容门禁

快照manifest含job_id、dataset_id、data_epoch、cutoff_seq、DTO/schema版本、总实体数/字节、每块索引/摘要、payload及manifest摘要、租期和由cutoff生成的resume_cursor；manifest只能在产物完整ready后下载。字段名/摘要descriptor规则见OpenAPI及字段映射。分块路径不接受客户端文件名；重复块同摘要幂等，不同摘要拒绝。

客户端验证scope/epoch/manifest/所有块后构造临时资料，完整外键检查并保存待传修改，再切换基线；文件摘要不是授权，也不能代替TLS/作用域检查。服务端为快照固定日志pin，确保下载后还能从cutoff续拉；pin/作业过期需显式返回410。

导入/暂存作业状态为uploading→sealed→publishing→committed，失败可为failed/cancelled/expired。sealed内容不可变，publish重试使用同作业ID/摘要并返回既有结果；普通数据发布仍同业务日志/回执事务，客户端未观察到committed前不能删原资料。发布时关系/版本/配额必须重查，不能信任上传时预检。

epoch变化后原有mutation回执可能来自丢失的备份区间，不可强行把本机旧队列整体标确认，也不可全量当新create。客户端先保存本地资料/待传，重新认证并取得新快照，按ID/服务器实体/删除凭据逐项对账，无法自动判断的保留为冲突；原本已确认但灾备丢失的数据按备份RPO及本地可恢复内容报告。

在协议切片冻结前，必须提供：请求/回执同ID重放、规范摘要跨语言、clear/unspecified、自己的回声+连续本地编辑、并发成员变化、日志压缩vs快照、epoch恢复以及旧客户端拒绝新实体的合成fixtures。当前Markdown示例仅验证格式和语义评审，不等价于上述契约测试。
