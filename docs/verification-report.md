# 1.0.0-rc.1 Windows候选版验证（2026-10-22）

环境：Windows11 x64、Intel Core i7-13700HX、Qt6.10.2、MinGW13.1；源码与自动测试只使用合成数据及QTemporaryDir。用户确认先交付Windows候选版。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| 最终Windows Debug build / CTest | PASS，13/13，14.71秒 | 新增启动配置隔离测试及批量/归档/迁移/筛选回归 |
| 最终Windows Release build / CTest | PASS，13/13，14.40秒 | 同上 |
| daymark_qmllint | PASS，无警告 | QML静态检查 |
| Windows原生合成smoke | PASS，退出0，2窗口，warnings=false | 宽窄共享草稿、批量提交真实SQLite、20项列表、各视图位置恢复、折叠恢复、200%工具区/列表/导航、Floating/Mini切换 |
| 合成PNG视觉审查 | PASS（所检场景） | 1120/880/1440 DIP、浅深色、200%详情/设置/列表/导航、批量对话框、Floating/Mini；截图只存忽略的build/v1-screens |
| 批量事务 | PASS | 后续任务版本冲突、后续SQL触发器注入失败均回滚先前写入；成功后最新版本与模型一致 |
| schema1升级/归档/导入 | PASS | 迁移前快照、备份失败阻止升级、任务保留、归档/恢复、旧JSON缺少archived默认0、schema2往返 |
| 备份周期与启动设置 | PASS（隔离配置） | 周期7天跳过近期快照、每日生成新快照；启动登记仅测临时INI读写/引用/失败，不修改真实Run项 |
| 10k混合数据层Release基准 | PASS，加载155ms、搜索2ms、月索引11ms | 20领域、200项目、50标签、30k清单、30k标签关联、30%完成；不是进程冷启动/全UI帧率 |
| 文档链接与diff检查 | PASS，39份Markdown，无空白错误 | 本地链接，不替代外部平台验收 |

首次批量测试曾错误假设持续关注Today任务可直接设未来开始，后将底层事务测试改为Anytime数据，另加workspace测试验证批量未来开始同时取消持续关注。编译检查发现的QML处理器分隔和类型注册问题已修复。原生截图发现窄窗200%工具区/侧栏挤占或遮蔽入口后，改成可滚动区域并增加尺寸与恢复回归；不隐藏这些初次失败。

本轮未运行macOS构建、真实系统登录启动、真实输入法/读屏/高对比、物理多屏/睡眠/时区、8h或独立干净机；均保持UNVERIFIED。Windows事件适配只实现了广播入口，不能把代码存在或方法调用当作真实广播验收。候选包的SDK隔离部署结论由包内BUILD-INFO.json及下方交付记录承载。

---

# UI-03A响应式详情验证（2026-10-22，历史切片）

本轮源码在v0.1本地交付后新增响应式详情；下方原v0.1包记录保持历史范围。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| Windows Debug build / CTest | PASS，12/12，9.60秒 | QTemporaryDir隔离数据库 |
| Windows Release build / CTest | PASS，12/12，9.41秒 | 同上 |
| daymark_qmllint | PASS，无警告 | QML静态检查 |
| 草稿/导航集成smoke | PASS | 宽窄往返保留草稿、任务/视图切换取消与放弃、保存后导航、保存后完成使用最新rowVersion；既有非法日期失败保留仍通过 |
| Windows原生后端smoke及截图 | PASS，退出0，2窗口，warnings=false | 1120×720、880×580、1440×900 DIP，默认和200%文字；原生渲染，动作由QML方法触发 |

截图为合成数据，保存在忽略的build/responsive-screens；检查了宽窗列表与详情预留空间、窄窗覆盖、按钮换行及可滚动内容。真实鼠标/焦点、中文输入法、Narrator/VoiceOver、macOS、多屏仍UNVERIFIED；不以程序触发信号替代人工验收。没有改schema或历史migration。

