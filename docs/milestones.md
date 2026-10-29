# 里程碑与可执行任务队列

SYNC-07/UX-61按用户要求先完成[独立账号窗口设计/评审](design/account-window.md)，再迁移入口与完整账号状态；原设置账号块移除。实际实现/验证状态见PROGRESS，不以设计评审代替渲染与测试。

SYNC-05按用户后续“完整实现→测试→文档/CI→推送源码”指令推进；运行代码及命令见[server](../server/README.md)、桌面适配见[实现契约](design/sync-client-implementation.md)。SYNC-02～04下列条目记录原设计顺序，不能继续解释为禁止已获授权的实现；当前完成度及平台边界仍只见PROGRESS。

SYNC-04在[强类型契约](design/sync-contracts/README.md)给出OpenAPI、Go/Qt声明、字段/默认值/三态/摘要映射及客户端同步DDL；开发前依[ADR-028](ADR/028-sync-api-contract-and-rpc.md)评审RPC与兼容门禁，DEV各批沿用相同用例/事务接口，不将传输格式耦合进领域。实际状态仅见PROGRESS。

SYNC-03已将SYNC-02细化为[DEV-01～08模块开发顺序](design/sync-modules/README.md)：先逐模块UT，再真实SQLite/文件/HTTP组件CT，按[INT-01～22](design/sync-modules/testing-integration.md)逐层组合至双端/灾备/平台；DDL仅设计稿，实施按批次追加migration，不将设计校验当服务已通过。状态及真实证据只见PROGRESS。

SYNC-02按[账号同步工程架构](design/server-architecture.md)第15节ARCH-1～6顺序推进：契约基线→真实SQLite事务→账号资料→普通任务双端纵片→完整业务/快照冲突→恢复与平台交付；首期Go单体+SQLite遵循ADR-026，网络契约见[v1协议](api/sync-v1.md)。不自动部署/真实上传；状态和门禁证据只见PROGRESS。

UX-60按[全局字体与比例留白](design/typography-and-proportional-spacing.md)统一中西文字体管线、角色字级和字号对应的页面/行内空间，复用实际几何、原生截图与完整CTest；状态只见PROGRESS。

UX-59按[提示与标题/工具栏契约](design/tooltip-and-layout-rhythm.md)落实P-18/19、单目标视图切换、低频显示选项收纳与垂直节奏；本地包按[ADR-025](ADR/025-minimal-portable-payload.md)精简，状态和交付证据仅见PROGRESS。

UX-58按[稳定空间契约](design/stable-layout-and-scroll.md)定义P-15～17，再迁移详情/同类滚动表面、循环控件、日历工具栏和组件几何，验证坐标关系、浅深/大字及真实构建。当前状态只维护于PROGRESS。

UX-57按[详情尺寸控制](design/inspector-size-control.md)先修复强制展开、收紧内容关系，再验证同会话/键盘/缩放及打本地便携包；平台状态和测试证据只维护于PROGRESS。

UX-56按[两栏设置](design/settings-split-navigation.md)实施侧栏/正文与键盘/滚动，再验证截图、完整测试、本地便携和源码推送；[ADR-024](ADR/024-local-preview-package.md)仅界定本地预览边界，正式门禁不变。当前状态只维护于PROGRESS。

UX-55全菜单按[语义清单](design/menu-system-review.md)实施共享分隔、入口分组和托盘恢复/设置；实际完成状态与平台限制见PROGRESS。

UX-52～54：按设计契约收束任务行留白/首次呈现、重复预设/结束重复和桌面实体菜单锚点；沿既有切片验证与预算执行，状态和限制只维护于PROGRESS。

UX-49～51按[定位与集合恢复](design/inspector-position-and-trash-selection.md)→[列表轴线](design/list-layout-and-editing-rhythm.md)→[双击原行草稿](design/list-double-click-draft.md)执行；延续同一编辑器/事务，列表和上下文探针各守本身预算，不增加重复套件。真实状态只见PROGRESS。

UX-48先按[全局规则](design/global-visual-system.md)更新共享视觉源，再迁移专用页面/组件、补全矢量与文字样式样板，执行构建/lint/完整CTest尝试及跨表面截图QA；实际状态只维护PROGRESS。

UX-47按[设置与页签契约](design/settings-navigation-and-inspector-tabs.md)整理设置目录/子页和任务内容页签，沿用原持久化及草稿门；完成证据见PROGRESS。

