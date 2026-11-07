# Daymark 当前进度

更新：2026-11-07。**2.0.2 已发布 / schema10 / HTTP JSON v1 / Qt6.10.2**。当前状态只维护在本页；原始过程、失败及修复保留于[历史记录](PROGRESS-HISTORY.md)。

## MEM-04 优化实现与2.1.0发行（进行中，用户授权发布）

目标：完成对象生命周期方案评审、逐片实现与真实内存对照，相关回归/安装/更新通过后提交并推送GitHub，发布新版本。单代理使用autonomous-project-loop。发布版本2.1.0（已核对GitHub最新仍为2.0.2），保持既有不可变tag/签名/独立部署门禁；不将本地MinGW包作为线上MSVC更新包。

OBJ-01已实施：DateField/TimeField的fallback内容仅打开时创建、onClosed卸载，inline路径不创建fallback。新增picker_lifecycle隔离验收覆盖选择/取消/重开/inline/200%缩放，连同time_input_ui **2/2 PASS，9.75s**。首次Loader误设只读implicitHeight导致加载失败，已移除并以最终构建复验；不把失败计为通过。

OBJ-02/03已实施：主列表共用TaskContextMenu，捕获身份/版本并在过期时关闭；设置仅创建可见分类，保留分类与滚动，日历保留轻根与窗口状态、关闭后卸载内容。隐藏工具窗允许释放场景图。修复新生命周期暴露的延迟回调、ListView异步cache孵化、动态布局焦点揭示，以及测试无条件访问隐藏内容；真实图形层级临时隐藏不卸载日历。picker/qml启动2/2 PASS（55.00s）、contextual_workspace PASS（16.81s）、desktop_surfaces PASS（52.89s），最终cache修正后qml_startup PASS（39.15s）。一次在旧测试占用exe时链接失败，待该进程自然完成后链接通过，未强杀用户实例。

OBJ-04已实施：关闭导入页排队discard、取消预览代次拒绝晚到结果、切资料worker重载清除计划并绑定代次；增加取消零写、切资料失效和重新预览后成功导入的真实临时SQLite测试。隐藏日历不做独立/未安排投影。task_workspace/continuous_calendar 2项PASS；日历测试发现旧夹具读取已卸载设置页导致崩溃，改为实际打开对应分类后验证，最终task_workspace/calendar_interaction **2/2 PASS，36.51s**。

当前OBJ-05：主窗隐藏资源与组件彻底隐藏资源释放、图标渲染A/B。初次OBJ03原生试验主窗visible但未exposed/未初始化场景图，已排除其启动内存比较；后续采样前强制校验主窗与组件实际呈现。有效原生A/B：DPR1.5/D3D11/20任务双窗已exposed且图形初始化，A(逐图标layer)启动提交246.16MiB、B(CurveRenderer)230.61MiB；B十次设置重开后247.58MiB，全部隐藏115.04MiB。以上是同机单轮阶段样本，不是统计SLA。原生动画曾出现终态等待帧确认，已加入同步/交换/提交计数和exposure前置检查；诊断版本在独立、无并行编译的运行中通过两组6条转换，未改变生产fence语义或放宽超时，早期失败不掩盖。最终版本原生复验仍待执行。


OBJ-06进行中：完整本地构建PASS，第一轮CTest **46/46 PASS，445.35s**；随后将Loader接口改为具名inline component和显式类型转换，消除新引入的QML lint警告，正在复验受影响UI。发布元数据/保护测试25项PASS，历史构建来源8项PASS。正式CI已接入第五个setup helper、安装验收宿主、固定散列NSIS和安装/修复/卸载门禁，再上传ready文件；公网验收改为明确2.0.2→2.1.0。GitHub远端main未超前、本地已有6个未推送功能提交，保护环境四个Secret和发布开关均已核对存在；尚未推送/tag/发布。上轮一次执行因自动审批额度耗尽未执行，当前授权操作已恢复。

评审：保持单QML引擎和稳定小窗画布；关闭选择器在退出动画完成后卸载内容，inline模式不建fallback；菜单按taskId/version捕获目标；低频窗口保留可恢复入口及会话状态，资源释放只针对已隐藏窗，不破坏动效/菜单/IME/草稿；导入计划须绑定资料生命周期。先选择器、再共享菜单、窗口/隐藏投影、临时数据与图形路径，单切片构建测试/测量，不提前宣布内存达标。评审主体为单代理自审，不冒充独立评审。

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
| 本地任务/分类/标签/日历/重复/提醒/备份 | IMPLEMENTED / WINDOWS-PASS | 当前完整Windows CTest 40/40包含仓库、事务、查询、时间、工作区和导入导出 |
| 主窗/小窗/Mini/球及分类快查 | IMPLEMENTED / WINDOWS-PASS | 分类默认全部未完成/可选今天；快速添加/排序/菜单/穿透恢复及统一呈现已实现，相关真实QML/原生回归已通过 |
| 账号/同步/完成面板筛选 | IMPLEMENTED / WINDOWS-PASS | 官方/自定义URL、8字符密码、本地优先、资料隔离、冲突及分类/标签交集；真实Qt→Go TLS同步CI成功 |
| 全局日志与诊断 | IMPLEMENTED / WINDOWS-PASS | 本地有界脱敏日志、请求ID、Sxxx-Cxxx、Qt/TLS底层证据、复制详情/日志入口；不上传 |
| Windows在线更新 | PUBLISHED / PUBLIC-GITHUB-PASS | 原生SDK/CLI/Win32薄UI、自动prepare、保存退出、受限启动/COMMIT/RUNNING/清理；真实2.0.1→2.0.2通过 |
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
| [2.0.1](https://github.com/downdone/daymark/releases/tag/v2.0.1) | `9e5ead18f7e4d2a990f0eccbe8096dec48dfd0e1` | 固定源码[重建42160035601](https://github.com/downdone/daymark/actions/runs/42160035601)；[签名/独立部署/发布42161946172](https://github.com/downdone/daymark/actions/runs/42161946172) |
| [2.0.2](https://github.com/downdone/daymark/releases/tag/v2.0.2) | `0bdaca92b5a1158df035c830dd926307464125d6` | [标签构建42161583632](https://github.com/downdone/daymark/actions/runs/42161583632)；[签名/独立部署/发布42164523268](https://github.com/downdone/daymark/actions/runs/42164523268) |

最新[Windows x64便携包](https://github.com/downdone/daymark/releases/download/v2.0.2/Daymark-2.0.2-windows-x64.zip)：44,041,439字节，assetId `715946759`，SHA256 `0b94cb2245b4070e164336570e05300aa269f5b45b866959100f9388aa06798b`。解压完整目录，运行根目录daymark.exe；versions内的payload不单独移动。无更新器的旧平铺包需先手动安装一次引导布局。

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