首次部署验证发现隐藏窗口未渲染时读取了旧布局尺寸；smoke现先触发渲染再断言布局，最终Debug/Release回归通过。另一次部署在固定500ms取模块时尚未加载运行库；打包检查改为最多5秒的有界等待，并在失败时回收探针进程。后续隔离SDK环境的部署smoke退出0、2窗口、无QML警告；未把上述初次失败当作成功。

---

# v0.1本地版验证（2026-10-22，历史交付）

环境：Windows11 x64，Qt6.10.2，MinGW13.1，CMake3.29.2，Ninja1.12。以下为v0.1原始本地包，后文保留更早历史证据。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| 最终Debug build / CTest | PASS，12/12，12.01秒 | 临时SQLite/服务/模型/单实例/窗口/QML |
| 最终Release build / CTest | PASS，12/12，8.04秒 | 同上 |
| Debug daymark_qmllint | PASS，无警告 | 属性/类型/布局静态检查 |
| Windows原生窗口测试 | PASS，8项含init/cleanup，0失败/错误，1后端特定跳过 | Qt状态；不证明物理拖动/系统层级 |
| 原生扩展QML与合成截图 | PASS，退出0，2窗口，warnings=false | 月/周/详情/组织/规则/设置/数据/日期/深色/200%详情 |
| 10k Release数据层基准 | PASS，加载38ms、搜索1ms、月索引11ms | 当前机器、真实临时SQLite；不是全UI帧率或干净机指标 |
| 文案对比计算 | 浅色正文14.97、次级5.52、选中12.96、强调6.24、危险6.54；深色14.50/8.09/10.29/7.95/9.48 | 相邻content/selected表面；不覆盖所有控件或透明桌面 |

用例覆盖TC-001～020主要数据流程、TC-021～025日历边界、TC-026同源模型、TC-030单实例、TC-031～039重复提醒与恢复、TC-045数据层基准。真实桌面TC-027～029/041、8h TC-042及系统事件TC-043/044不能因纯函数/状态测试而标PASS。版本化SQL错误、项目迁出失败注入、无效JSON零修改、重复ID/日期/外键、提醒认领一次和重启持久化均由真实临时库验证。

源码不含用户数据库、私人任务、原始机器日志或SDK。截图/log只在忽略的build目录。Qt许可服务版本诊断仍存在，未绕过检查；可选Vulkan头缺失，本轮不验证该后端。完整未验证项见平台矩阵与发行说明。用户已要求先交付本地，不推送或创建GitHub Release。

最终本地部署PASS：去除Qt/MinGW PATH及Qt/QML插件搜索环境，检查加载模块均来自包目录；原生smoke退出0、双窗口、无QML警告。包1443个文件，ZIP 44,074,547字节，SHA256为4111e1cefb9f2bf3ce1855120c9dcb1ba6647b9205bec41fe31437e22e703f24。包名Daymark-0.1.0-windows-x64-local-20261022-192938.zip。ZIP检查未发现数据库、私钥或原始日志。本机路径隔离不等于独立干净机验收。

---

## 历史记录（旧功能范围，仅保留证据）

# 验证记录

## UX-RESEARCH-01 UI/UX目标（2026-10-22）

本轮只改文档与开发约束，不改运行源码或数据库。读取Cultured Code官方产品/支持页面、Apple HIG及其页面提供的同域Markdown正文；来源、年代和访问日期见design/ui-research.md。没有安装Things或测量其像素，未获取专有产品资产；没有声称已验证实际Things客户端交互或效率。

| 检查 | 实际结果 | 限制 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | 当前Windows Debug基线，无运行源码变更 |
| scripts/test-windows.ps1 | PASS，6/6，3.91秒 | 既有基本功能回归，不验证新UI目标 |
| scripts/check-docs.ps1 | PASS，30份Markdown本地链接有效 | 外部来源另外阅读，不由脚本验证 |
| 官方资料→目标→UX验收追踪 | PASS（文档审查） | 来源事实与Daymark自主token/阈值已区分，历史截图已标注 |