UX-46按[就近面板精修](design/contextual-popover-polish.md)先外壳/属性行规则→新增样板→共享详情/时段小窗→构建/lint/既有UI入口与截图矩阵；保持紧凑高度、可访问命中和草稿链。状态仅见PROGRESS。

UX-45按[直接时段与批量整理](design/direct-time-range-picker.md)推进今天入口/只读时间映射→就近范围小窗与日期任务拖选→旗标/集合删除事务→临时库、真实QML操作与浅深200%验收；不改变schema或放宽旧watchdog，状态仅见PROGRESS。

UX-44按[年定位、标签菜单与尺寸](design/year-tags-and-calendar-sizing.md)推进布局后定位及月份语境→共享就近标签菜单→独立日历缩放/键盘→复用现有测试入口验证；不新增重复CTest套件，状态仅见PROGRESS。

UX-41按[主框架与字标](design/sidebar-and-wordmark.md)依次移除应用内菜单/设置入口补齐→原创字标→侧栏样板与大字→临时库回归、像素绘制及原生截图；不新增重复CTest入口，平台门禁仍独立。状态仅见PROGRESS。


UX-40先按[提示与菜单字级](design/help-tags-and-menu-type.md)定义准入/视觉，再迁移共享样式和各表面；日历恢复连续拖选、年日号唯一表达及草稿拖动/离开保存，复用现有临时库探针验证，不另加重复CTest入口。状态只见PROGRESS。


UX-39按[macOS日历操作](design/macos-calendar-interaction.md)实施：选择与双击/日期钻取→日历内新建/快捷键/焦点→视觉/拖动反馈→独立临时fixture与既有拖动/草稿回归；FR-038覆盖UX-35单击新建约定。

UX-38按[全窗口精修](design/desktop-surfaces-polish.md)顺序实施：小窗/迷你扫读与完成闭环→共享工具/滚动边缘→窄日历与实色→独立fixture及全回归；沿用UX-37C性能与平台门禁，不以外观精修覆盖未验证项。

任务列表下一轮拟按[UX-37复审](design/task-list-interaction-review.md)拆为选择闭环→父子关系及行骨架→详情直接呈现与首帧测量。本轮已授权落实选择、关系、密度及直接呈现；UX-37C会话重构/30次性能矩阵单列后续，不扩大数据库或任务树语义。Windows现有UI/watchdog失败继续作为发布门禁，状态只见PROGRESS。

用户追加的[UX-37D全局密度](design/desktop-density-review.md)作为上述三片共同约束：先标准件/列表详情样板，再同屏导航统一，随后日历与其余表面分片迁移，最后密度/响应/平台矩阵；保持字号、可访问命中和单一草稿。

UX-35/36按[连续月历](design/continuous-month-calendar.md)和[完整新增与桌面密度](design/calendar-creation-and-density.md)追加：只读有限周模型→真实时刻/范围新建→共享字段草稿/事务创建→密度收敛→历史/共享面板/连续月历独立fixture验证。对应FR-036/037；不得通过写占位任务或扩大原watchdog预算简化实现。状态与真实证据仅在PROGRESS。

UX-34按[系统评估](design/system-ux-review-2026-10.md)第6节安排R0可信基线→R1范围/返回→R2统一表达→R3就近编辑→R4月历→R5日周小时轴→R6设置/数据/组件分片→R7年导航和平台验收。FR-034/035已接受实施，代码/验证映射见[实现索引](design/ux34-implementation.md)，设计见[日历v2](design/calendar-workspace-v2.md)、[任务面板](design/contextual-task-inspector.md)及ADR-020；未实施不放占位按钮，不以设计完成替代功能交付。当前状态仅在PROGRESS。

UI-33按[统一起止与动作设计](design/shared-task-time-and-actions.md)及ADR-019实施：C++时间/旧资料与提醒 → 共享日期编辑 → 普通列表拖动/任务菜单 → 分类标签重命名 → 双构建、原生输入与缩放。当前状态仅见PROGRESS。

UI-32按[两层子任务与导航设计](design/compact-navigation-and-subtasks.md)及ADR-018实施；新建/改父校验、可见详情/草稿返回、紧凑来源导航及旧深层兼容为本片门禁，状态见PROGRESS。

UI-31全窗口统一按[桌面语言设计](design/macos-desktop-language.md)分片构建、截图与回归；收集箱上移至系统入口、自建分类区保留两级文件夹。当前状态和证据仅在PROGRESS。

UI-29 / FR-024～030按[紧凑桌面设计](design/desktop-task-inspector.md)处理详情信息层级、导航密度、展开编辑对齐及分类实际上下排序；双构建、CTest、lint和Cocoa证据记录后结束本片。UI28交付范围和局限保留在PROGRESS。

