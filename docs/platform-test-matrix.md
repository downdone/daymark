# 平台验证矩阵

UX-34～36：macOS15.8.1/arm64/Qt6.10.2最终Release21/21（105.87秒）、Debug21/21（112.26秒）及两构建lint通过。历史436/共享详情34/连续月历及新增19阶段独立fixture、原45秒末段watchdog均通过；Cocoa三组193 PNG、warnings=false。10秒请求短跑分别11.898/12.087/12.217秒，各1循环/1完整性检查，最大延迟1/51/50ms。覆盖范围保持、共享详情/失败草稿、日周时间/DST/拖动、连续周回收/跨月条、完整原子创建/外部点击保存、紧凑导航与窗口及浅深/200%。详见[验证报告](verification-report.md)及PROGRESS；Windows、物理输入/辅助技术、真实通知/穿透、多屏睡眠/8h及独立部署签名仍UNVERIFIED。

UI-33：macOS15.8.1/arm64/Qt6.10.2 Debug17/17（62.31秒）及Release17/17（59.66秒），两构建QML lint无诊断；Release offscreen/Cocoa各436阶段、164 PNG及10秒请求短跑通过。Cocoa实际11.788秒、1循环/1完整性检查，菜单两呈现8ms。覆盖单一起止/DST/旧日期、普通拖放、右键动作及改名、日历同ID点击、保存回调隔离与200%。日志ui33-completed-*，详见[验证报告](verification-report.md)与PROGRESS。物理输入/IME/VoiceOver、Windows、真实通知/穿透、多屏睡眠/8h及独立部署签名仍UNVERIFIED，短跑不替代长时；本机Qt Kit开发产物。

UI-32：macOS15.8.1/arm64/Qt6.10.2 Debug/Release17/17、两项lint、Cocoa413阶段与158张合成截图通过。覆盖紧凑来源导航、子任务行指针直达可见详情、保存/草稿返回、两层服务约束和旧深层恢复。10秒短跑实际11.883秒PASS；物理输入/IME/VoiceOver、Windows、真实通知/穿透、多屏睡眠/8h及独立部署签名仍UNVERIFIED，详情见[验证报告](verification-report.md)。

UI-31当前macOS15.8.1/arm64/Qt6.10.2：Debug/Release17/17、两项lint及Release Cocoa407阶段通过，152张合成截图；覆盖六入口、设置/新增/数据、日历网格/议程和浅深/窄窗/200%。短跑11.933秒、1完整性检查PASS。手势独立通知、Esc草稿保护与本进程测试激活另有回归。实际证据见[验证报告](verification-report.md)及PROGRESS；物理输入/IME/VoiceOver、Windows、真实通知/穿透、多屏睡眠/8h及独立部署签名仍UNVERIFIED。

## UI-30 macOS补充（2026-10-25）

arm64 / Qt6.10.2 Debug17/17（54.27秒）、Release17/17（52.37秒），两项lint无诊断；Release Cocoa398阶段、145张合成PNG、双窗口/QML warnings=false，10秒短跑实际11.968秒、1循环/1完整性检查PASS，最大事件循环延迟1ms。覆盖五视图、智能数量入口/普通树、整树完成/删除/移动/分组恢复及最近删除树，schema9/JSON9和旧格式、事务回滚/CAS、局部编辑草稿和真实拖放高亮、浅深/窄窗/200%。详细过程失败、呈现采样修订和证据范围见[验证报告](verification-report.md)。

物理输入/IME/读屏、Windows当前源码、真实通知/穿透、多屏/睡眠/8h和独立部署签名仍UNVERIFIED；自动合成不替代这些门禁，Mac产物依赖本机Kit，全部测试数据库隔离。

## UI-29 macOS补充（2026-10-25）

arm64 Qt6.10.2 Debug17/17（56.73秒）、Release17/17（55.33秒）、两项lint PASS。Release Cocoa372阶段、136张合成截图、双窗口/QML warnings=false，10秒短跑实际11.823秒、1循环/1完整性检查PASS。覆盖紧凑桌面布局、简易子任务编辑实际列对齐、四组详情和锚定日期/分类/标签面板、分类带子节点时真实指针上下移动及重载；200%快捷必须正在编辑并位于视口。详细证据及过程修复见[验证报告](verification-report.md)和[PROGRESS](PROGRESS.md)。物理键鼠/IME/读屏、Windows当前源码、通知/穿透/多屏/睡眠/8h与独立部署签名仍UNVERIFIED，本机Kit产物不代表独立发行包。

## UI-27/28 macOS补充（2026-10-25）

arm64 Qt6.10.2 Debug17/17（55.22秒）、Release17/17（54.60秒）及两项lint PASS；Cocoa Debug361阶段/132张合成截图/双窗口、QML warnings=false，10秒请求短跑实际12.05秒、1循环/1完整性检查PASS。涵盖schema5～8真实临时库迁移/导入/回滚、统一任务时间和循环、周期提醒认领、分类两级/右键/实际指针拖放、父子列表/独立面板、简易退出自动保存及失败保留、全分类文件夹、固定视图按钮、导航窄轨及200%布局。详细指标、过程FAIL和静态审查见[验证报告](verification-report.md)。

