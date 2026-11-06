# 模块、接口和数据契约

本节保留模块的设计契约；当前具体类/命令及与逻辑示例的差异见[实现操作](implementation.md)，源码入口为src/update。核心SDK纯C++20，不含Qt/GUI/SQL类型；内部静态链接，无稳定插件ABI承诺。类型示例省略访问控制实现；可信对象只能由验证工厂创建。

## 1. 公共类型

```cpp
namespace daymark::update {
using Bytes = std::vector<std::byte>;
using Digest = std::array<std::byte, 32>;
struct Version { std::uint32_t major, minor, patch; };
struct OperationId { std::array<std::byte, 16> bytes; };
enum class RetryClass { None, UserAction, AfterDeadline, ManualUpgrade };
struct UpdateError {
    std::string key;                // update.*，非本地化错误key
    RetryClass retry;
    std::optional<std::int64_t> retryAtUtcSeconds;
    std::optional<std::uint32_t> platformCode; // 日志脱敏后才公开
};
template<class T> using Outcome = std::variant<T, UpdateError>;
struct Done {};
enum class State {
    Idle, Deferred, Checking, Preparing, Downloading, Verifying, Staging,
    Ready, SavingAndClosing, Switching, RestrictedStartup, Running,
    Failed, ManualRequired, RolledBack
};
enum class CleanupState { NotEligible, Eligible, InProgress, Pending, Complete };
struct Snapshot {
    OperationId operation;
    std::uint64_t revision;
    State state;
    CleanupState cleanup;
    std::optional<Version> candidate;
    std::uint64_t receivedBytes;
    std::optional<std::uint64_t> totalBytes;
    bool canCancel, canRestart;
    std::optional<UpdateError> error;
};
}
```

UTC秒用于持久化期限；进程内预算只用单调时钟，系统时间回拨不能让一次请求无限延长。字符串协议为严格UTF-8；Windows路径在端口内部使用UTF-16，不从远端接收绝对目录。GitHub ID始终十进制字符串，不能经QML double转数值。哈希/nonce按固定长度验证，不复用taskId、账号token。

`VerifiedRelease`拥有严格验证后的版本、目标、schema/读写ABI、bootstrap范围、固定GitHub资产身份、ZIP散列/大小、索引散列、布局和相对payload路径；构造只在ManifestVerifier内。`PreparedUpdate`仅本地收据，不提升为信任根；使用前重新验签核对文件。`Outcome`只能返回完整值或错误，不用空值+ok布尔组合造成半成功。

## 2. SDK外部入口

```cpp
struct StartupContext;   // 已验证安装身份/当前编译版本/资料能力，不带密码
struct Preferences;
struct VerifiedRelease;
struct PreparedUpdate;
struct ExitHandoff;      // 平台受限句柄/管道能力，不是远端命令行
struct EventSink {
    virtual ~EventSink() = default;
    virtual void onSnapshot(const Snapshot&) = 0;
};
class UpdateSession {
public:
    Outcome<Done> check(const StartupContext&);       // 接受任务即返回
    Outcome<Done> prepare(const VerifiedRelease&);   // 自动发现路径使用
    Outcome<Done> cancel(OperationId expected);
    Outcome<Done> cancelPrepared(std::uint64_t expectedRevision);
    Outcome<Done> apply(const PreparedUpdate&, const ExitHandoff&);
    Outcome<Snapshot> snapshot() const;
    Outcome<Done> setPreferences(const Preferences&, std::uint64_t expectedRevision);
    // 成功/失败通过EventSink；接受任务不等于安装成功。
};
```

SDK不拥有UI事件循环。执行宿主在其工作队列驱动任务，回调来自该队列；UI适配器必须投递到自身主线程，不能在回调中直接操作QML/AppKit/GTK控件。进度最多10Hz；phase/terminal事件不节流。销毁/退订使用受控订阅令牌，停止接收后不再回调已销毁宿主。Snapshot拷贝有界，不含整个清单/文件列表/任务数据。

Windows CLI与GUI可以直接链接同SDK成为执行宿主；已存在执行者时另一个SDK Session走受限IPC观察/取消，不能抢锁重复执行。首期后台由CLI宿主执行，GUI只订阅即可；不增加常驻daemon。Qt主程序复用SDK规则与会话客户端，`QtUpdateAdapter`把Snapshot转属性、把Outcome错误映射既有Result/Error.key，不在Qt层重写策略。

## 3. 内部模块契约