UI-27按[统一任务系统](design/unified-task-system.md)覆盖用户FR-024～029：分类迁移 → 真正子任务/重要/置顶 → 统一时间/循环/提醒 → 共享编辑及列表/日历导航 → 双构建、迁移/导入、合成及原生门禁。新定义替代历史项目领域/双事件/清单切片，保留数据与平台门槛；唯一状态见PROGRESS。

本文件细化原始 Plan，不替代需求与原任务 ID。状态唯一记录于 PROGRESS；这里的任务编号用于逐轮开发和追踪。工期沿用 Plan 的63～121有效工程人日初估，完成真实 M0/M1 后重估，不据此承诺交付日期。

UI实施在对应M3/M4/M5业务切片中采用 [UI-01～06迁移顺序及UX门禁](design/ui-acceptance.md)，先做Today完整样板再推广。它不改变原功能依赖，也不自动将项目优先级改为全面视觉重写。

## 阶段依赖与门禁

| 阶段 | 前置与交付结果 | 门禁证据 |
|---|---|---|
| F 工程基线（本轮） | Qt 6.10.2、构建/测试、双窗预览、开发方法和设计 | Debug/Release build、2项 CTest、文档完整 |
| M0 平台 POC | F；透明、置顶、迷你、穿透恢复、几何 | 真实 OS 操作矩阵；缺 macOS 可继续业务但不算双平台完成 |
| M1 可用基础 | F，与 M0 逐切片交错；领域、迁移、单实例、错误 | 临时库迁移/失败回滚、双进程激活、空应用真实运行 |
| M2 存储闭环 | M1；任务和组织数据可持久化 | 创建到重启/恢复完整链，软删和事务失败安全 |
| M3 主窗闭环 | M2；无需日历也能日常使用 | PRD §1.3、详情、搜索、键盘、跨天回归 |
| M4 日历闭环 | M3；月/周规划与拖动 | 开始/截止独立、冲突零修改、跨视图同步 |
| M5 桌面 MVP | M0 已实现能力 + M3/M4；真实任务组件 | 连续完成20项、托盘恢复、多屏定位、两平台独立报告 |
| M6 效率能力 | M5；重复与提醒 | 月底闰日、幂等、模板覆盖保护、通知失败回退 |
| M7 V1 发布 | M6；数据保护、稳定性、原生安装包 | P0全验收、P1披露、10k/8h、干净机、Windows/macOS分别验证 |

M0/M1 优先解决窗口和可靠性；不得跳到云、AI、移动端。Windows 单机环境不产生 macOS PASS。

## 第一批执行队列

每行是一个可独立审查的纵向切片，完成后构建测试并更新证据。序号不是预先排定的日历日期。

| 顺序/ID | 任务与依赖 | 原计划/需求 | 可观察验收 |
|---|---|---|---|
| F-01 | 工具链、CMake、脚本、双窗启动 | M0-01/02、M1-01/07 | Debug/Release构建；QML加载和临时SQLite探针通过 |
| F-02 | 方法、原则、设计、任务与证据导航 | 工程纪律 | 后续会话可由PROGRESS定位一个任务并复现命令 |
| W-01 | 平台窗口适配与Floating/Mini/置顶/拖动；依赖F | M0-02/03、FR-012 | 两种布局切换可用，置顶与不置顶实际可观测，失败提示 |
| W-02 | 托盘、主窗恢复入口、穿透；依赖W-01 | M0-04、FR-012/013、TC-027/029 | 真实穿透后托盘解除；无托盘禁止穿透；明确退出 |
| W-03 | 设置、屏幕几何、锁定、重置；依赖W-02 | M0-05/06、TC-028/041 | 重启/拔屏仍可达，DPI与原生窗口矩阵有证据 |
| B-01 | Result、IClock、领域值和日期验证；依赖F | M1-03/06 | FakeClock跨天；本地日期/UTC往返；安全错误码 |
| B-02 | 数据目录注入、SqliteDb、初始迁移；依赖B-01 | M1-04 | 临时库重入迁移、FK/索引、版本过新拒绝；未触碰用户目录 |
| B-03 | 一致备份与迁移失败路径；依赖B-02 | M1-04、FR-017 | 既有库迁移前快照；备份失败阻止迁移；失败回滚原数据 |
| B-04 | 单实例与退出队列；依赖W-02/B-03 | M1-02/05、TC-030 | 第二进程激活且不写DB，服务竞态/陈旧锁覆盖 |