原生合成与实机人工门禁分列：Windows当前源码、物理输入/IME/读屏、系统通知/鼠标穿透恢复、真实多屏/睡眠/8h与独立部署/签名均UNVERIFIED；本机Kit开发产物不是独立发行包。未操作真实用户库、无推送发布。

## UI-23A macOS补充（2026-10-25）

arm64 Qt6.10.2 Debug16/16（48.74秒）、Release16/16（47.85秒）、两项lint PASS；Cocoa333阶段及125张合成截图PASS。覆盖服务六目标与失败零修改、直接详情/安排保存、非排序列表到侧栏实际合成拖放、跨周跨月任务横条与事件共同溢出、隐藏事件/深色200%。原生定位失败及修复见验证报告。Windows当前源码、物理鼠标/触控板/键盘、IME/读屏及系统/长时/独立部署门禁UNVERIFIED。

## UI-21/22 macOS补充（2026-10-24）

UI-21：arm64 Qt6.10.2 Debug16/16（45.01秒）、Release16/16（44.33秒）、两项lint PASS；Cocoa Release309阶段及121张合成截图PASS，双窗口/warnings=false。覆盖内容页/统一导航草稿、归属项目层级/标签独立、页面创建与概念切换、后台维护期间手势协调；同时保留任务/日历/事件/数据恢复等既有回归。UI-22最新结果见PROGRESS及验证记录。

UI-22：相同Mac Kit最终Debug16/16（46.16秒）、Release16/16（45.15秒）、两项lint PASS；Cocoa309阶段全截图PASS，已查看窄窗200%减负后整理/重复页。排序许可真实QML绑定回归PASS，后台维护/显式操作反馈分开，草稿/原规则保留。中间滚轮/长按/布局FAIL、探针呈现同步及属性通知修正见验证报告；不称一次通过，不把QML warnings=false当成无任何Qt/系统诊断。

此证据不包括物理鼠标/键盘/触控板、中文IME、VoiceOver、通知权限/送达、穿透外部恢复、多屏/DPI、睡眠/8h、独立部署/签名；这些仍UNVERIFIED。Windows未在本次Mac环境复测当前源码，历史Windows记录保留原版本范围；Intel和macOS原生开机启动仍未验证/未实现，不将旧表批量改PASS。

## macOS arm64 首次构建证据（2026-10-24）

macOS15.8.1 / Apple M3 Pro / Qt6.10.2，macos-debug配置与编译PASS；完整CTest16/16（46.09秒）、两项QML lint PASS。覆盖实际临时SQLite、迁移/事务/恢复、任务/组织/日历/事件/循环/提醒、worker、双进程单实例、10k基准及离屏QML合成流程。首轮单实例路径超限及修复见[验证报告](verification-report.md)。QML最终双窗口/warnings=false，只证明合成流程；窗口flag测试不替代真实OS交互。

Release、Intel、原生UI截图/物理输入/读屏/系统通知/穿透恢复/多屏睡眠/8h/独立部署/签名公证保持UNVERIFIED，macOS原生开机启动仍未实现。下方2026-10-22起的原始表及追加记录保留各轮历史范围；最新状态以[进度](PROGRESS.md)为准。

## 初始Windows验证记录

2026-10-22，1.0.0-rc.1 Windows本地候选。用户确认暂缺macOS环境；PASS只针对行内所述范围，macOS未执行任何本轮构建。