新UI目标是待实施设计：颜色候选尚未逐对实际对比计算；布局断点、文字缩放、读屏、真实用户动作预算与双平台实现均待后续UI-01～06验证。本轮不将文档完成当作界面达标，不重复声称macOS或窗口人工测试通过。

## BASIC-01 基础任务闭环（2026-10-22）

Windows11 x64、Qt6.10.2、GCC13.1，使用本机已有Kit，无新依赖。当前功能范围见 [设计](design/basic-tasks.md) 和 [任务记录](tasks/BASIC-01.md)，不是完整V1验收。

| 命令/检查 | 实际结果 | 边界 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | Debug源码、SQL资源和QML编译链接 |
| scripts/test-windows.ps1 | PASS，6/6，4.89秒 | SQLite、Task服务、异步模型、进程单实例、窗口、完整QML流程 |
| scripts/build-windows.ps1 -Preset windows-release | PASS | Release编译链接 |
| scripts/test-windows.ps1 -Preset windows-release | PASS，6/6，4.05秒 | 同上，非性能基准 |
| cmake --build --preset windows-debug --target daymark_qmllint | PASS，无警告 | Bound delegate及命名空间模型类型已明确 |
| scripts/test-window-native.ps1 -Preset windows-release | PASS | Windows窗口Qt状态和完整合成任务QML流程；不代表实际鼠标/焦点验收 |
| daymark --smoke-test --screenshot <build内PNG> | PASS，退出0，两窗口，无QML警告 | QTemporaryDir、真实Windows后端、合成数据 |
| 主窗/窄窗详情PNG视觉检查 | PASS | 1120×720及880×580 DIP；当前150%渲染下主操作、中文和日期错误提示无截断 |
| 追加WAL非空断言后，分别构建并运行Debug/Release task_service | PASS，1/1及1/1 | 确认未checkpoint内容被快照保留 |
| scripts/check-docs.ps1及共享文件审查 | PASS，26份Markdown、33个源/测试/脚本无尾部空白 | 未检出本机绝对路径/用户目录/许可绕过变量，构建截图与日志被Git忽略 |

QML流程实际触发快速输入accepted和详情保存：新建同步Today→非法日期拒绝且草稿保留→成功编辑→完成至Logbook→恢复→软删/撤销→脏草稿阻止主窗关闭→取消离开保留编辑→明确放弃而不改数据库。自动测试不使用生产AppData目录。

服务测试覆盖20条连续录入、512个补充平面Unicode标量边界、三态patch、日期/版本冲突零修改、未来focus/Someday优先级、完成/取消/恢复、项目JOIN隐藏恢复、只读写入失败、过新/未知库不覆盖、WAL快照内容一致。WAL快照用例关闭自动checkpoint并确认-wal非空，快照回读逐字段一致；备份失败不改任务。

异步模型以QAbstractItemModelTester验证增量信号、两个模型同步、失败不改缓存、退出等待已接受写入。单实例用真实第二进程和含空格临时目录验证激活协议，第二实例不创建任务库。

修复记录：QML可默认构造类型的final限制导致初编失败，去掉注册类型final后通过；Shortcut多绑定告警改为sequences，模型指针使用完整命名空间，delegate采用Bound；Windows offscreen字体目录来自本机Windows Fonts，仅用于测试，无下载字体。smoke stderr诊断可读，未吞启动错误。

截图和原始日志仅保留在忽略的build目录，截图均为合成数据。未验证：真实OS鼠标拖动/窗口层级/焦点、完整多屏DPI、系统午夜/睡眠事件、macOS、10k性能、8h稳定性和干净机部署。构建仍输出既有许可服务/可选Vulkan诊断，未修改系统配置。未提交或推送Git。

## W-01 窗口控制（2026-10-22）

环境沿用Windows11 x64 / Qt6.10.2 / MinGW13.1。实现和自动验证通过；真实拖动、跨应用窗口遮挡、焦点、DPI和macOS尚未人工验证，任务保持VERIFY。