默认 W-01→W-02→W-03→B-01～04；若平台验证受外部环境限制，可继续独立 B 子项，保留平台未验证状态，不伪造门禁完成。

## 后续详细分解

| ID | 切片与依赖 | 对应原计划 | 验收/测试 |
|---|---|---|---|
| D-01 | TaskRepository、TaskPatch、最小创建/列表调试UI；B完成 | M2-01/02/06 | 创建中文任务→重启读取；三态和日期拒绝；TC-001/002/040 |
| D-02 | 完成/取消/重开/软删撤销；D-01 | M2-01/06 | 同一ID生命周期、终态时间和版本冲突；失败无半状态 |
| D-03 | Area/Project/Heading及安全删除；D-02 | M2-03 | TC-014～016，完成项目不改任务，删除迁出事务失败回滚 |
| D-04 | Checklist/Tag/唯一键/稳定排序；D-03 | M2-04/05 | TC-017～019，关联重启保持，重排失败保留顺序 |
| D-05 | JSON格式契约与导出基础；D-04 | M2-07 | 冻结版本字段、UUID/日期格式、合成样本，无机器路径 |
| U-01 | QueryService及导航/Inbox/Today/Evening；D完成 | M3-01/02/03/09 | TC-003～013，快速录入20项，880DIP主路径可用 |
| U-02 | Upcoming/Anytime/Someday/Logbook；U-01 | M3-04/09 | 日期/项目优先级，恢复任务无重复，Today移出有实际效果 |
| U-03 | 项目页面、详情、日期/标签/清单编辑；U-02 | M3-05/06 | 400ms防抖、关闭保留草稿、日期冲突不写库 |
| U-04 | 搜索、主题、错误空态、跨天刷新；U-03 | M3-07/08/09 | TC-020/043/044，中英文大小写、午夜/唤醒、键盘焦点 |
| C-01 | CalendarService+MonthCalendarModel与月历；U完成 | M4-01/02/03 | TC-021/025，双日期标记、同日合并、跨年闰月 |
| C-02 | 日期新建与Start/Deadline拖动；C-01 | M4-04/05/07 | TC-022/023，非法移动保留DB；合并项先选择字段 |
| C-03 | 周历与视图联动；C-02 | M4-06/07 | TC-024，周首日/跨月/Today联动，无小时轴 |
| G-01 | WidgetTodayModel与真实任务显示/完成；C完成、W完成 | M5-01/02/03 | TC-026，连续完成20项，各视图同源同步 |
| G-02 | Mini、透明度、锁定、定位/托盘回归；G-01 | M5-04/05/06/07 | TC-027～029/041，窗口重建后恢复通路有效 |
| G-03 | Windows/macOS组件独立验收；G-02 | M5-08 | 平台矩阵、Spaces局限、闲置CPU基线；MVP试用门禁 |
| R-01 | 重复模板和FIXED实例化；G完成 | M6-01/02/03 | TC-031～033，31日/2月29跳过、重复执行不重复 |
| R-02 | 完成后N天与模板编辑保护；R-01 | M6-04/05 | TC-034/035，最多一个OPEN后继，历史/覆盖实例保留 |
| R-03 | ReminderService、OS适配与应用内回退；R-02 | M6-06/07 | TC-036/037，过去时间拒绝，睡眠重排，失败不冒充送达 |
| V-01 | 完整JSON导入导出和备份恢复；R完成 | M7-01/02 | TC-038/039，WAL往返、无效数据零修改、替换先备份 |
| V-02 | 10k基准与8h/睡眠/多屏回归；V-01 | M7-03/04/05 | TC-040～045，硬件/构建类型/样本/实测数字齐全 |
| V-03 | 原生打包及许可/用户说明；V-02 | M7-06/07/08 | 部署工具、干净系统运行、签名状态披露 |
| V-04 | 全量验收与发布清单；V-03 | M7-09 | P0、V1基础P1、未关问题及平台证据审查；授权后发布 |

## 共通验收约束

每个任务覆盖适用的失败分支；UI自动探针不能替代人工窗口验证。未满足项保持TODO/BLOCKED，不“完成后补测试”。新需求先映射FR/TC并评估依赖；没有规格对应的扩展放候选清单，不插入当前切片。

任务状态见 [PROGRESS](PROGRESS.md)，执行记录用 [模板](templates/task.md)，结果见 [验证报告](verification-report.md)。

## Windows候选后的执行切片

