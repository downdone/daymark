# TIME-01 离线智能时间输入

设计依据：[新增能力标准](new-capability-standard.md)、FR-001/012/014，UX-01/04/09/11/13。2026-11-06用户授权技术设计评审后实现。这里只维护契约，当前实现/验证状态见PROGRESS。

## 1 功能定义

主窗快速添加、小窗原位添加同一规则；保存一次生成任务及时间/提醒。标题原文始终保留。默认识别开启；仅日期只安排；明确时刻安排并提醒；明确“前/之前/截止”仅截止日，不猜当天23:59；“不要提醒/不提醒”抑制自动提醒；此时只持久化计划日，时刻保留在原始标题，不虚构日历时段或在建议中显示未保存的计划时刻。单时刻不生成虚构时长。

支持中文今天/明天/后天、(本/这/下)周X/星期X、yyyy-MM-dd、yyyy年M月d日、M月d日、上午/下午/晚上/中午＋数字时分、HH:mm、阿拉伯数字N分钟/小时后。中文一至二十三点及半点；只识别有边界的时间单位，3点式/11月报告/版本/编号不识别。

同一语句多个独立日期/时刻、非法日历、已过时刻、模糊早晚、复杂循环/时间范围→NeedsChoice，不自动采用部分结果；Enter保留草稿并提示选择；点击“忽略”后可保存纯标题，预览明确“未自动采用”。本周含今天；无前缀星期指本周尚未来到的同日，否则下周；明确本周已过日期不滚动。无年M月d日选择下一次合法日期且预览完整年份；明确年绝不改年。计划过去日期可保存，不补响过去提醒；过去时刻整体需选择。DST不存在/重复墙钟需手工选择，不猜偏移。

## 2 技术选型

C++20/Qt6.10.2 QRegularExpression、QDate/QTime/QTimeZone；纯离线、无第三方/AI。规则小而确定，输入最多1024 UTF16单元后保守拒绝解析；最终标题仍遵循512码点原校验。时间/时区显式注入，无SQL或网络。第一期不支持复杂自然语言循环，不把旧PRD的“复杂自然语言V1不做”误作禁止本次已授权切片。

## 3 架构

TextField原始输入→共享InputTimePreview→TaskWorkspace.previewTaskInput→TaskTimeRecognition::parse(原文,nowUtc,zone,options)→建议；提交时同一TaskService::createFromInput重新解析或采用手工override→原子create/edit→Repository/既有提醒/同步。QML不做日期推断或Today过滤。原有create接口保持普通字面量语义，程序/导入/既有编辑不被偷偷解析。

## 4 模块

TaskTimeRecognition：无副作用语法扫描/语义/未来提醒/DST、单元测试。TaskService新命令：合并draftForScope与识别、保持分类/标签、显式日期覆盖入口默认，原子落库。TaskWorkspace：偏好、手工override白名单校验、预览映射与异步上下文。InputTimePreview：文本旁建议/忽略/手工改时间，独立草稿状态、无落库。现有提醒模块：UTC认领/去重/弹窗与通知，无新轮询。

## 5 协议

无新RPC/HTTP/IPC。进程内QML map协议format=1；预览state=none/ready/choice，date ISO日、time HH:mm、target=planned/deadline、reminder布尔、summary中文、reason稳定key；span索引为UTF16，允许空。手工提交override只允许date/time/target/reminder，全部重校验，不信任QML传入UTC/SQL/资料路径。原有同步HTTP JSON v1复用已持久字段；远端/导入只取明确事实，不重新解析标题；旧客户端能读取同schema。

## 6 接口/结构

RecognitionOptions{enabled=true,dateOnlyReminder=false,dateOnlyMinute=540}; Recognition{state,reason,target,date,time,reminderUtc,spans}; parse(QString,QDateTime,QTimeZone,Options) const，结果无概率数字。Override{date,time,target,reminder}，手工选择优先，非法/过去提醒拒绝提交。

TaskService::createFromInput(title,view,options,ignore=false,date={},override={}) -> Result<Task>。先按原scope创建，识别只覆盖时间字段、不覆盖分类/标签/关注。截止日覆盖入口默认计划日，避免默认明天和截止今天形成无效范围。无日期/忽略仍保持原scope；NeedsChoice返回timeInputNeedsChoice，防止提交排队时提醒过期而静默丢失提醒。仅成功事务后QML清空；数据库失败无半任务、提醒/outbox亦回滚。

TaskWorkspace::previewTaskInput(title)->QVariantMap；createTask和createWidgetTask追加默认可选ignore/override，旧调用兼容。Worker内提交，时钟重新取值处理“20分钟后”，不会在UI线程SQL。preview仅建议不持久，手工选择保存精确墙钟；提交发现已过/坏日期显示错误保留输入。