| 场景 | Windows11 x64 / Qt6.10.2 MinGW | macOS Intel | macOS Apple Silicon |
|---|---|---|---|
| Debug/Release配置与编译 | PASS | UNVERIFIED | UNVERIFIED |
| 批量事务/归档/schema2迁移/旧JSON/周期配置 | PASS；临时SQLite与失败注入 | UNVERIFIED | UNVERIFIED |
| Windows开机启动 | 隔离INI测试PASS；真实Run登记/系统登录触发UNVERIFIED | 未实现原生启动适配 | 未实现原生启动适配 |
| 每视图位置/折叠导航/大字列表/Floating-Mini | PASS；QML及原生合成流程/截图 | UNVERIFIED | UNVERIFIED |
| 真实临时SQLite/WAL/回滚/重开/外键 | PASS | UNVERIFIED | UNVERIFIED |
| CRUD/六视图/搜索/日期与版本冲突 | PASS | UNVERIFIED | UNVERIFIED |
| 项目/领域/Heading/标签/清单、安全删除 | PASS（含事务失败注入） | UNVERIFIED | UNVERIFIED |
| Today排序/今晚与重启恢复 | PASS（服务与异步模型测试） | UNVERIFIED | UNVERIFIED |
| CalendarService边界/合并标记/改期回滚 | PASS | UNVERIFIED | UNVERIFIED |
| 重复规则/幂等/手改保护/完成后N天 | PASS | UNVERIFIED | UNVERIFIED |
| 提醒认领一次/持久记录/完成抑制 | PASS；系统实际送达UNVERIFIED | UNVERIFIED | UNVERIFIED |
| JSON预览/严格校验/追加/替换/快照恢复/每日备份 | PASS；真实SQLite临时文件 | UNVERIFIED | UNVERIFIED |
| worker模型、退出排空、单实例子进程协议 | PASS；不证明人工焦点行为 | UNVERIFIED | UNVERIFIED |
| 各功能页、草稿保护、浅深/200%详情QML启动 | PASS（offscreen和Windows后端合成流程） | UNVERIFIED | UNVERIFIED |
| 原生QWindow模式/位置/可见性/置顶/穿透flag | PASS（Qt状态和临时设置） | UNVERIFIED | UNVERIFIED |
| 真实拖动/缩放/层级/焦点/穿透外部恢复 | UNVERIFIED；代码入口已实现 | UNVERIFIED | UNVERIFIED |
| 托盘关闭保留进程/系统通知权限与送达 | UNVERIFIED；代码已实现 | UNVERIFIED | UNVERIFIED |
| 屏幕热插拔/DPI/系统午夜/睡眠/时区 | UNVERIFIED；边界算法/定时刷新已有 | UNVERIFIED | UNVERIFIED |
| 10k混合数据加载/搜索/月索引 | PASS；155/2/11 ms，i7-13700HX Release数据层 | UNVERIFIED | UNVERIFIED |
| 透明桌面可读性/IME/读屏/高对比 | UNVERIFIED | UNVERIFIED | UNVERIFIED |
| 签名/独立干净机/8h稳定性 | UNVERIFIED | UNVERIFIED | UNVERIFIED |

部署包在本机去除SDK PATH后单独测试，结果追加验证报告；这不是独立干净机验收。不得批量将本表改为PASS。

## V-02W自动恢复与短跑补充（2026-10-23）

| 范围 | Windows11 x64 / Qt6.10.2 | macOS |
|---|---|---|
| 时间/恢复MSG类型、合并、析构取消 | PASS；消息注入，非真实睡眠 | UNVERIFIED |
| 写入时延后维护、保留校验错误与编辑状态 | PASS；隔离模型及原生合成流程 | UNVERIFIED |
| 主窗规范化、隐藏/最小化保持、销毁安全 | PASS；Qt状态与合成离屏坐标 | UNVERIFIED |
| 连续业务循环/完整性检查/资源采样工具 | PASS；Debug 3循环、Release 6循环 | UNVERIFIED |
| Release 10k混合数据层 | PASS；129/1/8ms加载/搜索/月索引 | UNVERIFIED |
| 8h、真实睡眠、物理拔屏/DPI、闲置CPU与内存趋势 | UNVERIFIED；短跑不能替代 | UNVERIFIED |

本轮数据与短跑限制见[验证报告](verification-report.md)，执行方法见[指南](stability-guide.md)。

## CAL-05浏览与定位补充（2026-10-24）

Windows自动：实例排序/只读/失效key/软删测试PASS；Qt合成主/独立窗日期跳转、非法日期保持、实际预览/编辑路径PASS；原生180阶段、浅深/200%截图与420×480逻辑窄窗200%预览PASS。严格SDK隔离、清单/ZIP与运行库来源PASS。上述不替代物理键鼠/IME/读屏/多屏/真实通知/8h或干净机，macOS仍UNVERIFIED。当前包与源码证据只见PROGRESS。

UI-18：Windows原生219阶段自动合成场景/截图PASS，包括快速→完整共享草稿、明确离开保护、内联日期和独立日期菜单当前值、时间选择取消/错误保留、深色200%固定操作及既有滚动回归。最终构建/CTest/严格本地部署数据见PROGRESS；不等同真实鼠标/IME/读屏验收，macOS仍UNVERIFIED。

UI-19验收范围：Windows新增原行展开/完整可见、长备注摘要不丢内容、大字号焦点与手动滚动、新建资料与任务草稿的独立提交、成功完成反馈/减少动态效果、菜单同窗呈现和10k菜单可用性预算。命令耗时与原生两次呈现上界分别记录，不宣称所有物理输入延迟。最终执行结果见PROGRESS；真实键鼠/IME/读屏/多屏/睡眠/8h/干净机和macOS仍UNVERIFIED。

UI-20新增行范围：连续跨日周段/循环日程实际实例与时间预览、精简提醒一次提交、任务和清单合成拖动/Esc/落库、导航独立折叠偏好、校验错误放弃和存储错误保留。Windows构建/数据/合成QML证据见PROGRESS；物理手势/真实键盘读屏/平台系统门禁及macOS仍UNVERIFIED。