V-02W按[稳定性设计](design/windows-stability.md)补系统恢复回归和同进程隔离长跑/资源报告工具。先运行短时工具验证，再执行8h与真实睡眠/物理多屏；这些是不同验收项，当前结果仅在PROGRESS维护。V-03/V-04仍保留干净机、签名许可和macOS门槛。

V-03W按[交付验收设计](design/windows-delivery.md)先实现只读完整性与篡改检查、无SDK独立验收和完整生命周期DLL来源检查；可独立工程验证不解除V-02依赖，也不替代独立干净机、许可或macOS验收。

UI-14按[Apple HIG全界面设计](design/hig-redesign.md)分三片：共享组件与主导航/空态 → 分类设置与全部编辑/数据页 → 日历、提醒、组件及完整回归/严格部署。覆盖FR-001～019、UX-01～14；每片构建测试后再集成。所有入口、旧选项、草稿保护和恢复路径必须保留；Windows自动/截图、物理输入/辅助技术与macOS验收分别记录，不因全界面实现就解除平台门禁。

CAL-03按[独立日历设计](design/desktop-calendar.md)扩展FR-020：参数化共享查询、独立窗口与透明/几何持久化、共享日历精修，再验证三窗并存/独立翻页/数据同步/草稿/200%与严格Windows部署；平台人工门禁保持独立。

CAL-04按[事件系列设计](design/event-series.md)扩展FR-021：领域规则与004迁移/旧JSON兼容 → 表单/色条/提醒中心 → 月末闰日/DST/提醒去重及失败分支 → Windows构建、原生截图和严格部署。编辑删除整个系列，真实系统通知送达与macOS保留独立验收。

CAL-05按[评估路线](review-rc2.md)及[浏览定位设计](design/calendar-browsing.md)依次交付查询排序/定位键、只读预览、直接日期跳转和本地验证。其后CAL-06、REL-01、CAL-07、ENG-01、REL-02分别解决查找密度、真实平台验收、循环例外、维护性及正式发布；不混淆设备依赖和可本地实现项。

UI-15按[动作与配色审计](design/actions-and-color.md)交付：共享图文组件/角色token → 全页面按动作风险与可识别度迁移 → 浅深/200%/可访问名称、对比度与原生/部署验证。范围不替代CAL-06功能计划，也不解除平台人工门禁。

UI-16按[任务详情布局](design/task-detail-layout.md)先定义信息架构/行关系/条件显示，再实施Main与TaskDetailsFields纵切；验收互斥归属/分组、移动后的今晚/提醒、宽窄/200%与草稿原子性，不扩大CAL-06范围。

UI-17按[iOS分组设计](design/ios-grouped-details.md)统一圆角/图标/开关/画布和内容组，移除拖放伪按钮，按202阶段与本地部署验收。

UI-18按[就近编辑设计](design/direct-editing.md)推进共享日期/时间、快速编辑与全页直接选择，保留原子草稿和平台门禁。

UI-19按[原行编辑及即时反馈](design/inline-task-experience.md)修订快速卡形式、整理与创建、详情动作、列表/完成反馈和菜单响应。

UI-20 / CAL-06A按[连续日程与视觉修订](design/calendar-visual-refresh.md)推进跨日色条、长按排序、循环日程、全局颜色/行关系及提醒单层提交。

UI-21按[产品哲学](product-philosophy.md)与[导航页面契约](design/navigation-and-pages.md)先复审责任/结果/场景和主操作链，再交付主内容管理页、领域项目层级/标签独立入口及完整草稿与平台验证；当前状态只记录于PROGRESS。

UI-22～26按[全产品复审与简约方案](design/product-experience-review.md)单切片推进：先清理重复常驻元素/入口及状态措辞，再做今日与澄清、项目领域工作页、时间查找、设置恢复。遵守P-14“简约而不简单”；保留FR-001～022、草稿与安全语义，CAL-06/07、ENG-01和真实平台门禁不因重设计取消或宣称完成。

UI-23A按用户截图及[直接操作设计](design/direct-task-actions.md)先实现详情/安排/导航移动闭环并构建，再增加FR-023任务日期范围投影与共享月周日历，最后真实QML指针、临时数据、双构建与原生合成验收。状态仅见PROGRESS；不扩大为小时排程。

UI-30按[提醒事项式视图与任务树](design/task-list-context.md)和ADR-017推进FR-031：先树事务/schema9及旧JSON，再五视图/最近删除/局部编辑，最后完整构建与原生合成；替代旧父子独立语义的部分，状态仅见PROGRESS。