## 7 数据库

不改schema10，不添加识别表/索引。复用tasks.start_on/deadline_on（本地日）、remind_at_utc（UTC瞬间）、reminder_zone（当前IANA区）。识别不生成schedule/end。偏好置本地ui.ini：recognizeTaskTime=true、recognitionDateReminder=false、recognitionReminderTime=09:00；不随任务同步。预览/忽略/手改只存本次QML草稿，下一条任务重置；不写日志/JSON任务扩展。create/edit用既有atomically/savepoint，失败零写。

## 8 HIG/UX

采用HIG输入减负、清晰反馈、用户控制及可恢复。主窗/小窗输入下方仅有结果时出现一行可换行建议；完整年月日＋24h时刻＋提醒状态，文字可点开手工DateField/TimeField，独立“忽略”按钮。原文不替换，不干扰光标/选择/中文preedit；IME composing时不提交也不显示过期结果。Enter保存一次；Tab可访问建议/忽略，Esc取消时间弹层保留标题，成功清空后还原自动识别。输入非空/时间弹层打开维持小窗操作状态，不触发自动收球。

手工日期/时间只有本条有效。既有任务编辑不重解析，不覆盖手工安排；分类/资料切换沿既有草稿门。失败保持输入/忽略/手选，不能把用户选择抹掉。200%窄窗建议换行/弹层限屏，浅深/键盘/辅助名称与SettingsGroup共享样式。

## 9 可靠性/隐私/性能

最多有限正则扫描，预览随输入更新但不查库、不启动线程或网络；不收集语料、标题不写日志。单调/相对时间按提交时now重新计算；本地日期用addDays，DST墙钟用Reject转换并验证。显式时间覆盖日期入口默认，用户手工override覆盖解析，忽略最高优先；不触发过去提醒、不回填历史标题。

## 10 验证

规则：日期/中英文标点、数字/中文时分/半点、12点换算、周与跨年、2/29、未来/过去、多时间/区间、版本号/3点式/11月报告、禁提醒、DST gap/fold、输入上限。注入固定时钟/时区。

集成：QTemporaryDir真实SQLite，主/小窗同结果、分类/标签保留、入口默认覆盖、提醒字段/去重、忽略、手改、设置持久化、非法override/写失败回滚、旧接口/编辑/同步不变。QML：实际输入/IME守卫、建议修改与忽略、失败保留、成功清空、浅深/200%和小窗布局；物理IME/多屏/读屏单列。Release build/CTest/相关QML lint，macOS/Linux按当前用户指示不跑。

## 11 设计自审

评审主体：单代理技术自审，非独立评审。

| 风险 | 修订/结论 |
|---|---|
| 日期等于提醒导致猜时刻 | 日期默认只计划；可配置纯日期提醒 |
| 自动覆盖手工/历史 | 只快速新建；原create/edit不变，手选override优先 |
| 小窗默认今天覆盖明天 | 解析明确日期覆盖scope默认；分类仍保留 |
| 不支持的日期词被部分采用 | 明早/昨天/周末/下个月/天后及英文am/pm等不支持词整体要求手选，不把尾部时刻误当今天 |
| DST/过去/多时间 | 保守choice；原文保留，手工修正 |
| UI预览被当可信数据 | 保存端重新解析/校验；无QML UTC直写 |
| 偏好/解析数据进同步 | 无schema/协议扩展，本地偏好/草稿不上传 |
| 预览到提交之间过期 | 可见建议每秒轻量刷新，提交重校验遇到歧义/过期保留草稿，不能静默变为无提醒任务 |
| 半创建/失败丢输入 | 原子事务＋成功后清空 |

准入：设计无待决高风险，2026-11-06自审通过，按用户既有授权进入实施。以上验证是计划，未标PASS。

## 12 实施顺序

规范与自审→纯解析器/测试→服务事务/测试→主窗/小窗/设置→用户链回归→文档。当前版本2.0.2，不修改发行tag或未经请求发布。

选型依据：[Qt6.10 QDateTime](https://doc.qt.io/qt-6.10/qdatetime.html)的TransitionResolution::Reject与[TextInput](https://doc.qt.io/qt-6.10/qml-qtquick-textinput.html)的inputMethodComposing；文档随Qt6.10补丁更新，工程固定6.10.2，以本机构建/测试为准。HIG交互依据见[项目UI基准](ui-baseline.md)和[输入设计](https://developer.apple.com/design/human-interface-guidelines/entering-data)。

错误接口：timeInputNeedsChoice为识别歧义/失效（ValidationError）；timeOverrideInvalid为手工字段/白名单无效；timeReminderInvalid为过去/缺失/重复墙钟提醒。全部失败保持原始输入，None才按普通标题保存，ignore显式恢复普通标题路径。