| 模块 | 主要接口/输入输出 | 副作用/失败不变量 |
|---|---|---|
| VersionPolicy | `parseVersion(text)`、`compare(a,b)`、`evaluate(installed,verified,capabilities)` | 纯函数，溢出/预发行/跨runtime/schema/ABI/引导拒绝 |
| ReleaseDiscovery | `check(context,cancel,events)`→候选或完整检查结果 | 仅固定GitHub API，预算/限流/缓存；不把网络错当无更新 |
| ManifestVerifier | `verify(raw,signature,inventory)`→VerifiedRelease | 原始字节验签后严格解析；无磁盘安装操作 |
| PackageDownloader | `download(verified,receipt,cancel)`→完整ZIP收据 | 流式/断点/最终磁盘哈希；坏文件不继续Stage |
| ArchiveInspector | `inspect(zip,index,layout)`→经核对目录项 | 拒绝歧义路径/链接/大小超限/遗漏与额外文件 |
| PackageStager | `stage(zip,verified)`→PreparedUpdate | 写唯一同卷新目录、预检/索引、最后落盘Ready；不改current |
| StateStore | `load()`、`mutate(expectedRevision,change)` | 短锁+CAS+原生安全写；未知revision冲突不覆盖 |
| InstallCoordinator | `assess(prepared)`、`apply(prepared,handoff)` | 取得锁/等进程/受限启动/提交；只能本地授权路径 |
| StartupUpdateGate | `validateHandoff()`、`probeReadOnly()`、`activateCommitted()` | 这是Qt应用边界，COMMIT前不能走正常DB启动 |
| RecoveryPlanner | `plan(current,lastGood,journal,processFacts)` | 核心纯决策输出有限动作；平台执行器再复核并写盘 |
| VersionCleaner | `beginEligible(txn)`、`resumeCleanup(receipt)` | 只在RUNNING后清理，逐文件所有权校验；失败Pending |
| UiPresenter | `present(snapshot)`→文字key/动作可用性 | 共用状态语义，不决定版本资格/数据库行为 |

现有代码接点明确为：`AppRuntime::start`在调用`m_workspace.start`和构造`sync::Coordinator`之前选择启动模式；`SqliteDatabase`新增独立`probeForUpdate(path)`只读入口，不能给正常open加一个不完整的“跳过迁移”布尔就绕过备份/WAL等副作用；`ProfileIndex`增加只读枚举已管理资料路径/active的方法，限制在同资料根内、不跟随链接、不创建资料。受限模式的TaskWorkspace命令写门、提醒/重复/设置/同步延后启动要统一检查；构造函数若有副作用也必须移到获许可的start阶段。

内存/IO/解析异常在SDK边界转换为稳定错误，不跨模块抛到Qt/Win32回调。析构和取消不写current；不可完成的清理留有界Pending记录。平台资源用RAII，进程/锁句柄不得被无关子进程继承。

端口保持具体：`IHttpTransport`、`ICryptoVerifier`、`IInstallFiles`、`IProcessHost`、`IInstallLeases`、`IIpcChannel`、`IClock`。不创建通用DI容器或跨业务事件总线。每个端口错误必须传递稳定key与有限诊断，不以stdout打印后继续。

## 4. 网络/发布契约冻结

资源名与签名格式沿[总体方案](../windows-online-update.md)§5～7。核心规则唯一，HTTP由Qt/WinHTTP不同适配实现。资产获取接收200流或受控302；Range必须206与偏移/总量/ETag匹配，否则从零或失败。Release列表与资产不完整、分页超预算返回DiscoveryIncomplete，不能退化为“最新”。

FILE索引不列自身及两份bootstrap签名元数据；三者分别绑定/验签。`package.file_count`定义为ZIP所有普通文件总数（含上述三份），不含目录项；`unpacked_size`为所有普通文件实际字节和。`FILES.json.files`数量等于该总数减3，前提是三份固定元数据各存在且唯一。外部manifest只在ZIP上传后生成，签GitHub实际assetId/字节/散列；无散列循环。

FILES清单精确覆盖所有受管交付文件，不能只列exe遗漏DLL。版本目录/入口从固定布局与版本/source派生，`entry`只允许`daymark.exe`；远端不能下发安装根、shell、命令行或提权参数。正式包出现占位散列、未冻结runtime/最低系统或不完整门禁，发布工具必须失败。

## 5. 本地文件数据定义

