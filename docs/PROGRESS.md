# Daymark 当前进度

更新：2026-11-09。**2.2.3 已发布 / schema10 / HTTP JSON v1 / Qt6.10.2**。当前状态只维护在本页；原始过程、失败及修复保留于[历史记录](PROGRESS-HISTORY.md)。

## CHROME-02 / UX-80 顶区稳定与更新通知（2.2.3发行完成）

发行冻结源码`25a7f24a2ee2fe75965c60927c145b76165db091`：main MSVC42541171175 **52/52 PASS（470.40s）**及同步42541171311成功后才推注释标签v2.2.3（对象`733ffdf3abefb9b85392ee674bb9208d6b200046`）。正式候选42543020867 **52/52 PASS（484.22s）**、lint/包结构通过；签名/独立部署/发布42545413374成功，公开安装42545873062通过。

公网升级首轮42545868088在完成新Qt提交/清理后，观察器再次读取current.json遇到临时PermissionError；Engine按契约先写cleaned journal、再原子清除current.previous，脚本原先只在轮询内读取journal，随后直接读取current形成竞态。修复仅在测试工具中将两个一致记录一起放入原90s轮询，保留全部状态、版本、资料和程序错误断言；新增4个反例/边界，发布工具单测14/14通过。生产EXE、包、tag不修改，不增加超时；工具提交使用[skip ci]，随后重新验证同一公开2.2.2→2.2.3链路，不重复编译无变化的产品或覆盖资产。

### 2.2.3正式交付证据