| 命令 | 最终结果 | 范围 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | Debug增量构建 |
| scripts/test-windows.ps1 | PASS，3/3，1.46秒 | SQLite基线、widget_window、QML启动 |
| scripts/build-windows.ps1 -Preset windows-release | PASS | Release增量构建 |
| scripts/test-windows.ps1 -Preset windows-release | PASS，3/3，0.66秒 | 同上；不是性能基准 |
| cmake --build --preset windows-debug --target daymark_qmllint | PASS | QML静态类型/属性检查 |
| scripts/test-window-native.ps1 -Preset windows-release | PASS，QWindow失败0、错误0、跳过1，QML启动退出0 | 4个行为用例通过；另有init/cleanup；仅offscreen守卫在windows后端跳过 |
| Windows原生QML启动探针 | PASS，退出0 | 真实后端创建双窗口，无QML警告 |
| scripts/check-docs.ps1 | PASS，22份Markdown | 本地链接 |

widget_window验证：模式往返保留尺寸位置、隐藏时设置不误显示、flags保留Tool/Frameless、解除transient owner、可见性/close同步、失败不提交新模式/置顶、恢复模式失败仍可重试、销毁后安全失败、成功无操作清除旧错误。失败注入只验证命令契约，不当作OS能力成功；Qt接受置顶flag也不代表桌面遮挡测试通过。

初次编译因QML自动注册生成的头文件查找路径不足失败，补齐src/models包含路径后复测成功。原生测试命令初次用了不支持的junit格式名称，按实际Qt Test支持改为junitxml后通过；可复现入口已加入scripts/test-window-native.ps1。原始日志与JUnit（可能包含设备名）仅在忽略的build目录，不提交。

人工步骤见 [W-01任务](tasks/W-01.md)，平台边界见 [矩阵](platform-test-matrix.md)。无任务库、迁移或用户数据写入。

## 2026-10-22 工程基线

环境见 [environment](environment.md)，执行范围F-01/F-02；没有任务Repository、MigrationManager、托盘或真实穿透功能验收。

| 命令/检查 | 实际结果 | 结论边界 |
|---|---|---|
| scripts/build-windows.ps1 | PASS；Debug配置与29步构建成功 | Qt6.10.2/GCC13.1 |
| scripts/test-windows.ps1 | PASS；2/2 CTest，总耗时4.21秒 | 见下方测试职责 |
| scripts/build-windows.ps1 -Preset windows-release | PASS；Release配置与29步构建成功 | 未部署运行库 |
| scripts/test-windows.ps1 -Preset windows-release | PASS；2/2 CTest，总耗时0.60秒 | 非性能基准 |
| Release daymark --smoke-test，清除QT_QPA_PLATFORM后启动 | PASS；进程退出码0 | 原生启动探针，无视觉/交互验收 |
| scripts/check-docs.ps1 | PASS；21份Markdown本地链接有效 | 包含根目录和docs，不验证外部网站 |
| 共享源码/脚本/文档路径与隐私审查 | PASS；未检出本机盘符绝对路径、用户目录和许可绕过变量 | 构建原始日志仅留忽略的build目录 |

sqlite_runtime：QTemporaryDir隔离；真实QSQLITE文件库切WAL，写入中文任务样本文本，事务内写第二条并回滚，关闭重开后仅第一条保留。它验证Kit能力，不验证尚未交付的应用schema、备份或Repository。

qml_startup：真实应用加载Daymark模块，offscreen运行，检查恰好两个可见且尺寸非零的QQuickWindow，无QQmlEngine警告；250ms后自动退出。正常业务启动没有这一退出计时器；无业务数据库访问。

构建诊断：缺WrapVulkanHeaders；moc打印许可服务版本不匹配信息，但命令仍成功、产物链接成功且测试通过。未注册/修改本机许可服务，未设置绕过检查变量。Kit许可与分发核查保持待办。

原始日志保留在本机构建目录（不提交），当前记录仅保留可共享结果。macOS、透明视觉、DPI、穿透恢复、窗口焦点、通知、10k性能和部署均UNVERIFIED，详见 [平台矩阵](platform-test-matrix.md)。