| 文件 | 必需字段与语义 |
|---|---|
| preferences | format/revision、autoUpdateOnStartup默认true、lastAttempt/SuccessUtc、notBeforeUtc、cancelledVersion/Until；不存账号 |
| prepared-update | format/revision、transactionId/installId、expectedCurrentGeneration、versionKey、manifestHash、createdAt；存在不等于已切换 |
| current | format/generation/status=committed、installId、current相对路径、previous相对路径或null、transactionId；写门唯一权威 |
| journal | format/revision、transactionId、旧/新generation/路径、phase、manifestHash、旧/新进程身份、lastError；不能持久化nonce/继承HANDLE值当恢复能力 |
| running-receipt | transactionId/currentGeneration、映像路径身份、sourceSHA、资料正常初始化完成阶段、真实进程创建时间；只本机使用 |
| cleanup-receipt | transactionId、CleanupStarted标记、旧路径/索引hash、每文件已删/跳过/错误集合；不得包含用户文件内容 |

每个文件校验format、整数范围、深度、枚举及路径；未知格式停止自动写入。corruption checksum不是签名，篡改current不能让SDK跳过payload签名。原子write成功后返回的新revision是其他意图的CAS依据。临时写/提交失败保留最后有效状态，不用direct-write fallback。

## 6. IPC消息

管道仅当前用户、拒绝远程客户端、双向校验进程身份。handoff随机256位nonce通过明确继承句柄传递，命令行只含受限模式/句柄标识，不含口令/临时CDN URL。SDK进程身份为PID+创建时间+映像物理路径，PID本身不足。

帧为4字节小端长度+UTF-8 JSON，1～65536字节；每连接单调seq防重复，必须完整读帧，断开不拼接新连接残片。envelope字段：protocol=1、transaction_id、operation_id、seq、type、payload。seq只用有界精确整数，重连交换lastSnapshotRevision后从新连接序号开始，不回放已执行命令。

| 方向/type | 必需payload | 接收动作 |
|---|---|---|
| 双向HELLO | protocol范围、nonce响应、processIdentity | 不匹配关闭，不进入安装 |
| 主应用→宿主PREPARE | 清单/签名原始字节或本地受管引用、expectedGeneration | 宿主重新验签，幂等返回同operation |
| 宿主→观察者SNAPSHOT | 完整Snapshot | 只接收revision更高/当前operation |
| 观察者→宿主CANCEL | expectedOperation/revision | 仅可取消阶段有效，切换关键期返回Busy |
| 主应用→apply EXIT_HANDOFF | 已验证安装/资料上下文与旧进程能力 | 接手确认后才允许旧应用退出 |
| 新进程→apply READY | version/source/ABI、首帧/只读探测阶段 | 不表示已正常运行，不清理 |
| apply→新进程COMMIT | 持久current generation/transactionId | 新进程核对磁盘后才生产初始化 |
| 新进程→apply RUNNING | 正常初始化完成、generation、进程身份 | 持久化清理资格，再清旧版 |
| 双向ERROR | 稳定key/retryClass | 不是任意日志/脚本，脱敏展示 |

观察连接关闭不取消apply；执行者死亡的恢复以磁盘状态决定。nonce不当作网络认证token，也不允许复用到下一事务。

## 7. CLI功能定义

产品控制台宿主暂定`daymark-update-cli`，Windows后缀.exe。命令不接任意URL/未签名本地安装包/任意执行参数：

```text
daymark-update-cli check --json
daymark-update-cli prepare --json
daymark-update-cli status --json
daymark-update-cli cancel --operation <uuid> --json
daymark-update-cli cancel-ready --revision <integer> --json
daymark-update-cli apply --json
daymark-update-cli capabilities --json
```

安装根默认由自身位置解析；测试目标可注入隔离根，正式CLI只允许显式指定经同一身份/布局校验的已管理安装，不把目录参数当授权写任意位置。`apply`在有活动应用时通过它的保存退出协议请求重启，失败Busy/NeedsUserAction；不强杀、不绕过草稿，不提供`--force`。无活动应用则由正常根启动协议apply并启动新版。内部handoff模式不属于可伪造公共安装参数。

`--json` stdout为逐行完整JSON事件和唯一终态result，stderr仅无隐私诊断，不能把进度杂字混入stdout；非json面向人类。SIGINT/控制台取消事件在可取消阶段请求取消；apply关键区不直接异步打断，终态/退出码说明结果。检查默认不prepare；prepare包含发现/资格检查，apply仅用已有合法Ready。

退出码：0命令成功（有/无更新由result字段区分），2参数非法，10网络/超时，11限流/延期，20验签/哈希/包错误，21不兼容/需手动，30实例占用/草稿需处理，31空间/权限/IO，40切换/启动失败，41恢复需人工处理，50用户取消，60GUI不可用。参数和JSON事件契约做golden测试，不因前端文字翻译改变。
