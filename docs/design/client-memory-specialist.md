# MEM-02：桌面客户端内存专项设计

2026-11-06。状态：**设计/单代理自审与核心优化已实施；最终内存对照见[对象审计结果](object-management-audit.md#mem-04发行实现结果本地windows)**。当前验证事实统一见 PROGRESS。延续 [PERF-01](performance-and-memory.md)，落实 NFR-01～08、P-01/02/03/05/08/10/12/13/22 和 [新增能力标准](new-capability-standard.md)。

## 1. 问题与功能定义

用户确认主场景：**主窗口＋任务小窗，日常少量任务，观察到300多MB**。本专项首先降低此场景的常驻成本，其次降低仅球/托盘的驻留和长期增长，最后处理大资料/同步峰值。不能用改为C++、Debug/Release、减少任务数量或清空工作集作为解释或交付。

Qt/C++不是内存上限承诺：产品实际使用Qt Quick/QML、字体、场景图与Windows图形驱动。必须区分私有工作集、全部工作集、私有提交、GPU资源及峰值。既不能据300MB直接判泄漏，也不能据“Qt Quick会用缓存”认定无需优化。

成功：同一构建/数据/显示配置下达到预算，数据/提醒/同步/窗口交互不退化；休眠后恢复具有确定性；连续工作流容量有界。非目标：本轮不重写Qt Widgets、不更换Qt6.10.2、不升级协议/schema、不新增后台服务、不改Go认证成本、不默认联网遥测。

## 2. 当前证据与根因假设

### 2.1 原生短测（已测，不是最终验收）

使用2.1.0本地签名候选的平铺payload，Qt6.10.2/MinGW13.1 Release，20条ACTIVE/今天的合成任务，QTemporaryDir；Windows原生窗口，各场景独立进程20秒，取后10秒采样中位数。清除Qt/QML/DAYMARK/SSL环境并限制子进程PATH到Windows目录，程序从自身目录加载Qt。不打开用户库，不读取用户账号。

| 场景 | 可见顶层窗 | 私有提交中位数 | 私有工作集中位数 | 全工作集中位数 | 采样私有提交峰值 |
|---|---:|---:|---:|---:|---:|
| 仅主窗口 | 1 | 244.71 MiB | 200.04 MiB | 262.85 MiB | 244.71 MiB |
| 主窗口＋小窗 | 2 | 297.05 MiB | 241.52 MiB | 305.88 MiB | 305.36 MiB |

两次独立短测差约52.34 MiB私有提交，是**开小窗整个路径的增量**，不是小窗纹理大小，不可全部归因于GPU、QML对象或图标。20条任务即可出现此量级，优先调查固定界面/图形成本；仍不排除其余模块占用。

每场景只有一次，顺序固定，未采实际DPR/具体graphics API/GPU专用/共享分配，无导航和长期流程；不能证明长期无泄漏，也不能当优化前后统计结论。采样峰值不是OS记录的真实瞬时峰值。没有直接读取任务管理器的列定义，不把本表私有提交冒充用户看到的那一列。flat payload未启用managed在线更新，正式基线须补安装版和更新子进程。

相对路径：build/mem02-probe/baseline.json，诊断源码/probe.cpp和CMakeLists.txt同目录；它是临时分析工具，非生产入口。脱敏摘要及样本见 [证据](../evidence/memory-specialist-baseline.json)。应用SHA256记录在证据中。首个夹具用了错误的notes列名，修正为当前note_text后才形成有效短测；失败日志保留，不记有效样本。

### 2.2 已确认的代码事实与待证实收益

| 事实 | 位置 | 判断/下一验证 |
|---|---|---|
| 共享Glyph对每个图标启用layer，4x采样 | src/ui/style/Glyph.qml | GPU离屏资源、绘制批处理优先候选；需统计实际rendered layer数量/物理尺寸及CPU/GPU增量，尚未证明是300MB主因 |
| BrandMark/Wordmark使用8x离屏采样；字标随颜色重建Loader | src/ui/BrandMark.qml、BrandWordmark.qml | 共享渲染策略及主题切换容量审查；不改母版形状/配色，不手改生成路径 |
| 账号、设置、独立日历、小窗、提醒窗口在Main中直接实例化 | src/ui/Main.qml | 未打开的QML对象已经存在；不等于每个隐藏窗口已创建交换链，须分辨对象与实际图形资源 |
| 设置四类内容以visible选择，均实例化 | src/ui/SettingsDialog.qml | 先做页面惰性创建，再评估整个窗口按需创建 |
| 主窗和独立日历各自拥有CalendarPanel；月历组件直接创建 | Main.qml、DesktopCalendarWindow.qml、CalendarPanel.qml | 独立日历没用时避免相关对象/绑定/投影；保留两个日历各自日期与尺度 |
| 日/周和年视图已有active Loader；连续月历已有reuseItems及有限cacheBuffer | CalendarPanel.qml、CalendarMonthStream.qml | 已有能力，不重复列为新增优化；查实际有效性与隐藏后的绑定活动 |
| 一份m_cache＋四个TaskListModel，部分QVariant投影 | TaskWorkspace.h/.cpp | 大资料容量候选；Qt隐式共享意味着不能简单算作四份完整字符串，小资料优先级低于渲染固定成本 |
| 一个QQmlApplicationEngine，稳定小窗原生画布 | AppRuntime.h、WidgetViewModel、WidgetWindow.qml | 保留，不新增每窗engine，不通过反复缩放原生窗口回退闪烁修复 |

PERF-01已改善查询/模型/提醒等算法；其4万条测试进程的内存收益不能代替本专项整应用驻留结论。

## 3. 技术选型与预算

固定Qt6.10.2公共API；Qt Quick/QML Profiler定位对象、分配和绑定，Windows WPR/WPA/VMMap按已发现热点分析私有堆、文件映射和图形栈。先用现有Win32进程计数；GPU采样缺失时为null＋原因，不能写0。不开常驻Profiler，不新增发行依赖。

渲染选型必须A/B/C对照：A现有Shape.GeometryRenderer＋逐项MSAA图层；B Qt支持的CurveRenderer直接抗锯齿；C保留原路径母版的共享栅格图标缓存。首选评估B，支持/视觉不满足时评估C，未对照前不在全局去掉layer或降画质。software后端、原生D3D、高对比和100/150/200%都要验；历史初帧/滚动裁剪故障是反例，不能删注释后盲改。

C若采用：既有路径按物理尺寸、DPR、颜色/主题代次与光学参数生成共享图像，QML使用可批处理Image；不引入外部图标资产。CPU缓存按字节LRU，首轮上限8 MiB，可见项可持有资源并单列统计，旧主题无活跃引用后可回收；不把CPU容量冒充GPU实际容量。可复用Qt内置绘制/SVG模块，新增链接模块时记录ADR/部署变化，不使用Qt私有场景图接口。

**2026-11-06 MEM-03修订：撤回下表作为当前验收预算；它仅保留为初稿历史。用户指出200MB级目标缺少对象成本依据，空Qt Quick窗口的本机对照已明显低于此量级。先完成[对象审计](object-management-audit.md)、消除不必要常驻并测剩余成本，再重定预算；不能用此表替现状辩护。**

以下是初稿中的**暂定目标，未达成且现已暂停采用**；MEM-02-A记录正式显示/硬件配置及Qt最小场景成本后冻结。主验收用20/100条混合任务、原生正常窗口、100%参考显示，实际用户DPI另列，不能靠切换后端拿较低数字。

| 场景 | 私有提交目标 | 私有工作集目标 | 其他约束 |
|---|---:|---:|---|
| 主窗口＋小窗，稳定空闲 | ≤220 MiB；后续争取≤180 | ≤190 MiB | 同配置较当前基线至少约25%提交下降作为首轮方向，最终按复测基线计算 |
| 仅主窗口 | ≤180 MiB | ≤150 MiB | 不提前创建低频工具内容 |
| 仅球/Mini，小窗及主内容已休眠 | ≤140 MiB | ≤110 MiB | 提醒、托盘、分类上下文保留 |
| 仅托盘，隐藏稳定60秒 | ≤110 MiB | ≤90 MiB | 不是调用OS trim工作集制造数值 |
| 100轮相同开关/导航，预热后回稳 | 净增≤10 MiB | 结合系统调页观察 | 对象/缓存/句柄有界；内存不完全回落不自动算泄漏 |

每一切片记录私有提交、私有/全工作集、可用GPU指标、对象/层/句柄数量和恢复延迟。若Qt/驱动对照基线已经逼近目标，先报告证据并调整取舍，不伪造收益，不先承诺50MB。

## 4. 目标架构与模块

仍是单进程、一个QML引擎、一套业务服务及Repository。组成：

- **ResourceProbe（诊断）**：平台计数器及场景标记，不参与业务状态；只在测试/用户显式诊断时开启。
- **SurfaceRegistry（C++应用协调）**：窗口/页面按需注册、异步创建去重、生命周期和代次；只管理呈现，不决定任务归属。
- **SurfaceHost（QML薄适配）**：保持open/close/visible等稳定意图接口，Loader承载实际工具页；UI不依赖尚未创建子控件的id。
- **RetentionPolicy（C++纯策略）**：Cold/Warm/Interactive状态、活动租约、回收条件；时钟可注入。只在隐藏稳定/内存压力/资料切换事件触发，不周期扫全树。
- **IconRenderPolicy/Cache（呈现层）**：共享质量策略、容量和后端回退，保持现有语义/母版。
- **ProjectionBudget（数据适配）**：按活跃视图维护ID/版本投影；QueryService仍是筛选唯一来源，Repository仍负责读取。

不将所有模块一次生成。先有测量和单项收益，再逐个引入所需接口；当前表是职责边界。

## 5. 生命周期与UX约束

Surface状态：Absent→Loading→Ready→Visible→HiddenWarm→Retiring→Absent；加载失败→Faulted，可从原入口重试。启动仅创建主窗必要内容＋已启用的当前桌面形态。设置先只建选中页；账号/关于/数据工具/独立日历首次使用再建。主窗根壳保持，现有单实例激活、托盘、更新重启通路继续有效。

Loading期间重复open合并，不生成重复窗口；关闭/资料切换递增代次，旧加载或后台结果不再接入新资料。AppRuntime不再依赖启动时findChild拿到全部低频窗口，采用注册通知和QPointer，WindowStateStore在实例注册时绑定、注销时释放。

禁止回收条件：任务/快速添加草稿、编辑/保存、IME组合、拖动、未处理提醒、活动菜单/弹层、键盘交互、Ball/Panel动效或待呈现帧。动画结束与帧栅栏确认后才可能转HiddenWarm。不得为了低内存丢选中、滚动、日历浏览日期、分类或撤销。

主窗/小窗不以同一策略一刀切：频繁Ball↔Panel保持稳定原生画布和热资源；先优化图标与未用页面。主窗长时间隐藏可优先尝试释放可重建图形资源，QML草稿仍保留；确认收益后再设计主内容休眠。账号关闭时按既有策略清理密码，密码不进入状态快照；未保存内容不能自动写入数据库来换取可回收。

HIG映射：连续性→保留上下文；用户控制→原入口恢复/错误可重试；清晰反馈→首次加载忙碌反馈且不吞点击；适应性→100～200%、高对比、减少动态和键盘仍可用。沿UX-09～13及现有菜单/画布契约验收。拟定恢复目标：热恢复≤150ms、冷工具首次打开≤500ms，主内容首次启动仍遵守NFR；目标机复测，不能牺牲恢复速度只交内存数字。

## 6. 接口与数据结构（设计契约）

```cpp
enum class SurfaceId { MainContent, WidgetPanel, Settings, Account,
                       DesktopCalendar, Reminder, DataTools };
enum class RetentionState { Absent, Loading, Ready, Visible,
                            HiddenWarm, Retiring, Faulted };
struct SurfaceToken { SurfaceId id; quint64 instance; quint64 profileEpoch; };
struct ResourceSample {
    qint64 monotonicMs;
    quint64 privateCommitBytes, workingSetBytes;
    std::optional<quint64> privateWorkingSetBytes;
    std::optional<quint64> dedicatedGpuBytes, sharedGpuBytes;
    int liveObjects, visibleWindows, activeLayers, liveHandles;
};
// GUI线程；返回受理身份，不等于完成创建。surfaceReady/Failed回调带token。
Result<SurfaceToken> requestOpen(SurfaceId, const OpenIntent&);
Result<void> requestClose(SurfaceToken, CloseDecision);
EvictionDecision canEvict(SurfaceToken, const InteractionSnapshot&) const;
void collectEligible(CleanupTrigger, MonotonicTime);
```

SurfaceLease为活动原因的RAII租约，最后一个租约释放且隐藏稳定后才允许回收。OpenIntent只传业务身份/版本及可重建UI状态，不持有QML指针/密码/整任务JSON。请求和回调必须比对instance/profileEpoch，QPointer失效直接取消。首轮不提供持久化Surface快照格式，不改变账号资料命名。

采样失败/低频窗口加载失败要有稳定错误key及Result；具体诊断号实现时从现有注册表分配，不在设计中重复硬编码。后台诊断失败不能让正常提醒/保存失败。公开诊断不含标题、账号、URL、用户路径、SQL内容或堆中字符串。

## 7. 协议、数据库与数据规模

无新RPC/服务器/网络协议；现有HTTP JSON v1保持。进程内意图/租约是实现接口，不作为公开插件ABI。诊断报告可使用本地format=1 JSON，字段可加、未知项可忽略；无默认上传。private/GPU/工作集字段不互相替代，不支持的指标带availability及原因。

schema10、migration、WAL、备份和事务保持。首轮不改SQLite缓存或索引。数据规模优化以1k/10k/40k混合任务另行测量，避免把微基准收益套用少量任务主场景。

大资料后续方向：唯一只读TaskSnapshot＋ID/版本投影，详情按需读取；旧快照在有界异步任务期间持有，资料切换失效。明确QString/QList隐式共享及detach成本，按实际分配验证；不得引入跨重载裸指针。循环实例、日历范围、搜索缓存必须有容量/键/失效表，写入、日期/时区、分类、账号代次均覆盖。长备注/同步JSON不得在不可见委托频繁转完整QVariantMap；导出/同步/备份仍完整。

同步和更新峰值分开：记录序列化前/中/后、reply/QProcess及队列释放；worker结束后不留驻后台更新器。既有传输大小限制不是可降低的内存预算；超限的批次/流式协议调整另做契约，不悄悄截数据。

## 8. 分层取证和优化顺序

| 切片 | 唯一问题/实现边界 | 通过条件 |
|---|---|---|
| MEM-02-A | 版本化诊断夹具、最小Qt/空Quick/Controls/产品的成本阶梯；20/100任务正式基线 | 5次独立运行、交错顺序、完整构建/显示/后端元数据，能解释指标，原始证据保留 |
| MEM-02-B | Glyph/品牌离屏层的A/B/C对照，选择共享渲染方案 | 主场景确有收益；像素/光学、裁剪、首帧、主题/高对比/软件后端和滚动不退化；旧方案可回退 |
| MEM-02-C | 设置按页创建→低频工具/独立日历惰性创建 | 未用内容对象为0；首次开/关/重开、失败重试、账号切换、焦点和状态一致 |
| MEM-02-D | 窗口隐藏资源策略；先长隐藏主窗，最后评估内容休眠 | 恢复延迟达标，连续帧无黑条/闪烁；草稿/IME/提醒/菜单/动效时不回收 |
| MEM-02-E | 快照、投影、长备注、循环/日历缓存；仅实测后实施 | 大资料内存增量有界，查询/CAS/排序/选中/滚动/快照异步安全保持 |
| MEM-02-F | 同步/导入/备份/更新瞬态及长期稳定性 | 峰值归因、完成后容量/进程释放；功能正确且恢复不退化 |
| MEM-02-G | 同配置前后总验收、安装包复验 | 主场景预算＋无关键交互回归；明示剩余平台和物理验收边界 |

若A证明另一个来源更大，可重排B～F并记录证据，不按表覆盖率盲改全部层。每切片先正反例测试、再最小改动、再同条件复测，失败保留，不反复跑整套寻找最好值。

## 9. 取证方案与可靠性

成本阶梯使用同Qt、编译器、动态链接、字体与显示后端：QtCore→QGui/空Quick→Controls→主壳→20条任务→小窗→各工具首次开关→隐藏60秒。阶梯用于归因，不把最小程序当产品目标。正式每场景5次进程，报告跨进程中位数/范围，不能把同进程80个采样当80次实验；样本少不报P95。

Windows通过GetProcessMemoryInfo取EX2，OS不支持时回退EX，私有工作集不可得则null；EX2要求较新补丁，不能提升最低系统版本。GPU由受支持工具单列；必要时WPR做短时间分配栈，不默认记录真实用户堆。Profiler只用于合成夹具，其自身开销不纳入最终低内存成绩。

正式夹具包含50%今天、分类/标签、10%长备注、完成/删除以及可控提醒；另有1k/10k/40k/长备注/多提醒和同步隔离数据。每次先就绪/首帧及活动数归零，再取稳定窗口；遇任务失败、少窗、少记录、后端错、资料加载错整次无效，不把提前退出当省内存。单独标记冷启动磁盘条件。

100轮重放预热20轮后，测后80轮回到同一场景30秒后的内存、对象、句柄和缓存容量，检查单调增长/斜率；先做30分钟再2小时，8小时实机另列，不以短测替代。QTemporaryDir内测试程序可结束自建进程，不结束用户实例。

禁止EmptyWorkingSet/SetProcessWorkingSetSize伪装改善；禁止定时全局GC/clearComponentCache、清空全部纹理、默认切software、降低KDF、丢任务、缩短日历可见范围、取消动效、改回小窗逐帧原生缩放。资源释放使用Qt公共接口，渲染线程生命周期信号不得在GUI线程直接释放QSG对象。

## 10. 单元、集成与端到端测试

- 策略单测：假时钟覆盖隐藏/恢复、冷暖、租约阻止回收、超时代次、内存压力、重复open、关窗后晚回调、快速切账号及加载失败重试。
- 图标单测/渲染：key/DPR/颜色/主题失效/LRU容量及活跃引用；代表性形状14/16/20/24/32px，浅深/高对比/100～200%，首帧、滚动裁剪、native/software；无视觉证据不能替换默认路径。
- 适配集成：真实QML对象生灭、WindowStateStore注册解除、EditorSession保存/失败、独立日历状态、主列表持久索引、隐藏时不再投影/刷新，提醒调度始终运行。
- 原生完整链：任务编辑/IME→关主窗到托盘→提醒→球开小窗→菜单/拖动→恢复主窗；关/开设置/账号/日历、负屏/混合DPI、更新RUNNING/重启及退出保存。重点复验此前透明菜单和收球末帧闪烁。
- 性能门禁：CI检验对象/容量/通知数量等确定性不变量与相对回归，不在共享runner硬卡跨机器RAM绝对值；目标机原生报告预算。每个优化分别确认CPU、帧延迟、峰值与驻留，没有以速度换低内存。

本轮只新增设计/脱敏证据，未更改生产运行逻辑。已构建临时诊断程序并完成上述两场原生短测；现有performance CTest 1/1 PASS（3.26s）。完整优化回归尚未开始，不把前一任务44项通过充作MEM-02实现验收。

## 11. 设计自审

单代理自审：范围聚焦已确认场景；已把度量错配、隐式共享、DPR/GPU归因、惰性加载失状态、旧profile回调、黑条/末帧闪烁、清缓存伪优化、Profiler扰动、一次短测过度推论列为约束。初稿容易把低频窗口创建等同图形设备分配、把小窗增量归图标，已明确修订为待归因。已有日/周年Loader和月历复用未重复认领。

结论：**设计可进入MEM-02-A/B对照实施，预算须A后冻结；尚未确认图标或任何单模块的真实节省量，不宣称优化完成**。用户当前要求从设计考虑，本轮停留在专项方案与初步取证，先供评审；不存在新增外部授权/依赖安装/发布动作。后续每次只推进一个切片。

## 12. 来源与交付

- [Qt6.10 Item layer开销及批处理限制](https://doc.qt.io/qt-6.10/qml-qtquick-item.html#memory-and-performance)：解释候选机制，不作为Daymark根因证据。
- [Qt6.10 QQuickWindow资源持久化/释放](https://doc.qt.io/qt-6.10/qquickwindow.html#resource-management)：默认保留及重新初始化代价；接口提示不保证特定后端一定释放。
- [Microsoft进程内存计数EX2](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex2)：计量含义、可用性及系统要求。

Qt在线6.10页面当前显示6.10.3文档；本项目仍固定6.10.2，具体实现以本地6.10.2头文件/运行测试为准，不据网页升级Kit。交付包括本设计、当前进度、脱敏原始样本；不提交临时库、用户日志、SDK和测量构建物，不自动推送或发布。
