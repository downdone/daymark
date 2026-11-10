# UI-81 / ACCOUNT-01 界面焕新技术设计

2026-11-10。遵循[新增能力标准](new-capability-standard.md)，视觉依据见[全局风格契约§8.11](daymark-interface-refresh.md#811-现有实现上的全局风格统一)及详情§8.10。用户已授权将设计应用到现有版本：基于现有实现统一风格，不做大的交互改动，不新增业务功能。实现状态只见[PROGRESS](../PROGRESS.md)。默认详情继续现有就近小弹窗，无停靠/大面板。

## 1 功能定义与实例

本轮目标是让已有能力容易看懂、容易找到、操作位置稳定。主窗、桌面小窗、Mini、球共享视觉角色；账号继续独立入口。详情沿用现有「标题/备注 → 时间、提醒、时段、重复 → 分类/标签/关注/置顶 → 父子任务」，不新增一套开始/截止/提醒语义。

| 用户链 | 结果和不变量 |
|---|---|
| 选任务→信息按钮→读详情 | 时间范围、指定提醒、时段和循环摘要无需展开多组才能辨认；未设值是原有添加入口。 |
| 时间→开始/结束日期 | 复用DateField/TaskInspectorPicker；点击日期即写入同一草稿并回原字段，未选择直接返回不改字段；最终落库仍经过原保存门。 |
| 提醒→指定日期/时间 | 复用ReminderField；重复提示同一任务仍与生成后续任务分开。 |
| 时段→选择/调整 | 复用TimeRangePicker和TaskTimingFields；跨日、时区、提前提醒沿原逻辑，不以页面重画重置字段。 |
| 切详情宿主或主题 | 同一任务、同一草稿与版本；不保存、不查库、不重建编辑内容。 |
| 切任务/范围/资料或关闭 | 走现有navigateSafely与saveEditor；合法提交后离开，失败留在原处。 |
| 从小窗进入完整详情 | 走现有主窗任务入口，保存当前小窗草稿；不创建第二份独立编辑器。 |

非目标：不复制Apple字体/图标，不新增地点/联系人/附件、不新增提醒类型、不重新设计任务模型。本轮不新增账号恢复服务、邮件系统或数据库表；此前账号完整能力需求保留为后续独立切片。

## 2 现状与技术选型

Qt 6.10.2 / C++20 / Qt Quick继续使用；Qt内置控件、既有DS样式和原创AppIcon足够，不引入UI库、WebView或RPC。服务端继续Go单体/SQLite/HTTP JSON v1。Windows平台菜单、标题栏与窗口边界保持现有已修复链路。

| 已有入口 | 本次使用方式 |
|---|---|
| Main.qml的editor Pane、editorScroll、editorFields | 保留单宿主/单滚动轴与保存门，抽出呈现边界时保持objectName。 |
| TaskEditorSession.qml | 保留任务身份、保存状态、来源与待执行动作，不创建平行业务草稿。 |
| TaskTimingFields / TaskDetailsFields | 继续提供全部时间/组织属性；只接入紧凑token、值列对齐和明确摘要。 |
| DateField / ReminderField / TimeField / TaskInspectorPicker | 继续单层选择、取消、应用与焦点返回；不以浏览器原生字段替代正式选择器。 |
| TimeRangePicker / TagMenu / DS.Menu | 保留时段直接操作、标签多选及原生命令菜单；按控件职责计算自然尺寸。 |
| DesignTokens / Theme / AppIcon | 统一字体、角色圆角、颜色、间距、可见图形与热区。 |
| TaskWorkspace / TaskService / Repository | 增量模型、校验、CAS和原子落库原路径不变。 |

原型的HTML日期输入、单个样例标签和内存修改只用于说明排版关系，不是生产替换方案。

## 3 架构与数据流

```text
主窗导航/列表/日历 ─→ Main.navigateSafely ─→ 同一个TaskEditorSession
                                               │
                         Main.editor Pane（原有就近弹窗）
                                               │
                    原标题/备注 + TaskTimingFields + TaskDetailsFields
                                               │
                 原saveEditor → TaskWorkspace异步命令 → TaskService → Repository
                                               │
                               operationFinished → 更新版本/反馈/继续待执行动作

WidgetViewModel → 原三形态事务/独立桌面范围 → 原主窗任务详情入口
账号窗口 → 原TaskWorkspace同步意图 → Coordinator → 原HTTPS v1 → Go/SQLite
```

QML在GUI线程做布局和意图转发；业务判定、SQLite写入继续在worker。时间摘要优先复用CalendarService/TaskWorkspace已有输出；不在QML另写Today或日历投影算法。主题和布局不触发模型全量重查。

## 4 模块与生命周期

| 模块/变更点 | 输入、输出和所有权 |
|---|---|
| 现有Main导航/顶区/正文 | 直接接共享视觉token，保持原入口、顺序和命令；本轮不以风格统一为由要求先提取WorkspaceShell。标题栏和正文不重复扣除安全区。 |
| Main.qml的editor Pane | 保留原sourcePoint、左右避让、边界夹紧、手动移动和显式展开；默认小弹窗，不占正文布局槽。无需新增停靠宿主。 |
| 原editorFields/editorHeader | 在原控件上统一文字轴和自然高度。保留标题/备注/日期/提醒等原objectName和命令；不要求先抽新组件才能精修。 |
| TaskTimingFields | 紧凑时间/提醒/时段/重复行、原选择器和摘要。取消自定义重复仍恢复repeatSnapshot。 |
| TaskDetailsFields | 原分类/分组、TagMenu、关注/置顶及父任务选择。分类变化引起的脱离父任务提示仍保留。 |
| TaskInspectorPicker | 持有owner字段和kind；关闭释放引用并回owner。禁止关闭后回调写入已换任务。 |
| Theme/DesignTokens | 分角色token，覆盖主窗、桌面、账号与选择器。回退主题/增强对比继续完整可用。 |
| 现有AccountWindow / SettingsDialog | 原登录/注册/同步/资料/退出及设置四分类，仅接视觉；不新增账号管理分区、API接线或偏好。 |

沿原editor会话和选择器所有权，不为不同窗宽加载第二份编辑器。改变窗口尺寸仅重算原弹窗位置/可用高度，不运行fillEditor覆盖脏值。现有显式展开入口保留，只有用户主动操作才扩大；窄窗不自动进入大面板。实际控件销毁只在会话安全结束后发生。

## 5 协议与兼容

视觉与详情切片不涉及新IPC或网络协议，继续原QML信号/方法和TaskWorkspace命令。正式JSON DTO、服务器端点、同步游标与outbox不因视觉变化修改。

当前账号流程沿[v1唯一机器契约](../api/sync-v1.openapi.json)，不增加请求。设备/改密/导出/注销等后续接线需求不并入本轮风格统一；旧服务能力回退仍是其实施门禁。恢复邮箱/找回也保留为独立需求，本轮不发布新端点或把未来客户端入口画入现行设计。

请求绑定服务origin、server instance、资料ID、账号身份与代次。切资料、退出或关闭敏感操作后，迟到结果只释放资源，不覆盖当前账号界面。网络失败沿Sxxx-Cxxx/请求关联和脱敏日志，不新增第二种错误体系。

## 6 类型、接口与结构契约

### 6.1 展示状态

下列是呈现契约，不落业务库。实现可用QML readonly property/现有Qt DTO，不要求预建通用框架。

```text
InspectorHostMode = CompactPopup | ExplicitExpanded   // 原有模式，无自动停靠
InspectorLayoutInput = { availableWidthDIP, availableHeightDIP, textScale,
                         sidebarWidthDIP, pointerKind }
InspectorLayoutResult = { mode, widthDIP, maxHeightDIP, labelWidthDIP, scrollRequired }
InspectorPresentation = { taskId, sourceObjectName, focusObjectName,
                          contentY, openPickerKind }   // 复用会话来源/焦点状态
```

沿现有420 DIP宽度上限和可用工作区夹紧规则，先优化内部排版/留白，再实测是否需要收宽；原型360仅为关系演示，不替换生产常量。标签列36～44起测；Qt原生截图发现四字“结束重复”会断行，生产统一为56 DIP（随文字缩放），值列自然伸缩、父任务值左对齐。默认高度随内容，超出可用工作区才滚动；打开/关闭不挤压正文或改变列表起点。窄窗仍为有边界的小弹窗，按原可用空间夹紧；不自动铺满整栏/整页。OS边框与Win32客户区计算不变。

### 6.2 复用调用约定

| 调用 | 前置、结果、失败 |
|---|---|
| `openEditor(taskId)` / `navigateSafely(action)` | 现有保存门先完成；无任务/已删除按原状态拒绝，保留有效焦点。 |
| `inspectorPicker.openFor(field, kind)` | kind仍为date/time/reminder，字段属于当前会话；打开不写库。 |
| `owner.chooseDate(...)` / ReminderField.applySelection | 将原本地输入写入同一草稿；取消只关闭。生产仍走原字段解析和时区处理。 |
| `timingFields.detailsPatch()` | 保持缺失=不改、null=清除、值=设置；未编辑的schedule/repeat不重建。 |
| `extraFields.detailsPatch()` | 原分类、标签、关系、关注和置顶语义；无变更不生成额外写入。 |
| `saveTaskWithDetails(id, version, title, notes, planning, start, end, focus, patch)` | 原TaskWorkspace接口；返回bool仅是受理，不是提交成功。成功由operationFinished确认；失败保稿、版本冲突不覆盖远端。 |

### 6.3 时间字段映射与防丢失

| UI | 已有事实/转换 |
|---|---|
| 时间、开始/结束日期 | 原startDate/deadline控件及originalStartOn/originalEndOn；继续既有统一日期投影和timeDatesDiffer提示。 |
| 时段 | TaskSchedule：全天QDate区间，定时UTC起止+IANA时区；全天结束日排他，界面显示包含日的转换仍在原边界。 |
| 时段的提前提醒 | `schedule.alertMinutes`；依附时段。移除时段时通过schedule=null一并清除，界面说明影响并可在草稿内撤回。 |
| 提醒 | `remindAtUtc`及原ReminderField本地输入；独立保留，不因清除时段而取消。 |
| 重复提示同一任务 | `reminderRepeat/reminderEvery/reminderZone`；留在原提醒选择器，不能映射为任务循环生成。 |
| 重复生成后续任务 | 既有RecurrenceRule和TaskTimingFields；版本、停止历史保留语义不变。 |

`task()`已有storedStartOn/storedEndOn和投影日期。不得因为UI看到了时段结束日就额外写一份“截止”事实；未改日期时仍使用原original字段。摘要必须由同一份现有草稿派生，不能把已安排的高级值藏成“未设置”。

## 7 数据库与事务

本轮视觉、宿主、已有账号接线**不改schema**：不新增颜色/布局业务列，不复制任务或提醒，不改历史migration。设备偏好继续原QSettings；业务库仍schema10。

原保存事务包含Task字段、schedule、tag/关系和循环变更，并产生既有同步命令；失效CAS/校验失败零写入。独立提醒的投递/认领沿现有服务，不因控件重建再次提醒。后续确需账号恢复时单开服务端migration/备份/兼容评审，不借本轮UI顺带改库。

## 8 HIG/UX与完整状态

依据P-13/14/15/16/18/21、UX-01/02/07/09～13/37/57/79/80；Apple[输入](https://developer.apple.com/design/human-interface-guidelines/entering-data)、[布局](https://developer.apple.com/design/human-interface-guidelines/layout)、[弹层](https://developer.apple.com/design/human-interface-guidelines/popovers)映射为内容优先、同轴分组、就近编辑、自然尺寸与可恢复退出。

- 标题17/22、正文14/20、字段/菜单13/18、辅助12/16作为100%候选；沿正式版Windows微软雅黑UI管线，Mac使用平台字体。不复制SF资源或把截图字形像素当pt。
- 时间、提醒、时段、重复维持原入口；原选择器操作流程不变。正文平面呈现、组间一条细线；字段常态轻、hover/focus可辨。
- Popup按内容自然高度和工作区上限计算。Main当前picker.active触发520高、TaskInspectorPicker的400高、详情240最小值需按实际内容预算替换；避免没内容也留大底。
- TimeRangePicker有真实时间轴；保留其可操作区和直接选择，不把它当短菜单压成几行。选择器、命令菜单、任务详情分别测量。
- 保留native菜单及原键盘行为；Esc先关当前选择器，再退出详情，保存失败不离开；焦点回来源。IME组合Enter不提交。
- hover、selected、focus、disabled、empty、busy、error分开。长备注一个正文滚动轴；200%可换行，滚动槽独占，不能覆盖字段。
- 菜单/选择器保持实体，正文透明偏好不能传递到编辑层；不改桌面渲染事务或使用opacity模拟穿透。

## 9 可靠性、隐私与性能

同一任务最多一个主窗编辑会话；宽窄切换不发DB/网络命令。所有保存仍经过已有异步请求/版本，pendingAction只在成功后执行；局部选择器的取消不能触发整体放弃。

维持现有TaskListModel虚拟化/增量更新；不为每个任务常驻一套详情或日历，不预热账号管理页。目标不是新增缓存；同等合成场景下比较创建/销毁对象和private bytes，未通过基线前不写“已降低内存”。

诊断只记录模块、动作、状态、版本/代次与错误码；不记录标题、备注、邮箱、令牌或真实数据库路径。导出与注销沿原校验/确认/结果未明查询；本地继续可用。

## 10 单元、集成和完整链验收

| 层级 | 用例与证据 |
|---|---|
| 布局/状态单测 | 宽窄预算、100/200%文字、空/长摘要、只改变承载不改变草稿；浮层自然高度、有效分隔和必要滚动。 |
| 时间/业务回归 | 复用task_service/calendar用例；日期only、跨日/时区、无效时段、独立提醒与alertMinutes不互相覆盖，循环取消恢复，原投影日期不被误写。 |
| QML集成 | 复用task_list_interaction/contextual_workspace/calendar_interaction；真实字段选择→取消/应用→保存→重开，失败保稿、CAS、切任务/资料的迟到结果隔离。 |
| 窗口/桌面 | window_chrome/title_bar_layout/desktop_surfaces/desktop_task_menu；黑边、顶区切换稳定、菜单实体、临时展开保持条件与回收。 |
| 账号 | account_access及原Qt↔Go临时资料TLS链；旧服务能力缺失、认证超时、改密/撤销结果未明、导出校验/注销状态查询按对应切片追加。 |
| 性能 | 同一数据/Kit/场景对比；10k查询/滚动、连续开关详情、列表/日历/桌面切换，检查对象回收与UI线程阻塞。 |
| 人工 | Windows浅深/100～200%、320窄窗、键盘/物理IME/混合DPI/读屏分别记证据；Mac原生未验继续UNVERIFIED。 |

HTML原型只验证关系与局部交互，不能计入Qt产品PASS。开发每切片跑受影响用例；正式发布候选一次完整CTest和原有发布门禁，不把相同未变测试反复全跑。

## 11 全面评审与准入

| 区域 | 问题/裁决 | 实施门禁 |
|---|---|---|
| 品牌/颜色/排版 | 已有母版与分角色token足够，避免重新做logo或另起字体体系 | 浅深/对比/真实字体与图标比例 |
| 主窗分区/标题栏 | 导航/任务/详情职责清楚，保留原生边界修复 | 类别/范围切换几何不跳，叠放窗无黑边 |
| 详情内容 | 先前混合“截止与重复”等分组偏离现有实现；现收敛原字段与选择器 | 原字段全保留、草稿/错误/父子链回归 |
| 日期/提醒/时段弹层 | 高度常量导致空底；取消/应用/返回流程已有 | 自然尺寸、工作区夹紧、无无效滚动 |
| 分类/标签/菜单 | 使用现有单选/多选组件，不能用一个样例标签替代完整功能 | 搜索/长名/无结果/多选/新建/返回焦点 |
| 列表/日历 | 沿现有查询与四尺度，不以当前静态原型重写 | 范围不变、拖动/原行/详情同ID，锚点稳定 |
| 子任务 | 沿原两级事务与详情返回，不新增内存子项模型 | 级联影响、批量、移动与撤销 |
| 小窗/Mini/球 | 视觉统一；范围和动效仍由既有ViewModel/事务负责 | 八种启用集合、六条转换、草稿/菜单/拖动保持 |
| 账号/设置 | 保留现有账号动作与设置四分类，未来账号管理草案不混入当前设计 | 原服务选择/注册/本地路径、备份归通用、现有错误与本地资料保留 |
| 性能与可靠性 | 主要风险是重复会话、控件重建和全表刷新 | 对象/请求计数与既有性能基线 |

结论：**收敛后的视觉与技术方案可进入分切片实施**。独立agent只读核对现有QML，确认应复用字段/选择器及上述不可变行为；主代理补技术与全局自审。此结论不代表四形态、账号管理或Qt新界面已实现/验收。现有日历/账号功能按原契约复用，缺失账号接线仍须自己的实际服务测试。

终审补核：独立agent未发现阻断的接口、保存、时间或兼容矛盾；非阻断意见已修订为日期点击写同一草稿/未选择返回不改，以及总设计中的恢复邮件方向归后续队列。本轮具体通过/失败证据仍只见PROGRESS。

窗口承载最终修订：用户明确使用已有小弹窗后，由主代理复核并撤回停靠/整页方案。实施回到原editor Pane定位及大小逻辑，只精修视觉/自然尺寸，无新增宿主。前轮独立复审的单会话、字段、保存与兼容约束继续适用，其停靠几何部分不再作为准入依据。

## 12 实施队列与交付边界

1. 共享token与主窗分区：复用Theme/DesignTokens；详情直接沿用Main现有editor Pane，不接通停靠分支、不增加大面板，不动窗口原生裁边。
2. 详情与弹层精修：优先Main原控件装配、FormRow对齐及TaskInspectorPicker自然尺寸，再逐个检查TimeRangePicker/TagMenu，保持原测试身份。
3. 桌面视觉接入：只接共享视觉角色，不重写WidgetViewModel、输入mask或动画事务。
4. 账号/设置视觉：统一现有登录注册、同步资料和设置四分类的字级/字段/分组；不接新API，账号其他需求留在独立队列。

当前按用户授权接入共享样式并验证现有操作链，代码与截图证据见PROGRESS。仅实施视觉与自然尺寸，不新增协议/schema/账号功能；本轮不发布或推送。