- 生产源码及不可变标签见上文。[main42541171175](https://github.com/downdone/daymark/actions/runs/42541171175)与[正式候选42543020867](https://github.com/downdone/daymark/actions/runs/42543020867)各自完整52/52通过及lint/包结构通过；本地2.2.3版本构建及两项标题栏复验2/2、26.79s。完整发行身份见[证据](evidence/windows-release-2.2.3.json)，本地MinGW不混入正式MSVC资产。
- [签名、独立部署及发布42545413374](https://github.com/downdone/daymark/actions/runs/42545413374)成功，五程序固定指纹自签名、实际Qt/CRT来源和窗口启动、Setup/卸载器签名、包内安装验收及7项公开资产digest核对，ready JSON/签名最后上传。沿项目固定证书策略，尚非Windows公共发行者信任，无根证书导入。
- [匿名公开安装42545873062](https://github.com/downdone/daymark/actions/runs/42545873062)通过：Unicode/空格目录、注册、Qt RUNNING/模块来源、运行中拒绝、两次修复、旧程序清理和卸载保留资料；持久Qt日志**3 passed / 0 failed / 0 skipped，48.878s**。[证据](evidence/windows-installer-2.2.3.json)的日志SHA256、Setup SHA256与公开asset digest已逐项核对。
- [最终匿名公网升级42546731691](https://github.com/downdone/daymark/actions/runs/42546731691)通过：公开2.2.2→2.2.3、自动prepare、真实Qt保存退出/重启、持久COMMIT/RUNNING、旧exe删除，合成SQLite及未知/修改文件保留；前后各29个模块来自对应payload，无fixture或信任覆盖，30.703s（其中Qt更新22.483s）。[原始结构化证据](evidence/windows-update-2.2.2-to-2.2.3.json)由成功job直接提取，并绑定生产源码25a7f24和只读观察工具78b09b5；保留首轮失败，不移动标签或覆盖已签名资源。
- [便携包](https://github.com/downdone/daymark/releases/download/v2.2.3/Daymark-2.2.3-windows-x64.zip)：44,520,119字节，assetId725806851，SHA256 `aaea48caca621d8450f5b4abbc942f61e2ba79dd93816a4bfc832b2bb239b79c`。[完整安装包](https://github.com/downdone/daymark/releases/download/v2.2.3/Daymark-2.2.3-windows-x64-setup.exe)：43,623,712字节，assetId725806853，SHA256 `df701c1a89ac91294ded6a34c7a7680cf1fd30db7b17973505c076d2ceb76c00`。精确7项资产与边界见发行证据。
- README、状态、版本索引、设计及交付指南已同步；最后仅文档/证据提交按链接、元数据/隐私与diff校验，以[skip ci]避免再次完整编译相同产品代码。正式程序身份始终绑定v2.2.3源码25a7f24，观察工具及收尾提交不替换该身份。

用户反馈2.2.2切换收集箱/今天时整窗布局上下跳动、更新就绪提示与白色正文难以区分。本轮先复审主窗/设置/账号的顶区、正文和弹层预算，再修复与验证，不改已发布标签。根因是主窗header高度依赖可选副标题，设置header也依赖当前分类说明换行；更新Pane未定义通知表面。

方案沿HIG内容优先、一致性、清晰反馈与适应性（P-13/15/17/18，UX-79/80），见[设计与融合关系复审](design/title-bar-layout-stability.md)：主窗固定单行标题度量；辅助日期/搜索统计宽窗同行、窄窗/大字移入原页脚，不为空副标题占行。主窗字标、页标题、工具及窗口控制共用一级中心轴，按钮保持顶边热区；设置按分类标题首行、账号按品牌组合中心对齐，不强行统一三种窗口高度。设置按全部说明的最大自然高度稳定正文起点。通知采用正文右下惰性卡片，避开导航、快速输入和底部动作，实体蓝色表面、边界、图标、标题、版本分行及主次按钮；不抢焦点，稍后返回最近有效位置，原保存重启链不改。

先补坐标回归确认旧代码FAIL：收集箱→今天header **60→78 DIP**，11.63s用例明确失败。用户追加融合复审后完成整窗轴线、40次主窗页面、16次设置分类、账号同轴/登录注册和通知宽窄/浅深/200%/高对比截图。Release构建、QML lint（零警告）及214份文档链接校验通过。

完整52项CTest **51通过/1失败（481.32s）**，唯一失败是通知Enter重启意图未完成。补齐Enter/Return与非重复按键处理，并把重启派发移到主窗的稳定上下文，避免关闭惰性通知使处理上下文失效。三项复验中标题栏7.59s、启动39.51s通过，通知测试在键盘按下关闭按钮后出现辅助代码失效引用；修正pressKey为持有窗口QPointer，松键时不再读取销毁/脱离场景的按钮。最终两项 **title_bar_ui/title_bar_layout 2/2 PASS（25.03s）**，明确完成标记、40+16切换、不抢焦点、返回最近焦点、忙碌禁重启、释放和单次重启意图全部验证。保留失败及修复链，不把增量结果写成最终快照重新跑52/52；未重复无关用例。

最终CTest生成5张合成原生客户区截图，已复看主窗共轴、设置稳定说明预算和正文右下通知；[证据](evidence/title-bar-layout-stability.json)记录精确源码文件哈希、各轮PASS/FAIL与截图SHA256。此前直接GUI调用的默认Shell退出值不足以证明完整动作，本轮最终验收采用CTest真实进程退出、明确完成标记和截图，不以空日志判定成功。

新增独立title_bar_layout（45s保护/50sCTest），原title_bar_ui的30s/35s不改；先前合并探针验收接近原预算，现按覆盖范围拆开，不延长旧用例或删断言。所有输入为QTemporaryDir合成资料；通知UI夹具版本9.9.9不代表实际安装更新。以上本地证据的后续正式交付已完成，精确结果见本节发行记录。2.2.2标签/资产不修改；物理读屏、多屏/DPI及macOS仍UNVERIFIED。

## CHROME-01 / UX-79 Windows融合顶区与2.2.2发行（完成）

用户要求新增Daymark融合顶区并可切换系统原生，完成后发布新版本。功能覆盖主窗/设置/账号，按设备持久化，保留窗口外框、状态、焦点、草稿、关闭保护和更新写门；Win32非客户区适配隔离在platform。设计与单代理自审见[十二项契约](design/windows-title-bar.md)。2.2.2安装包、便携包和签名更新资源均已公开，实际安装与2.1.1→2.2.2公网更新通过。

2.2.0本地/main通过，标签候选48/50失败；2.2.1本地与标签候选51/51通过，但main新增生命周期用例50/51失败，已知ScrollPhysics目标销毁竞态，因此两版均无程序资产，空Release转草稿、原标签保留。2.2.2保留原有CalendarMetrics与周行值快照，卸载前显式detach滚动策略并清空目标、取消列表创建；全量探针只强制完成真实可见场景的控件，独立16种月/周/日/年×字号×立即/绘制后关闭用例保持零QML警告与旧页弱引用门禁。共享单例尺寸实验造成回归，已撤回未提交。

2.2.2 Release构建与QML lint通过（无lint警告），calendar_lifecycle/qml_startup/desktop_surfaces **3/3 PASS（100.33s）**，最终本地完整CTest **51/51 PASS（460.23s）**。冻结源码`13e68f064bda6e66df1bd2575fd8468b0e2367f1`已推送main，账号同步42473347105成功，MSVC完整构建42473347093 **51/51 PASS（477.13s）**及lint通过后，才创建并推送不可变注释标签`v2.2.2`（对象`c9ee81a7c520b6eb3c02ebda155fb54360c45a6e`）。源码Release42475170554成功，正式标签候选42475170289 **51/51 PASS（474.76s）**，lint、包结构及归档通过。本地完整套件退出时出现一次既有ToolTip销毁期height-null警告，不将完整进程宣称为零警告；独立日历16种生命周期用例的零警告门禁已通过。

### 2.2.2正式交付证据

- [签名与独立部署/发布42477036184](https://github.com/downdone/daymark/actions/runs/42477036184)成功：五程序固定指纹自签名、独立Windows实际Qt/CRT来源与启动、签名Setup/嵌入式卸载器、安装验收及7项资产digest核对，ready JSON/签名最后上传。证书沿项目固定策略，不导入根证书、不冒充Windows公共发行者信任。
- [实际匿名公网升级42477474916](https://github.com/downdone/daymark/actions/runs/42477474916)成功：公开2.1.1→2.2.2、自动prepare、生产Qt保存退出/重启、持久COMMIT/RUNNING、旧exe删除，未知/修改文件与合成SQLite保留；无fixture或信任覆盖。45.825s，其中Qt更新24.797s，前后各29个模块来自对应payload。[原始结构化证据](evidence/windows-update-2.1.1-to-2.2.2.json)直接提取成功job输出，身份与公开资产核对一致。
- [公开安装验收42477479819](https://github.com/downdone/daymark/actions/runs/42477479819)成功：匿名下载同一Setup、固定指纹及版本验证，QTemporaryDir真实Unicode/空格路径安装、注册、Qt RUNNING/模块审计、运行中拒绝、两次修复、旧程序清理和卸载保留资料；持久Qt日志**3 passed / 0 failed / 0 skipped，51.404s**。[安装证据](evidence/windows-installer-2.2.2.json)的日志SHA256和Setup哈希已与实际文件/公开digest核对。
- [便携包](https://github.com/downdone/daymark/releases/download/v2.2.2/Daymark-2.2.2-windows-x64.zip)：44,495,824字节，assetId723778672，SHA256 `754bc72bcde4bf7b514a64e023fc01a1a283053e619e85d783d22ee40aad3320`。[完整安装包](https://github.com/downdone/daymark/releases/download/v2.2.2/Daymark-2.2.2-windows-x64-setup.exe)：43,598,832字节，assetId723778677，SHA256 `fd671b5fd9b7a36d09e34526fb0a189e5f5e3b0d0dbf0506be9b9cf279c16d05`。完整身份、各门禁与其余5项资产见[本轮证据](evidence/windows-title-bar-2.2.2.json)。正式包由MSVC候选生成，本地MinGW构建不混入公开资产。
- 已更新使用说明、README、交付指南、当前状态和版本索引，清除旧“三平台CI/无安装器/未发布”描述。发行后的收尾提交只含文档与证据，执行文档链接、元数据/隐私和diff校验，使用`[skip ci]`避免再次完整编译同一功能源码；发行门禁始终绑定上述精确标签源码。

客户区截图只证明QML绘制，不把遮挡或黑屏当原生视觉PASS。物理多屏/DPI/读屏、Snap悬停及非Windows仍UNVERIFIED。

## MEM-04 优化实现与2.1.1发行（完成）

用户已授权完成自审、实现、测试、推送及新版本发布。单代理使用autonomous-project-loop；不改写已发布标签，不混用本地MinGW与正式MSVC产物。该轮正式Windows程序为2.1.1；生产源码为`76a74df801d334fa7c5db3cdaa87d9d81b438f5f`，标签`v2.1.1`已推送。

| 切片 | 实施与验收 |
|---|---|
| OBJ-01 选择器 | 日期/时间fallback在打开时创建、关闭动画结束后释放；inline不建fallback。隔离生命周期测试覆盖取消、选择、重开、键盘及200% |
| OBJ-02 共享菜单 | 主列表只持有一份菜单，打开时捕获taskId/version；不随当前选中项漂移，模型使目标失效时关闭 |
| OBJ-03 窗口与页面 | 设置仅创建当前可见分类，保留导航/滚动；独立日历保留轻根和窗口状态，关闭后卸载内容；隐藏工具窗释放可重建场景图。可见小窗保持持久画布，层级临时hide不触发回收 |
| OBJ-04 临时计划 | 取消导入立即失效UI代次、排队discard，切资料时清除旧计划；晚回调不恢复旧预览。真实临时SQLite覆盖取消零写、旧计划拒绝及重新预览成功 |
| OBJ-05 图形 | CurveRenderer因真实图标回归拒绝发布。保留Geometry/MSAA，只在颜色/路径变化时重建Shape；原生恢复图标像素检查与浅深两组6转换通过 |
| OBJ-06 回归与发行 | 本地Qt6.10.2 Release构建及QML lint无警告，完整CTest **48/48 PASS，453.41s**。GitHub main源码/桌面与Account/sync通过；正式MSVC候选完整48/48 PASS（446.89s）、QML lint/打包及五程序自签名通过。独立部署/安装发布、公网2.0.2→2.1.1及带持久日志的公开安装验收均PASS |

同机D3D11/DPR1.5、20条合成任务的原生34阶段，双窗启动可达对象**30,029→17,157**，私有提交**297.84→254.62MiB**、私有工作集**242.25→202.00MiB**；十次设置重开后提交**495.83→312.95MiB**，全部隐藏**500.52→204.01MiB**。完整对照见[对象审计](design/object-management-audit.md)和[脱敏证据](evidence/object-management-after.json)。这是本地最终Geometry实现的单轮阶段快照，不是正式MSVC资产测量、统计SLA、逐字节堆/GPU归因或跨机器内存保证。此前220/190MiB预算已撤回，2h/物理多屏/读屏及非Windows仍UNVERIFIED。

自审覆盖拥有者/销毁时机、草稿与焦点、菜单目标身份、异步代次、资料隔离和窗口呈现；不是独立评审。不使用定时trim/全GC，不改变schema10、HTTP JSON v1、Qt6.10.2及现有更新提交/清理语义。

发行过程失败如实保留：2.1.0在GitHub生成码门禁发现16个已登记setup错误码未同步Qt映射，未上传程序/ready资产；已取消过期构建，将空Release转草稿，原`v2.1.0`保留不移动。2.1.1补齐映射并增加两个生成契约CTest，桌面/标签编译均先通过源码门禁。正式发行与公网证据见本页发行表及下列记录，不以本地48项代替。

### 2.1.1正式交付证据

- 精确生产源码`76a74df801d334fa7c5db3cdaa87d9d81b438f5f`，不可变`v2.1.1`；[正式候选42389695899](https://github.com/downdone/daymark/actions/runs/42389695899)完整48/48、446.89s。main源码/桌面42389564764及同步42389564848均成功。
- [签名/独立部署/安装发布42392523180](https://github.com/downdone/daymark/actions/runs/42392523180)：五程序固定证书指纹验签、独立Windows实际原生窗口/Qt与CRT来源、签名Setup及嵌入式卸载器；7项公开资产逐个GitHub digest核对，ready JSON/签名最后上传。
- [匿名公网升级42393012685](https://github.com/downdone/daymark/actions/runs/42393012685)：2.0.2→2.1.1、自动prepare、生产Qt保存退出/重启、持久COMMIT/RUNNING、旧exe删除、未知/修改文件及合成SQLite保留；无fixture/信任/API覆盖、无账号/资料上传。完整32.632s，其中Qt更新22.825s；前后各29个模块来自自己的payload。[结构化证据](evidence/windows-update-2.0.2-to-2.1.1.json)由成功job直接输出提取，未依赖86MB程序归档下载。
- [公开安装最终验收42394028927](https://github.com/downdone/daymark/actions/runs/42394028927)：匿名下载同一Setup、固定证书/版本验证，QTemporaryDir真实Unicode/空格路径安装、注册、Qt RUNNING与模块来源、运行中拒绝、两次修复、旧程序清理、卸载保留资料。Qt自身持久文件日志1007字符，实际完成标记及**3 passed / 0 failed / 0 skipped，44.992s**均验证；[证据](evidence/windows-installer-2.1.1.json)绑定安装包与日志SHA256。首次空stdout门禁不足已如实记录并修正，最终不再凭空日志判断通过。
- [正式便携包](https://github.com/downdone/daymark/releases/download/v2.1.1/Daymark-2.1.1-windows-x64.zip)：44,442,698字节，assetId721283811，SHA256 `b6c98a00b18414612037aee187502dd462464769865c5817e92518d2bccff98e`。[安装包](https://github.com/downdone/daymark/releases/download/v2.1.1/Daymark-2.1.1-windows-x64-setup.exe)：43,549,648字节，assetId721283816，SHA256 `ad200a8024067a3d46deb60af70cc5cbaa3ef8e3ac52aa2bbc53de3e7dea1a0a`。
- 本机两项CDN下载180秒有界超时，分别只收到7,355,715/130,365字节，已标为.part且不分发；没有把它们当成功本地包，未反复重试。正式公开包及独立匿名下载/安装/更新均成功。本地内存测量、正式MSVC和其他机器现场仍分别归因。

## MEM-03 对象管理全面审计（分析与诊断实现完成）

用户要求重新审视200MB级占用，已暂停MEM-02初稿预算，不作为合格线。完整[对象审计](design/object-management-audit.md)覆盖QML/窗口/选择器/编辑/菜单/图标/数据模型/线程/同步/导入预览/更新/日志持有链、销毁时机和风险；[脱敏证据](evidence/object-management-audit.json)含实际快照，不能把可达QObject清点等同全堆字节归因。

新增显式`--object-audit-report <新路径>`隔离诊断：QTemporaryDir、TaskService预建20条任务、34检查点，已有报告拒绝覆盖，禁用用户data-dir；正常启动不采样。新参数已接入日志隔离。原生D3D11/DPR1.5：启动可达QObject30,029/QuickItem13,455，当前可见链候选647；未打开独立日历仍有6,581对象，设置1,413、账号343。39个CalendarPicker（3159子树对象）、32个DateField、3套TaskTimingFields，主任务行11个/单行最多495对象，小窗行5个/单行78。子树统计重叠，不能相加；1169离屏源对象不代表1169已分配纹理。Task固定结构1296字节，少量任务的约四套固定结构约101KiB，不含字符串/JS堆。

同Qt的空QQuickWindow独立短测：单窗58.32MiB私有提交，双窗89.20MiB（无QML/Controls/字体/业务，不是同功能对照）。完整客户端初始297.84MiB，首次设置/账号/独立日历后依次约367/434/496MiB，隐藏后不明显回落且sceneGraph仍初始化；十次设置重开对象数稳定。最后仅球/全部隐藏仍29595对象、500.52MiB；单次诊断GC不显著下降。结论是过度创建＋首次使用后图形资源长驻，未证明持续泄漏或逐字节驱动归因，不宣称优化完成。

已实现/构建诊断和真实集成测试：初轮object_lifecycle_audit、widget_pointer_ui、performance **3/3 PASS，77.34s**。日志隔离修正后的最终诊断定向复验 **1/1 PASS，70.59s**；真实Windows完整34阶段PASS，日志/资料均临时；runtime_diagnostics另1/1 PASS（0.83s），QML lint、证据结构/隐私及diff检查PASS。原型热插入/混合DPI及私有ItemLayer枚举不足的记录保留在build，不替代最终证据。堆分配栈、GPU字节、2h/物理多屏等仍UNVERIFIED。暂未改生产UI/业务/schema/协议，不自动推送/发布。

下一优先级OBJ-01闲置日期/编辑内容按需创建，OBJ-02主列表共享菜单，OBJ-03低频窗口生命周期；同时定义临时ImportPlan的显式discard/profile代次。不要先重做SQL，也不通过trim/定时全GC伪装低内存。

## MEM-02 内存专项（设计与初步取证完成，优化未实施）

用户确认主窗口＋小窗/少量任务出现300多MB。已完成[专项设计](design/client-memory-specialist.md)，按十二项标准覆盖目标、选型、架构、生命周期/接口、协议/数据库、UX/可靠性、分层验证与单代理自审。先测量/归因，再图标离屏层对照、低频UI惰性创建、隐藏资源、投影/同步峰值，逐片实施；未改生产代码，不宣称已优化或已定位唯一根因。

同一个2.1.0本地Release平铺payload，QTemporaryDir合成20条今天任务，真实Windows、清SDK环境，各场景独立20秒，后10秒采样中位数：仅主窗私有提交244.71MiB/全工作集262.85MiB；主窗＋小窗私有提交297.05MiB/私有工作集241.52MiB/全工作集305.88MiB，采样提交峰值305.36MiB。可见顶层窗分别1/2。每场景仅1次、无DPR/实际图形API/GPU/长期测量，不是最终SLA或前后优化证据，不能把约52.34MiB开窗路径增量全部归GPU/图标。脱敏摘要及样本见[证据](evidence/memory-specialist-baseline.json)。

代码确认Glyph逐项layer＋4x采样，品牌8x；设置四类内容同时创建，主窗/独立日历及多个工具直接实例化。它们是优先候选，收益尚未测；日/周年Loader和连续月历复用已有，不重复认领。方案保留稳定画布、菜单层级、输入草稿/IME、提醒及同步，不采用工作集trim或定时全清缓存。拟定主场景提交≤220MiB/私有工作集≤190MiB等目标，正式成本阶梯/5次原生基线后冻结，不作跨机器硬保证。

临时诊断程序构建PASS，两场有效短测PASS；初次夹具列名不匹配已修正，失败不记样本。当前performance CTest 1/1 PASS（3.26s）；未重跑无关整套测试，前一任务44项不充作本专项实现验收。下一切片MEM-02-A正式可重复夹具/Qt最小成本阶梯，随后B共享图标渲染对照；本轮方案供用户评审，不自动推送/发布。

## WID-08 / SETUP-01（实现通过，实际安装包验收中）

按用户授权修复Mini小太阳及置顶拖动，单代理执行新增能力标准：[指针设计](design/widget-pointer-move.md)、[安装器契约](design/windows-installer.md)、[ADR-035](ADR/035-native-windows-installer.md)均完成自审，没有冒称独立评审。开发候选版本2.1.0，线上正式版本仍2.0.2；不自动推送、发布或打tag。

WID-08：Mini太阳统一执行主要动作，常驻Mini也可展开；Panel/Mini/Ball共用单一DragHandler→C++位移，不再在QML抓取中转交startSystemMove。位移不改窗口层级，锁定/穿透结束拖动，可再次解锁拖动。实际QML Mini点击与Mini/置顶Panel拖动、锁定恢复测试PASS；物理多显示器跨DPI、用户现场偶发问题仍UNVERIFIED。

SETUP-01：每用户原生NSIS向导，签名managed布局、根宿主/Qt完整离线部署、HKCU/快捷方式/卸载、安装SDK JSON/稳定错误码/Win32细节。可信索引验签及全文件散列、运行占用拒绝、外安装互斥、同卷暂存/回滚与崩溃恢复、损坏程序修复、反复重装备份链、RUNNING后清理、保留未知/修改文件。数据库schema10/HTTP协议不变，无新服务/系统服务/Root证书导入。正式CI门禁保持，本地候选不生成正式更新manifest，不把本机MinGW冒充MSVC线上升级。

构建PASS。全量CTest **44/44 PASS，428.72s**（本轮中间快照）；最终安装SDK调整后针对性 **7/7 PASS，44.63s**，实际Mini/置顶Panel QML拖动最终 **1/1 PASS，4.05s**。首次Mini探针因零时间戳的合成鼠标事件未触发DragHandler，修正测试事件时间戳后通过；安装事务首轮Windows内部句柄阻止目录改名已通过外锁重设计修复。一次旧SDK构建对应更新后“损坏程序修复”夹具失败，最终重建后7/7通过。实际签名Setup/向导/安装启动/重装/卸载验收待产物生成，不提前写PASS。独立干净Windows、物理多屏/读屏以及macOS/Linux继续UNVERIFIED。

## TIME-01 智能时间输入（本地完成，未发布）

用户授权设计评审后全面实现，并要求固化新增能力工程标准。已将[统一设计标准](design/new-capability-standard.md)和[模板](templates/capability-design.md)接入AGENTS、engineering、工程原则P-22；后续新增能力默认覆盖功能定义、选型、架构、模块、协议、接口/数据、数据库、UX、可靠性、测试及评审，不涉及项说明原因。[TIME-01契约](design/task-time-recognition.md)和[ADR-034](ADR/034-offline-task-time-recognition.md)记录了单代理技术自审、问题修订及准入；没有冒称独立评审。

已实现：离线C++解析、主/小窗共用建议与手选/忽略、日期/时刻/截止/相对提醒、UTC及时区校验、原子任务创建/提醒、偏好持久化。手工选择优先，原文保留；日期默认不提醒，明确时刻才提醒，不猜结束时长；歧义/过期保留草稿要求选择/忽略，不静默保存成无提醒任务。现有任务编辑/导入/服务端不解析标题。schema10及同步HTTP JSON v1不变，无云/新RPC/第三方依赖。本地设置→通用→快速添加可关闭识别或配置纯日期提醒。

验证记录：Qt6.10.2/MinGW13.1 Release全目标构建成功。首次完整CTest **41/42 PASS，407.83秒**；失败为原有update_windows_flow，签名升级子链输出已通过、首次CTest未输出细分Qt Test失败位置。随后两个窗口子用例单独通过；审查发现枚举窗口可能早于ShowWindow，补同一窗口可见条件等待（不重开、不增预算），并显式输出Qt Test文本。最终受影响 **4/4 PASS，46.79秒**（解析/完整任务服务/完整工作区/更新事务），最终QML链 **1/1 PASS，3.13秒**；补充不支持日期词的保守边界和真实键盘事件后，解析/任务服务/QML **3/3 PASS，5.30秒**，42项各有通过记录；没有把首轮失败删去或冒称最终重新跑过一遍全量。见[脱敏证据](evidence/task-time-recognition-windows.json)。

解析测试覆盖中文/ISO日期、周/跨年/闰日、不支持日期词不误用尾部时刻、时刻/半点、相对时间、截止、禁提醒、误判反例、非法/多时间/范围/过去、DST gap/fold和输入/耗时上限。真实QTemporaryDir SQLite验证原子回滚、到期认领/去重；工作区验证主小窗日期一致、分类保留、非法override拒绝及忽略优先、本地设置重载。QML真实组件链验证建议、忽略与成功后恢复、手工修改、Tab/Enter/Esc事件、QInputMethodEvent组合输入防提交、失败保留/成功重置、小窗及200%文字。额外自审修正提交期间时间过期的静默降级风险，并修正建议弹层被布局管理的四项lint提示；最终daymark_qmllint通过且无警告。

真实Windows原生TIME01探针PASS，浅色、深色200%、时间选择弹层、小窗四份截图已检查；小窗选明天时显示“按所选日期安排”，避免与默认今天提示冲突。证据位于忽略的build/time01-*文件，截图散列在脱敏报告中。Windows最小尺寸约束产生setGeometry提示，窗口继续正确显示，未将其当QML错误或物理DPI验收。物理输入法候选/读屏/多屏和macOS/Linux仍UNVERIFIED；未新增后台唤醒承诺。

本轮保持单代理、合成临时资料，无真实用户库/账号操作；已实现代码/文档，本地发行版本仍2.0.2，未自动推送/打tag/发包。本轮早期构建中的QML语法、偏好类型校验遗漏和探针main宏名称冲突均已修复，原始build日志保留；完整回归仅运行一次，后续只复验受影响项。

## UPD-07 首次启动误报占用（本地修复完成，未发布）

用户反馈他机首次解压启动出现原生`update.instance_busy`。已查到FileLock把CreateFile/LockFileEx全部失败统一误报占用，丢失Windows错误；首次初始化与正常启动门也没有短暂竞争等待。现场根因尚未确认，不能宣称已有实例或要求删数据。沿UF-04/05/12、CT-04/05、HIG清晰反馈/可恢复（UX-09/11/13）修正分类、原生弹窗与本地脱敏日志，启动门等待有单调时钟预算，保留apply互斥和写门。

已实现：共享锁分类/有界获取逻辑供Qt与原生入口复用；正常launch门最多10秒，只等待真实字节锁冲突；apply/runtime安全互斥不放宽。原生中文弹窗按权限/共享占用/启动竞争给恢复建议，显示实际Win32码，并写安装目录内脱敏日志；不再一律建议重装。

真实证据：同一只读launch文件夹具，修复前退出30/`update.instance_busy`，修复后退出31/`update.lock_access_denied`/Win32 5；文件权限测试均恢复。Qt6.10.2 Release应用/宿主链接成功。相关CTest **4/4 PASS，20.57秒**：真实错误弹窗/关闭/日志、全新签名目录未经CLI initialize的首次根启动、原生签名升级/持久提交/清理、Qt控制器与SDK/锁/密码学。原生MSVC19.52独立干净重建和2/2测试PASS（0.81秒），不冒充VS2022或他机验证。证据在忽略的`build/upd07-final-tests.log`、`build/upd07-msvc-clean.log`及只读对照结果。使用隔离合成资料，不动用户库；公开GitHub脚本已去除提前initialize，但本轮没有再次运行公网验收。

过程失败保留：首次新增冷启动夹具等待继承stdout的EOF而超时，改为等待launcher本身退出；原生对话框控件ID与预期不同，改为枚举标准对话框类/子控件文字并用WM_CLOSE关闭；首次本地MSVC增量对象仍引用旧签名，干净重建后通过。原始日志保留，未放宽生产超时或跳过断言。沙箱未能可靠观察桌面窗口，最终原生测试在真实桌面权限下通过。

**发布版仍为2.0.2，本轮修复未发布**。现场根因还需对方机器的具体Win32码/日志，不能把只读夹具结论套用到他机；物理DPI/读屏、本轮完整40项及公网冷启动未重验。若要分发修复须制作新版本完整引导包；首期更新不替换安装根的旧launcher，不能仅靠payload自动更新修复旧入口。

## 已完成范围

用户授权完整实现Windows功能、启用GitHub并验证实际更新，选择项目自签名，允许官方地址通过受保护CI Secret编译。当前功能收尾CLOSE-01、发行启用UPD-06均完成；Linux/macOS按用户指示跳过。未覆盖用户数据库、不上传真实任务/隐私日志、不移动旧tag/资产，保持单代理。

| 功能 | 当前结果 | 证据与边界 |
|---|---|---|
| 本地任务/分类/标签/日历/重复/提醒/备份 | IMPLEMENTED / WINDOWS-PASS | 当前正式Windows CTest 48/48包含仓库、事务、查询、时间、工作区和导入导出 |
| 主窗/小窗/Mini/球及分类快查 | IMPLEMENTED / WINDOWS-PASS | 分类默认全部未完成/可选今天；快速添加/排序/菜单/穿透恢复及统一呈现已实现，相关真实QML/原生回归已通过 |
| 账号/同步/完成面板筛选 | IMPLEMENTED / WINDOWS-PASS | 官方/自定义URL、8字符密码、本地优先、资料隔离、冲突及分类/标签交集；真实Qt→Go TLS同步CI成功 |
| 全局日志与诊断 | IMPLEMENTED / WINDOWS-PASS | 本地有界脱敏日志、请求ID、Sxxx-Cxxx、Qt/TLS底层证据、复制详情/日志入口；不上传 |
| Windows在线更新 | PUBLISHED / PUBLIC-GITHUB-PASS | 原生SDK/CLI/Win32薄UI、自动prepare、保存退出、受限启动/COMMIT/RUNNING/清理；真实2.0.2→2.1.1通过 |
| Windows完整安装器 | PUBLISHED / PUBLIC-INSTALLER-PASS | 原生向导、当前用户安装/修复/卸载，固定指纹与实际持久Qt测试日志，资料保留通过 |
| 已实施性能优化 | IMPLEMENTED | 已交付索引/快照/增量模型/提醒判断等，现行测试通过；整应用/GPU/长期SLA未宣称已验收 |

## UPD-06 完成队列

| 事项 | 状态 |
|---|---|
| G0 仓库/权限与不可变源码核对 | DONE：明确downdone账号，API确认admin；旧tag未改，源码身份分开记录 |
| G1 自有签名/受保护环境/独立部署 | DONE：P-256公钥内置；四程序自签名与固定指纹验证；独立Windows实际Qt/CRT来源验证；不导入Root/TrustedPublisher |
| G2 两版正式资源发布 | DONE：2.0.1与2.0.2各有ZIP、ZIP.sha256、SHA256SUMS、更新JSON及签名五项公开资源 |
| G3 真实GitHub升级 | DONE：独立Windows从匿名公开资源升级，生产Qt保存重启、持久提交/RUNNING、删旧程序与保留任务全部通过 |

## 发行身份与资源

| 版本 | 精确生产源码 | 构建/最终发布 |
|---|---|---|
| [2.1.1](https://github.com/downdone/daymark/releases/tag/v2.1.1) | `76a74df801d334fa7c5db3cdaa87d9d81b438f5f` | [候选42389695899](https://github.com/downdone/daymark/actions/runs/42389695899)；[签名/独立部署/安装发布42392523180](https://github.com/downdone/daymark/actions/runs/42392523180) |
| [2.0.1](https://github.com/downdone/daymark/releases/tag/v2.0.1) | `9e5ead18f7e4d2a990f0eccbe8096dec48dfd0e1` | 固定源码[重建42160035601](https://github.com/downdone/daymark/actions/runs/42160035601)；[签名/独立部署/发布42161946172](https://github.com/downdone/daymark/actions/runs/42161946172) |
| [2.0.2](https://github.com/downdone/daymark/releases/tag/v2.0.2) | `0bdaca92b5a1158df035c830dd926307464125d6` | [标签构建42161583632](https://github.com/downdone/daymark/actions/runs/42161583632)；[签名/独立部署/发布42164523268](https://github.com/downdone/daymark/actions/runs/42164523268) |

历史2.0.2[Windows x64便携包](https://github.com/downdone/daymark/releases/download/v2.0.2/Daymark-2.0.2-windows-x64.zip)：44,041,439字节，assetId `715946759`，SHA256 `0b94cb2245b4070e164336570e05300aa269f5b45b866959100f9388aa06798b`。解压完整目录，运行根目录daymark.exe；versions内的payload不单独移动。无更新器的旧平铺包需先手动安装一次引导布局。

首次tag候选的打包失败未上传资产；恢复使用同一2.0.1源码、独立记录的已审查main工具SHA和run身份，重新完整测试，没有移动tag或把工具SHA冒充源码。2.0.1内部README保留原候选文案的问题已在2.0.2最终说明中更正，旧包没有覆盖。

## 真实验证

- **2.0.2完整Windows CTest 40/40，0失败/0跳过，389.75秒**；[桌面CI](https://github.com/downdone/daymark/actions/runs/42161582762)和[服务/真实Qt同步CI](https://github.com/downdone/daymark/actions/runs/42161582820)成功。本地2.0.2 Release链接完成，显式隔离版本探针退出0；更新组件4/4与公共验收包装器的原本地夹具回归通过。
- **实际公开GitHub 2.0.1→2.0.2 PASS**：[Windows验收42164528281](https://github.com/downdone/daymark/actions/runs/42164528281)、[脱敏结构化报告](evidence/windows-update-2.0.1-to-2.0.2.json)。源控证书pin、生产CLI内置公钥、匿名API/真实ZIP/真实Qt均使用正式资源；无fixture/API/信任覆盖。全部SQLite位于QTemporaryDir，未登录账号或上传任务。
- 实际Qt升级阶段**22.44秒**；完整验收94.439秒（包含60.631秒等待新版资源公开、1.23秒引导包下载及验证）。旧/新各29个Qt/SQL/CRT模块均观察到来自对应签名payload，子进程清除SDK/Qt搜索路径和CI凭据；新进程COMMIT/RUNNING与清理完成，旧exe删除，未知/修改文件保留，合成任务/SQLite完整性保留。
- 自签名验证：四个实际PE签名通过固定SHA256证书pin与独立CustomRootTrust校验；错pin、未签名、改PE、改清单/载荷、错key和错签名域均拒绝。改PE得到`TRUST_E_BAD_DIGEST 0x80096010`；系统/用户Root库不变。`signed`旧公共签名字段仍false，`authenticode.policy=self-signed-pinned/publicTrust=false`明确，不伪造Windows公共信任。
- 工程修复：MSVC显式标准头、独立UTF8、原生manifest去重；CRT十成员（含vcruntime140_threads.dll）逐个微软签名/版本/x64/散列校验；官方URL输入保留至所有CMake构建/测试/打包结束再清理，实际EXE必须包含配置；签名/部署报告与源SHA、工具SHA、触发run、全部文件散列绑定。原始失败均保留于历史与忽略的build证据。
- 收尾验证：发布元数据/隐私守卫15项通过，覆盖强制跟踪本地配置/密钥及大小写变体；198份Markdown相对链接和12项固定依赖来源散列通过。历史文件保留原字节前缀，再追加本轮原始过程。

## 密钥与GitHub配置

受保护Environment `windows-update-release`限制main/v*；更新私钥、代码PFX/密码、官方URL均为加密Secrets，公钥配置与托管开关已启用。源码仅含公钥、证书指纹与URL示例。自有私钥从临时build目录移入忽略的`config/signing.local`，owner/SYSTEM-only ACL和CurrentUser DPAPI验证通过，密钥身份未变，无明文私钥遗留；签名证书仅CurrentUser/My，不在信任根库。DPAPI绑定用户/机器，不能把普通文件复制当跨机恢复。

清单/索引签名认证更新来源；项目自签名EXE**不是Windows公共发布者证书**，仍可能有SmartScreen/未知发布者提示。见[自签名契约](design/update/self-signing.md)、[受控重建与GitHub配置](design/update/github-prerequisites.md)。

## 明确保留的外部边界

| 项目 | 真实状态 |
|---|---|
| 本机GitHub CDN | 首次匿名下载发生90秒读超时，未启动Qt。一次64KB有界范围读取HTTP206/7.207秒；未改TLS/代理、未盲目重跑。本次公网全链PASS来自独立Windows，不能冒充本机CDN恢复 |
| 另一台机器注册超时 | 诊断功能已实现；现场根因未确认，仍需该机新版详情/日志 |
| 物理DPI/多屏/读屏/睡眠/8h及Windows10最低build | 本轮没有相应实机验收；托管windows-2022和当前Windows局部验证不能替代，不阻塞已获授权的Windows功能/CI范围发布 |
| Linux/macOS | 按用户指示跳过，不实施、不构建、不宣称通过 |

[CLOSE-01与UPD-06完整原始历史](PROGRESS-HISTORY.md) · [文档导航](README.md) · [更新实现](design/update/implementation.md) · [诊断契约](design/runtime-diagnostics.md)。本地开发入口仍为build/Launch-Daymark.cmd，依赖开发Kit；可转发发行包请使用上述正式完整ZIP，不复制build/bin单个EXE。


MEM-04最终评审补记（2026-11-07）：CurveRenderer实验被原生截图评审拒绝，发行保留Geometry/MSAA。新增恢复后设置/最近删除/日历入口的声明ink与实际像素断言；该断言先捕获Geometry旧色，随后Glyph仅在颜色/路径改变时重建Shape后真实Windows通过，两组6条转换无尺寸事件/QML警告，截图已检查。不是通过提高阈值或忽略失败放行。最终选定版本原生34阶段PASS，启动可达17,157对象（之前30,029），提交254.62MiB/私有工作集202.00MiB；十次设置后312.95MiB，全部隐藏204.01MiB。未采用实验低值冒充发行数据，尚无逐字节堆/GPU归因或跨机器内存保证；完整脱敏数据见object-management-after.json。最终选定代码完整CTest **46/46 PASS，434.46s**，QML lint无警告；6项类型化复验PASS（165.62s），最终原生恢复图标/动画像素PASS。单代理源码/所有权/隐私/CI门禁自审通过，49项明确文件已暂存且源码隐私检查PASS；进入提交/推送/tag及正式MSVC流水线验收。


2026-11-07 GitHub门禁：b79329b及不可变v2.1.0已推送；Source Release和Account/sync成功，但Build/source-policy的generate-diagnostics --check发现此前安装能力新增16个setup key后未同步src/sync/Diagnostics.h。主程序当时仍在编译，无程序资产/ready发布。已取消两个过期编译，将无程序资产的2.1.0 Release转可恢复草稿，保留原标签，不覆盖资产或移动标签。修复为重新生成唯一来源映射，并增加diagnostic_registry/generated_sync_contract两个CTest、desktop等待source-policy、tag编译前同样检查。正式版本顺延2.1.1；不是绕过门禁或把旧源码标为通过。2.1.1最终本地构建及QML lint PASS（无警告），完整CTest **48/48 PASS，453.41s**；完全按GitHub source-policy命令复现的27项Python保护测试、源码隐私/版本、两个生成契约检查全部PASS。准备推送修复，先核对GitHub快速源码门禁，再创建v2.1.1，不移动v2.1.0。新的正式MSVC/安装/公网更新仍待验证。

2026-11-08发布工具修复：正式MSVC候选42389695899与main桌面/同步均成功；首次受保护发布42391537466签名通过，独立部署发现audit-native-imports.py固定nargs=3，拒绝新增安装helper的第四个参数，发布跳过且未上传程序资产。改为逐个审计所有提供的host，至少一个参数，CLI回归验证旧三项/新四项、第四项外部DLL拒绝及空参数拒绝；3项Python审计测试和实际本地四PE审计PASS，发布保护10项PASS。仅修main工具，保持v2.1.1源SHA/候选/标签不变，重新执行完整签名与独立部署，不重编已通过的产品。

2026-11-08获取工具修复：42391984474五程序签名与独立真实Qt/CRT部署均PASS，发布阶段固定NSIS ZIP散列拒绝了下载响应，未上传资产。本机复现Invoke-WebRequest取得141862字节HTML而非ZIP；原2,363,925字节官方ZIP散列仍与ADR一致。修正为官方文件直链+Windows原生curl、HTTPS及有界超时，新增ZIP头检查，保留原双散列。

2026-11-08正式发行：42392523180签名/独立部署/安装发布PASS，7项资产已附加v2.1.1，所有GitHub digest复核通过。42393012685真实匿名2.0.2→2.1.1更新PASS。安装宿主退出成功但默认Qt Test stdout为空，补充显式`-o -,txt`并在未来发布前检查实际完成标记；新增只读公共安装验收工作流，从同一精确tag build取得宿主、匿名下载现有签名Setup，在QTemporaryDir重复验证并保存完整输出，不替换任何已发布资产。Python发布保护10项、Python/YAML/全部新PowerShell块语法PASS；公共安装日志证据待运行。

安装证据补充：首次公开补验42393583562返回成功，但Qt在Windows重定向stdout仍为空；PowerShell命令空结果的-notmatch未能拒绝该空输出，不能把该日志门禁视为通过。改由Qt Test自身写独立文本文件，ReadAllText强制真实文件/字符串，再验证实际NSIS完成标记及3 passed/0 failed/0 skipped；发布器同样核对该文件。缺artifact的本地故障探针退出1并可靠写出Qt日志，证明文件日志路径有效。补验仍只针对不可变安装包，不重跑已通过的公开升级或产品全套。

2026-11-08 MEM-04收尾：Windows2.1.1正式发行及最终非空安装文件日志复核完成，事实汇总于本页首部；后续工具修复均仅修改main发行/验收逻辑，不改原tag/程序散列。macOS/Linux跳过，物理/长期、另一机器注册现场与全堆/GPU归因边界保留。

CHROME-01过程：首轮构建发现quint32→QJsonValue重载歧义，显式qint64后编译通过。偏好/readonly与写失败回滚独立测试均PASS。原生测试25秒阻塞，通过持久Qt日志定位到WM_NCLBUTTONDOWN：Qt全局过滤不足以覆盖同步非客户区消息，改为每个HWND的SetWindowSubclass/DefSubclassProc，Surface销毁及QObject销毁均解除；不放宽超时。一次隐藏启动原生probe未exposed明确排除；一次在链接未结束时尝试启动被拒绝，未计运行成功。后续只在编译完成后执行。

CHROME-01原生切片：改为HWND subclass后window_chrome CTest PASS（1.30s），内部6 passed/0 failed/0 skipped；实际同步最大化消息、八轮切换物理外框和句柄、普通/最大化/隐藏状态及写门/部分失败/持久化回滚均通过。未把此前25秒阻塞或hidden启动未exposed计为成功。开始2.2.0构建及实际QML用户链；其他平台/离屏只走原生默认且不调用Win32接口。

CHROME-01用户追加融合设计：第一版原生UI启动发现AbstractButton.action为FINAL，已改captionAction；实际用户链随后通过入口/三窗切换/任务和账号草稿/浅深200%/账号键盘关闭，到第11阶段发现最大化切换在下一事件轮状态不保持，尚未通过。用户要求顶区与界面融为一体，v2设计复用现有字标/页面工具、设置分类头及账号品牌，不增加独立横条；补充交互命中排除与正文可用区域契约。继续保留并定位最大化用例，不发布未通过产物。

CHROME-01首轮完整回归：Qt6.10.2 Release构建/QML lint通过（无lint警告），50项中47通过、3失败，340.98s，未发布。失败为desktop_surfaces终态帧/空闲回球、task_list_interaction的旧正文/场景坐标混用，以及qml_watchdog_late在短备注焦点前置场景中提前失败。已补充实际帧读取、交互/计时器诊断，箭头断言改为场景坐标；新顶区减少正文高度后暴露短备注被上次滚动裁切，按完整短字段定位修复。字体环境已按标准Windows测试入口补齐，但不能将其当短备注根因或把3项失败计为通过；继续针对性验证。原生新增IsZoomed与最小化/还原矩形断言、Enter键用户链均已通过针对性测试，尚待最终完整回归。

CHROME-01最终构建预检：Qt6.10.2 Release再次构建与QML lint无警告；强制Windows QPA的window_chrome/title_bar_ui两项PASS（8.02s），包括真实HWND IsZoomed、物理外框、最小化/还原位置、隐藏不唤起、三窗切换/草稿、日历场景→正文箭头、Tab顺序与Enter/Space。空闲回球验收改为呈现帧与空闲阶段分别计时，保留各自1500/1900ms预算与生产帧确认；非盲重试，不改变生产自动返回时序。最终50项完整回归进行中，先推送已通过切片启动Windows CI，完整门禁成功前不创建发布标签。

CHROME-01最终本地门禁：完整CTest **50/50 PASS，451.61s**；QML lint无警告，强制Windows QPA的原生用户链/Tab/Enter/Space及最终浅色主窗/设置、深色200%窄账号和恢复截图均PASS（截图仅客户区）。[本地证据](evidence/windows-title-bar-2.2.0.json)记录范围、摘要和图像散列，正式MSVC资产另验。源码`21a945bf8ace6709ce3b53abdbb792712e441a29`已推送main；不可变注释标签v2.2.0对象`7dfc6ef36119d0088c2ea11bf48db3b9da1d25ba`已推送，源码发布42451106147成功，正式候选42451106148进行中。main源码门禁成功，同步42450609779成功，桌面42450609807仍在运行；尚无2.2.0程序资产，不宣称正式发行完成。

2.2.0候选最终FAIL：main Windows42450609807完整通过，但标签候选42451106148编译PASS后48/50，qml_startup出现卸载上下文的CalendarMonthStream/CalendarPicker/ScrollPhysics访问；desktop_surfaces回球断言时实际returnRemainingMs仍25，hover/press/menu/keyboard/moving/visual均false。未生成或上传程序资产。保留原v2.2.0对象和失败记录，修复进入2.2.1，不移动标签：卸载前显式停投影/布局/回收Timer及清空ListView模型取消incubators，探针只执行有效QQmlContext的deferred控件；新增12轮快速/绘制后卸载。空闲验收预算2600ms=1000空闲+1500呈现等待+100调度，不改变生产1000ms逻辑；不盲重跑旧候选。

2.2.1生命周期补充：新12轮用例首次复验捕获WeekRow在延迟销毁期间读取已先销毁的CalendarMetrics.headingSize；不能仅清空列表就宣称完成。改为值类型rowMetrics，卸载前冻结必要尺寸并断开已销毁QObject依赖，正常可见时仍绑定实时尺寸；未加常驻日历、未屏蔽警告。最终calendar_lifecycle与完整qml_startup两项PASS（44.60s），QML lint无警告；51项完整回归已开始，源代码冻结准备推送。

2.2.1最终本地完整CTest **51/51 PASS，455.67s**，QML lint无警告；[证据](evidence/windows-title-bar-2.2.1.json)。不可变v2.2.1已推送，对象`6c7071b93b10426666edc1f703ab4710411048e5`，生产源码`d2c75ea7fc38374506ab5cabd97acb4eb5349e1e`。开始正式MSVC候选，不将本地通过替代签名/安装/公网升级门禁。

2.2.1远端main42469313594在MSVC50/51失败：新增calendar_lifecycle捕获ScrollPhysics目标已销毁仍求contentHeight，未签名或发布。完整复审不再逐个字段补空值：滚动策略detach先禁用绑定并清空目标，卸载前遍历视图解除滚动策略；两套纯主题日历尺寸由引擎级CalendarMetricSet共享，寿命覆盖可卸载页面，字号/间距实时跟随Theme。生命周期扩至16种（月/周/日/年×100/200%×立即/绘制后关闭），保持所有QML警告与旧页弱引用门禁。修复进入2.2.2，先通过main远端再打标签，不重跑旧失败候选。

2.2.2本地复审：共享CalendarMetricSet实验引入CalendarMonthGrid绑定回归，未提交，已撤回；保留原有CalendarMetrics所有权和值快照，聚焦显式detach滚动绑定。全量探针也限定为当前可见且真正挂接window.contentItem的控件，纯C++容器子树仍遍历，16种独立生命周期场景保留，不屏蔽任何QML警告。

2.2.2收窄修复复验：原有尺寸所有权恢复，显式滚动detach与可见场景的deferred遍历均保留；16种生命周期、完整startup和桌面组件3项全部PASS（100.33s），无QML警告，最终51项进行中。接下来先推送main并通过实际MSVC门禁再打标签，避免仅以本地结果继续产生候选。
