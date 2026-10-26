# Daymark 实施进度

### UX-43：提醒事项式列表精修（2026-10-26～27，IMPLEMENTED；关联AUTO-PASS / 完整门禁仍FAIL）

按用户截图及[列表设计](design/reminders-list-refinement.md)统一浏览/编辑文字轴，移除重复关系图标，以缩进和展开箭头表达父子；备注、时间、归属分层，按钮按悬停/焦点呈现，保留智能范围关系入口与原草稿/右键/拖放。基线bb35a03、工作区干净；1.0.2双构建、关联PASS，完整21/24含三个已知失败。计划先共享行/快捷编辑最小改动，再独立探针与浅深/200%截图、构建/测试、隐私审查后提交并推送main；不自动发布tag或二进制。

已实现：TaskRowSummary分文件绘制标题、单行备注预览和辅助信息；TaskRow保留手势/菜单/完成/选择，原行编辑使用相同完成圆、标题、备注与分隔线轴。移除父子分支图标列和展开时0/N；普通树24 DIP缩进，智能范围保留父名/子项数量入口。收起或全局隐藏子项仍有数量入口，空树结果不占展开槽；模型在既有投影遍历聚合hasHierarchy，不增加查询或QML扫描。日期不重复今天，置顶/特别关注用小标记及可访问名称，信息/拖动按钮按悬停/焦点出现。原行无大卡底色，无修改不显示保存勾；失焦保存、无效草稿保留、右键/多选/详情迁移/拖放契约不变。

验证：Qt6.10.2 macOS arm64 Debug/Release构建及两目标lint PASS（ux43-{debug,release}-final-build.log、ux43-{debug,release}-lint.log，无lint诊断；编译保留Qt生成代码unused参数警告）。完整Debug21/24、117.61秒（ux43-debug-full-tests.log），Release21/24、105.69秒（ux43-release-full-tests.log）；沿用qml_startup/late watchdog phase430日历详情断言、desktop_surfaces stage9完成冲突/布局三项旧失败，不提高45秒预算或删断言。最终共享模型、UX34工作区及三项独立UI关联Debug5/5、27.62秒，Release5/5、24.80秒（ux43-{debug,release}-final-tests.log）。

task_list_interaction扩展至58阶段，保留原选择/菜单/详情测试并补充父/子列偏移、备注预览、浏览/编辑同列、离开保存、非法标题恢复、收起/全局隐藏子项入口、搜索去展开槽及恢复、浅深/200%/高对比。最终离屏抓图ux43-final.png.ux37-list-refined-{light,inline,dark-200,inline-200,invalid-title,contrast-200,flat}.png已复看，无QML warning；离屏插件提示不作为原生实测。Windows/macOS物理输入、VoiceOver/Narrator及真实DPI仍UNVERIFIED。

源码守卫、12项发布守卫、103份Markdown链接、四种主题/对比颜色检查及diff检查PASS。更新产品哲学、用户指南和当前UI契约覆盖旧关系图标方案；版本仍1.0.2，本轮为后续源码修订。未改用户数据库或运行中的旧窗口；只提交代码/测试/文档，截图、日志、缓存和Qt运行库均保持忽略。完成后按本轮授权非强制推送main，远端结果在回复中核对。

### UX-42 / 1.0.2：今日年视图、桌面层级与关于（2026-10-26，IMPLEMENTED；关联AUTO-PASS / 完整门禁仍FAIL）

按[设计](design/desktop-layer-and-year-focus.md)处理三项新要求：年视图进入定位今天；独立日历/小窗/Mini增加与置顶互斥的固定桌面模式；收集箱改紫色。先共用平台接口/原生层级、失败回滚与设置测试，再接菜单/设置及日期入口，执行双构建/测试/lint与隔离原生验证。基线当前1.0.1 macOS Debug构建成功、21/24测试通过，已知qml_startup、desktop_surfaces、late watchdog失败见上轮日志current-macos-debug-build-test.log；不以新功能通过抹去原门禁。风险为原生句柄重建、激活后的Z序、恢复通路及跨平台可测性。不触碰用户数据库或正在运行的旧窗口。

用户追加关于及1.0.2提交：设置新增独立SettingsAboutPage，版本读取应用元数据，QQ交流群1057825113、反馈邮箱409962@qq.com为只读可复制内容，复制即时反馈；品牌说明从通用迁入，帮助/备份原入口保留。CMake两处版本、README及独立[1.0.2说明](release-notes-v1.0.2.md)同步，无schema变更。

窗口实现集中daymark_window_platform，macOS公开NSWindow.level及桌面图标层、Windows公开SetWindowPos及本进程Z序保护分别实现；Qt端负责互斥、几何/显隐保持及原生句柄重建后恢复，WidgetViewModel与TaskWorkspaceWindow接线，QML只呈现菜单/开关。Mini就近菜单可切换；设置文件写失败回滚日历层级。固定与锁定拖动独立，穿透仍保留托盘/主窗恢复通路。

验证：Qt6.10.2 macOS arm64 Debug/Release构建及两目标lint PASS（ux42-{debug,release}-build相关日志、ux42-{debug,release}-lint.log）；完整Debug21/24、113.45秒（ux42-debug-full-tests.log），Release21/24、108.67秒（ux42-release-build-test.log）。同三项旧失败：qml_startup/late watchdog在phase430的日历详情断言，desktop_surfaces在stage9完成冲突/布局断言；未改45秒预算或删断言。新增关联widget_window、ux34_workspace、calendar_interaction通过；平台代码可读性整理及键盘复制后最终Debug3/3、6.10秒（ux42-final-focused-tests.log），Release3/3、5.64秒（ux42-release-final-focused-tests.log）。

独立日历探针扩展为67阶段，覆盖年入口今日选择及月份钻取、独立日历隔离、关于版本/文本/复制、200%深色、Mini实际菜单点选、设置层级同步及浅深收集箱色差；离屏截图已复看ux42-capture.png.calendar-about-{light,dark-200}.png，无QML warning，离屏插件自身的尺寸提示不作为原生实测。语义色审计增加紫色徽标前景配对，四种主题/对比组合通过。macOS Cocoa原生widget两个用例及calendar两个用例分别4/4 PASS（含init/cleanup，ux42-native-{window,calendar}-tests.log），验证实际level、激活/显隐/Mini/穿透及保存失败回滚；Windows构建/原生、macOS多Spaces/全屏/显示桌面及物理输入/读屏完整矩阵UNVERIFIED。

过程失败保留：首次新增native matcher错误地读取offscreen伪句柄导致崩溃，已在原生入口核对真实后端并保留“离屏不等于原生”断言；原年视图探针仍点击2028年旧月份，已按本轮“进入即今天”更新日期fixture并保留单击/双击行为断言。发布守卫12项通过；不提交缓存、截图、日志、Qt库或用户数据库。本轮仅按用户要求提交1.0.2代码，不自动推送、打公共标签或创建Release。

### RELEASE-1.0.1 源码提交及发布规则（2026-10-26，SOURCE_PUBLISHED；完整质量门禁仍FAIL）

用户授权提交GitHub、制定发布规则并建立版本tag。只读核对发现远端已有v1.0.0→23cbc0a；用户明确选择保留，最新版本改为1.0.1/v1.0.1。新增[发布规则](release-policy.md)、独立[发行说明](release-notes-v1.0.1.md)，CMake两处及README同步；不改旧tag/Release或旧发行说明。

发布守卫检查完整稳定版本、两处版本/说明精确一致、已跟踪源码中的产物/常见密钥/本机路径；错误只输出文件名。真实发布另验annotated tag、checkout SHA及origin/main可达性；既有Release仅允许完全一致的幂等重试，不修改不一致记录。11项标准库单测覆盖patch说明与旧1.0说明兼容、非法tag/错版本/错说明、运行库/库文件/缓存/秘密的拒绝及不回显、untracked排除、注释/轻量标签、checkout和main可达性。分支CI单独执行一次源码守卫，tag工作流串行并使用短期最小权限token，不上传二进制。

本轮重新验证1.0.1：Release构建和两目标lint PASS（`release-1.0.1-{build,lint}.log`，无lint诊断）；11项发布守卫PASS、3.806秒（`release-1.0.1-guards.log`）；完整Release23/25、144.09秒（`release-1.0.1-tests.log`）。qml_startup在phase412、late watchdog在phase404超时，继续保留FAIL，未改45秒预算/断言；其余23项含核心/存储/集成/性能/包/五项独立UI均PASS。既有最终Debug和Windows原生67阶段证据见UX-41；macOS及其他人工平台门禁UNVERIFIED。已在1.0.1发行说明公开以上边界，源码发布不冒充二进制RELEASE-READY。

审查并暂存UX-38～41的实现/测试/文档及发布规则；源码版本/隐私守卫、101份Markdown链接、cached diff及既有语义颜色检查PASS。排除无关未跟踪架构草案，缓存/本机构建/日志/库/Qt SDK不进入索引；项目身份及UTC提交元数据沿用，不改公共历史、不触及个人数据库。下一步非强制原子推送main与单个v1.0.1，回读分支、tag及无资产源码Release后另记发布证据。

发布证据（同日追加）：应用源码提交`7a6a804cc021181bb92ad520559263a88ce86fa0`，带注释v1.0.1对象`8259bceca6944ae5646366b79d9cdd895ffed21e`；main与新tag已非强制原子推送。远端v1.0.0对象仍为`d2b0e3ae273eeb32395bd08d7c905955daf7439b`、commit仍23cbc0a，均未覆盖。项目作者/标签署名均为Daymark Contributors及项目邮箱，UTC元数据。

原始[标签发布作业](https://github.com/downdone/daymark/actions/runs/40327015059)在发布步骤失败，单测/元数据已通过；未登录日志不可读，未冒称已定位全部原始异常。保留tag，从main增加明确的tag+40位commit+source_only恢复请求（流程提交6f05095），工具与不可变源码分别checkout；[恢复作业40328034427](https://github.com/downdone/daymark/actions/runs/40328034427)成功。[Daymark1.0.1 Release](https://github.com/downdone/daymark/releases/tag/v1.0.1)已真实公开且Latest，指向7a6a804；正文包含23/25、旧超时、人工/macOS边界及仅源码说明。发布脚本成功响应要求assets为空，页面只有自动生成的源码归档，没有Qt/MinGW/安装包或个人数据。

后续工具修正明确处理webhook的annotated tag对象SHA、显式获取main、发布错误安全注释及共用串行队列；增加对象SHA解析回归，最终发布守卫12项PASS、3.934秒（`release-final-guards.log`），不为修复流水线改动公开应用版本tag。原始失败记录保留，根因未凭日志完全确认。发布规则/恢复入口与本记录继续提交到main；桌面远程CI独立于源码发布，不把Release成功当全部测试成功。无关架构草案仍未提交。

远端CI回读补充：源码提交7a6a804的[Build and test作业40327015406](https://github.com/downdone/daymark/actions/runs/40327015406)已结束为FAIL；source-policy通过，Windows/macOS桌面job分别退出1/8，未登录日志不可读，未将远端失败擅自归因于本机相同超时。Release仅有GitHub自动生成的zip/tar.gz源码入口；主分支发布工具修正及证据已推送，不改v1.0.1快照。正式二进制仍不具备全绿门禁。

### UX-41 主框架、字标与侧栏（2026-10-26，IMPLEMENTED；相关最终PASS / 完整门禁FAIL）

用户截图要求去菜单、品牌字标和侧栏精修；先定义[完整设计/映射/状态及验收](design/sidebar-and-wordmark.md)，追加FR-039。Windows删除窗口内文件/视图/帮助及占位，保留原生系统标题栏、工作区操作及共享快捷键；macOS保持屏幕级原生菜单。设置仍为外观/组件/通用三类，通用增加版本、本地优先说明、数据与备份/使用提示接线；退出保留，切换先关闭设置并走既有保护路径。

新增原创BrandWordmark，daymark小写几何笔画、固定字距/基线、单层a和圆弧d/m；QML独立矢量路径，无外部字体或小米资产。侧栏与设置复用，普通界面仍系统字体。侧栏标题/开关居中、56起卡片8～10内边距、12正常名称/选中半粗、16数字、小语义图形及浅选中底；分组箭头贴近标题、操作统一命中、分类关系槽收紧。默认两列、150～200%单列，字号不缩小，脚部固定、正文溢出滚动。旧分类/标签、折叠/拖动、键盘、子项及草稿不变。

验证：Qt6.10.2最终Release/Debug构建和两目标lint PASS，无lint诊断（`ux41-verified-{release,debug}-{build,lint}.log`）。整轮完整Release23/25、145.96秒（`ux41-final-release-tests.log`），旧qml_startup/watchdog均在phase389触发45秒门禁；未延长预算或删除断言。最终字标兼容修正后相关Release五项5/5、38.94秒（`ux41-last-release-tests.log`）；最后加Bound约束后desktop_surfaces Release PASS、11.67秒（`ux41-verified-release-tests.log`），Debug重查PASS、12.03秒（`ux41-debug-recheck.log`）。此前关联Debug五项5/5、39.96秒；完整门禁仍FAIL。

复用desktop_surfaces临时库扩为67阶段，不加CTest入口：菜单确无占高、六范围往返、任务分组折叠/恢复、窄轨恢复、200%单列/名称不截断/脚部可达、设置实际点击到帮助及数据；字标检查画幅/有限尺寸以及实际窗口像素，验证浅深切换/恢复。最终Release Windows窗口后端退出0，三个可见窗口、无QML warning（`ux41-verified-native.log`）。复看`build/ux41-captures/final.png.desktop-{sidebar-light,sidebar-dark-200,settings-support}.png`；品牌颜色/字形及设置入口均可见。此为Qt合成输入/自身窗口抓图，macOS实机、物理键鼠/IME/读屏、P95与干净机仍UNVERIFIED。

过程FAIL保留：CurveRenderer在本机主窗未绘出，GeometryRenderer后浅色可见但换深色残留旧描边；小型字标在配色变化时重建绘制节点并使用独立抗锯齿层，实际像素断言后通过，不只检查对象存在。尺寸绑定改为显式品牌画幅；内嵌Component加Bound使lint无外层ID诊断。最后Debug单项一次在既有stage13完成反馈判定失败，未改产品/断言，隔离重查PASS；该时序风险仍保留，不将首次FAIL抹掉或称绝对稳定。99份文档链接、语义颜色对比、diff/新改动隐私检查PASS。保留用户正在运行的旧进程，仅在build目录改名保留占用exe以完成新链接；用户需退出旧进程后打开新版。未触及个人数据库，保留所有既有未提交改动及未跟踪架构草案，未提交/推送或上传二进制。

### UX-40 提示/菜单及日历拖动修复（2026-10-26，IMPLEMENTED；相关AUTO-PASS / 完整门禁FAIL）

用户追加菜单字级、tooltip准入、年视图高亮、连续拖选及新建面板拖动/外部保存。先定义[完整规则与日历契约](design/help-tags-and-menu-type.md)，按P-13/14、FR-037/038、UX-01/09～13实施：完整可见文字和通用展开/关闭/完成不重复提示；必要图标说明动作，截断内容补全原文；600ms/5000ms、辅助12、正文13正常字重；键盘焦点不触发，菜单跨窗/拖动抑制，保留多行备注预览和受限拖放原因。已检查全部ToolTip调用与共享图标类，保留可访问名称；分类/子任务操作提示简短，不复述任务标题。菜单显式统一字体、勾选/箭头及文字列；宽度同时使用实际Text自然宽度，中文长项100/200%不意外换行。

日历空白/任务/端点拖动保留指针，避免外层Flickable偷取手势；六帧连续拖动后精确生成一个90分钟草稿。年视图只绘制本月日号，非本月占位不可选/聚焦，今日与选择分色；相同投影不重复重建代理。新建面板标题栏可拖动并限制于窗口内；外部点击拦截底层，空标题关闭不插入，有效草稿原子保存后关闭，日期错误/写入失败保留输入。内部选择器和取消/离开保存门保留。未改schema、查询或数据语义。

验证：Qt6.10.2最终Release/Debug构建及两目标QML lint PASS（`ux40-final4-{release,debug}-{build,lint}.log`，lint无诊断）。完整Release23/25、142.50秒（`ux40-final3-release-tests.log`）；qml_startup在phase414、qml_watchdog_late在phase411触发既有45秒超时，未延长预算/删阶段。最终相关Debug五项5/5、36.52秒（`ux40-final3-debug-tests.log`）；最后仅缩短分类/子任务提示文字后，Release三项3/3、24.54秒（`ux40-final4-release-tests.log`）。所有核心、存储、集成、性能及包校验在完整运行中PASS；完整门禁仍FAIL。

复用calendar_interaction临时库58阶段和desktop_surfaces临时库48阶段，不增加重复CTest入口：连续指针吸附/范围、真实落库schedule、标题移动、大字可滚动、外部点击无底层动作、非法输入零插入、年视图代理与高亮；mini/通用展开无提示、必要置顶提示、短分类不提示/长分类全文、焦点不触发、跨窗菜单抑制、菜单实际字级/单行。最终Windows窗口后端两组均退出0、各三个可见窗口、无QML warning（`ux40-{calendar-interaction,desktop-surfaces}-native.log`）。复看`build/ux40-captures/`中的拖动草稿、年视图和原生菜单自身窗口100/200%截图。此为Qt合成输入/自身抓图，物理鼠标/触控板、IME/读屏、macOS实机及P95仍UNVERIFIED。

过程FAIL已修复：标题区fillHeight误吞表单高度，改为标题控件行高并保留200%滚动断言；原生窗口分类截断需等待真实布局，探针抓取该窗口后断言，不跳过检查；共享tooltip验证加入所属控件并发送跨窗Leave，避免合成指针仍停留在另一个窗口；菜单截图改抓Popup.Window自身，实际发现中文换行后修复度量。初次链接被运行中的旧程序占用，进程结束后重建PASS。98份Markdown链接、语义颜色对比、diff与变更隐私检查PASS。保留UX-38/39与未跟踪架构草案；新UI未提交/推送、未上传二进制。最新本机程序`build/windows-release/bin/daymark.exe`。

### UX-39 macOS日历操作对齐（2026-10-26，IMPLEMENTED；核心交互AUTO-PASS / 完整门禁FAIL）

用户要求操作和精细度对齐macOS Calendar。先核对Apple日期导航、键盘、编辑及Qt的macOS键位映射，再形成[完整差距/契约矩阵](design/macos-calendar-interaction.md)、FR-038与ADR-023，覆盖UX-35/FR-036单击空白即新建旧约定。主窗与独立日历共用；任务、C++时间命令、CAS/DST、树联动、原子新建和离开前保存门不变，无schema变化。

本轮已实现：月格/日期任务/时间块单击选择、双击或Return编辑；月格数字双击进日，空白双击新建，更多仍开议程；小时空白单击只定位，双击/Return或拖选新建；周日期与年月份/日期分离选择和双击钻取。Command/Ctrl+1～4在日历上下文切日周月年，New保留范围/日期/时刻；今天、跳日期、前后、编辑与安全删除有键盘通路，文本Delete保留编辑含义。关闭恢复日历焦点，重复New不覆盖活跃草稿，范围/日期/筛选变化清除旧选择，时间块消失时清除目标。Mac便携Ctrl映射Command，物理Control上下文使用Meta；本地macOS菜单按当前独立日历窗口路由，Windows增加日期菜单。

视觉与精细行为：日期数字和事项分别表达选择，取消整格大蓝背景；普通时间块淡底/细色条，选中填色，端点与详情按选中呈现。周头星期/数字分层，窄大字缩短星期而保留可访问全文，数字去除过量内边距且断言不省略；日期任务栏按空间缩短，不与26等数字相撞。月份导航区分显式定位与手动滚动，闰日跳转后立即前后不会因可见周归属跳过一月；字号缩放保留视口真实分钟，不把08:00视口变成04:00。

验证：Qt6.10.2 Release/Debug最终构建及两目标QML lint PASS（`ux39-final7-{release,debug}-{build,lint}.log`，lint无诊断）。完整Release23/25、152.98秒（`ux39-final2-release-tests.log`），qml_startup与晚阶段watchdog仍在phase388超时；未提高45秒预算或删除阶段。后续选择/窄周样板关联Release5项PASS（`ux39-final2-ui-tests.log`，31.18秒），Debug5项PASS（`ux39-final-debug-tests.log`，56.29秒）；最终字号时刻锚点/头部修正Debug相关三项3/3、17.65秒（`ux39-final7-debug-tests.log`）。独立`calendar_interaction`临时库38阶段，CTest 30秒；完整入口25、fast仍17。旧连续日历的单击新建指针输入改为真实双击，原取消/错误保留/事务/字段断言保留，历史末段任务双击编辑也更新输入。

最终Windows窗口后端38阶段退出0、三个可见窗口、无QML warning（`ux39-final7-native.log`）。已复看`build/ux39-captures/final.png*`的月选择、新建及480宽200%深色周：日期完整、类型栏不重叠、视口保持08:00。此为Qt合成指针/键盘及自身窗口抓图，macOS实机、物理触控板、IME/读屏、真实首帧/P95仍UNVERIFIED。日历多选、剪贴板、系列批量修改、外部账户等差距在设计中明确，不能据本片宣称Apple全产品等价。

过程FAIL保留：端点改为选中后出现，拖入成功先选中其身份，旧端点断言恢复PASS；原生切周前未布局就点击到错误日期，探针按实际布局/目标等待，不以固定睡眠冒充响应；月格回收/销毁时新增颜色绑定出现root为空，增加绑定生命周期保护后独立probe无警告。检查新建取消以服务投影/最终全量计数为准，不误把日历时隐藏的列表投影当全量；跨独立窗口的合成输入明确激活来源并等待真实可见目标。97份Markdown链接、diff与变更隐私检查PASS；保留UX-38改动和未跟踪架构草案，未提交/推送新改动或上传二进制。最新程序`build/windows-release/bin/daymark.exe`。

### UX-38 全窗口与桌面组件精修（2026-10-26，IMPLEMENTED；关联AUTO-PASS / 旧完整门禁FAIL）

按用户要求从产品哲学、Apple HIG及macOS日历/提醒事项机制审视导航、任务列表、就近详情/新增、日历、设置/数据、小窗与迷你条；问题、保留理由、视觉/交互关系与验收见[全窗口精修](design/desktop-surfaces-polish.md)。主窗负责整理与安排，小窗负责今日扫读/完成，迷你条负责摘要/展开。保留同一QueryService、任务模型、草稿与恢复通路，不创建新的筛选语义、数据库或窗口设置副本。

小窗采用统一字级、日期/数量标题、独立滚动内容及单一添加底栏；新WidgetTaskRow显示既有时间摘要、父子关系和重要标记。成功落库后勾选/190ms收拢，减少动态效果时无延迟移除；版本冲突保留原行和错误。迷你条分离数量与首项，正文点击/Enter/Space或下箭头展开，上箭头收起；无置顶按钮，小窗保留图钉；锁定仅禁止位置/大小变更，不阻止展开。背景透明与文字分离，错误在迷你条仍有图形及完整可访问说明；最小高度跟随字体和错误内容，主动窗口模式切换恢复键盘焦点。

共享工具按钮改为紧凑圆角且禁用无悬停填色，滚动条使用次级语义色、仅真实溢出显示；作用于主窗列表/日历、详情、设置和桌面表面。日历工具栏解除隐式最小宽度互相撑大，窄/大字分行，连续月取消重复标题；月格更多数量放不下时以完整省略入口代替截断数字，保留计数提示、键盘和辅助操作。独立日历置顶可访问状态及实色/高对比回退同步。

验证：Qt6.10.2 Release/Debug最终构建和两目标QML lint PASS（`ux38-final3-{release,debug}-{build,lint}.log`，lint无诊断）。Release完整22/24、130.48秒（`ux38-final2-release-tests.log`），仍为旧qml_startup phase372及watchdog phase374超时；未延长45秒预算或删断言。随后最小高度关联5/5 PASS（25.71秒），最终月格/焦点修正Release四个独立UI 4/4 PASS（26.52秒，`ux38-final-ui-tests.log`）、Debug四项4/4 PASS（25.88秒，`ux38-final3-debug-tests.log`）；Debug窗口控制器另已PASS。未重复运行未改变的核心事务全套。新增`desktop_surfaces`独立临时库35阶段、CTest 30秒，完整入口24、fast仍17；只在此profile要求三个窗口，其余保留两个窗口检查。

Windows窗口后端最终独立probe退出0、三个可见窗口、无QML warning（`ux38-final-native.log`）。`build/ux38-captures/final.png*`保存并复看小窗浅深、迷你/错误/200%、完成反馈、260宽/高对比及420宽日历；菜单只据控件尺寸断言验证，单窗grab不包含独立菜单窗口。此为Qt合成指针/键盘与自身窗口抓图，不代表物理输入、真实OS置顶/穿透、壁纸透明对比、读屏或macOS实机PASS；UX-37C真实首帧/P95继续UNVERIFIED。

过程修正：发现200%日历被子布局撑到556宽、完成动画期间空态过早出现、标准快捷键多绑定QML warning；已修正布局最小值、按内容高度延后空态及sequences。菜单关闭与下一次操作分事件轮次；减少动态场景断言落库/模型移除/无动画，不把Qt延迟销毁对象当成业务失败。月格焦点静态警告已修复后双lint无诊断。95份Markdown链接及变更文本隐私扫描PASS；保留未跟踪架构草案，未提交、推送或上传新UI产物。最新程序`build/windows-release/bin/daymark.exe`。

### 1.0.0源码发布（2026-10-26，SOURCE-PUBLISHED；完整二进制门禁FAIL）

用户要求发布1.0、更新版本并提交GitHub；已明确选择先发布源码，Qt运行库再分发许可待确认，不上传Windows便携二进制。更新CMake/应用版本和当前发行文档，保留旧RC历史，复验Qt6.10.2构建/CTest/QML lint及隐私/相对链接；不提高45秒watchdog预算；20ms轮询/软件渲染试验暴露旧探针去抖、tooltip及虚拟行时序耦合，已撤回，保留60ms及完整断言。源码提交包含此前UX-37、schema9打包校验修正；未跟踪架构提案不纳入。检查远端差异及身份/历史，不强推或重写公共提交；只推main及新建v1.0.0标签，GitHub源码Release明确已知未测与不含二进制。

测试方法审视覆盖全部23个CTest入口，按core/storage/integration/ui/watchdog/performance/package归类，新增fast（17入口）/ui/full脚本及UI资源锁，默认和主CI仍full；[完整方法与链路矩阵](testing-method.md)记录各层唯一风险责任。具名C++测试110→108，仅三个外观偏好重复建库/重启场景合为一个，默认/错误类型/改值/重启断言均保留；原150多处worker轮询改为等待同一stateChanged，仍限原5秒。迁移、WAL、导入晚失败、树CAS/中途失败回滚、提醒去重、平台失败恢复及436末段故障注入不删除。发布说明、应用及Windows文件版本为1.0.0；macOS bundle版本元数据同步但本轮未验证。

使用仅tag触发的GitHub源码发布workflow：本机没有API凭据，复用现有SSH推送及作业短期GITHUB_TOKEN、contents:write创建Release，不新建账户/长期凭据、不上传二进制。源码Release与Qt公开再分发/签名/人工平台门禁分离，见[ADR-022](ADR/022-test-layers-and-source-release.md)。最终Release完整21/23（139.30秒，`v1-final-release-tests.log`），旧qml_startup与qml_watchdog_late仍FAIL；工作区测试4.53秒，UX34工作区0.50秒。两构建/两目标QML lint已通过，Windows命令行和文件/产品版本均为1.0.0。Qt许可工具有服务版本诊断但构建返回成功；未修改、注册或绕过许可设置。Debug最终完整21/23（273.46秒，`v1-final-debug-tests.log`），同两项旧探针FAIL。源码提交`23cbc0a28fc1d842fb24e11a344a19f66af3a633`已推送main，注解标签`v1.0.0`解析至同一提交。已验证[GitHub Release](https://github.com/downdone/daymark/releases/tag/v1.0.0)公开页面HTTP 200、版本及仅源码说明，资产仅GitHub自动源码zip/tar.gz，无上传二进制，无Draft/Pre-release标记。公共API受403限制，当前远端平台CI结论未核实，不据源码发布宣称二进制门禁通过。

### UX-37A/B/D：桌面选择、关系图标及全局密度（2026-10-26，IMPLEMENTED；定向PASS / 完整门禁FAIL）

用户追加菜单双高亮、Shift区间与单/多选菜单要求，并明确本轮同时完成[全局密度方案](design/desktop-density-review.md)。本轮已实现菜单唯一活动填色/禁用项透明；ID/版本选择、可见锚点范围、Ctrl/Command增减、文本/列表全选分工、键盘范围及集合内/外右键；批量选择替代完成圈并暂退单项详情/拖动。已完成集合可同事务重新打开，保留独立完成子项，版本冲突/中途失败整批回滚；无schema变更。关联PRD批量P1、FR-025/030/031/035，UX-02/04/05/07/09/10/11/14。

已统一DesignTokens/Theme及共享控件：正文/导航13、列表标题14、详情/工具栏标题16、辅助12；导航32起、智能卡52起、列表36/44起，控件视觉约28/命中至少32。紧凑/舒适偏好全局化并保留保存值，文字缩放独立。父子关系使用独立原创图标、轻缩进和父任务名，父项图形可直达子项；摘要单行去重、溢出保留全文。详情合并真实标题、空备注一行、默认时段摘要、开始/结束属性同行和按需时间/重复规则；无子项分段改为直接添加入口。同步日历新增、分类标签/选择器、设置/组织/提醒/数据及小窗/独立日历的共享行关系/边缘，不压缩日历时间轴。

原行入口已去除先打开完整Pane再关闭的布局/焦点往返；保存期间保留最新导航目标。普通点行展开暂沿用；空白只选中及来源滚动锚点持续跟随仍列UX-37C。UX-37C完整会话数据解耦、30次冷/热P95、物理输入/IME/读屏和macOS本轮源码验证不随本轮完成。独立UX37探针使用临时库、实际Qt指针/修饰键及首个frameSwapped记录；offscreen不替代真实OS延迟。

验证结果：Qt6.10.2 Release/Debug构建及两目标QML lint通过（最终收尾日志`ux37-{release,debug}-title-*`）；服务/工作区和三个UI独立fixture已通过，最后UI三项两构建均3/3。Release全套21/23 PASS（140.26秒，`ux37-release-all-final.log`），qml_startup在phase391、qml_watchdog_late在phase395超时；Debug完整21/23 PASS（149.05秒，`ux37-debug-r4-tests.log`），仍为同两项旧长流程超时。保留45秒内部预算，不将定向通过标作发布就绪。

Windows窗口后端的多选/共享工作区/连续日历三组临时库合成输入均退出0，`build/ux37-native-final/`保存浅深、200%、紧凑/舒适、时间展开/重复自定义、菜单/设置/新增等截图。已复看实际任务标题、起止同行、单/多选菜单与禁用移到无活动填色。首帧埋点收到的`frameSwapped`样本为-1（无呈现信号），因此详情真实首帧与30次P95仍UNVERIFIED；不能用Window.visible、抓图或事件完成时间替代。物理输入/IME/读屏、透明穿透真实效果及macOS本轮源码亦UNVERIFIED。

最小ID/版本选择投影100/1000/10000项本机单次0.056/0.329/2.950ms，`ux37-selection-benchmark.txt`；只测模型投影，不等于QML全选交互耗时。默认紧凑且明确舒适值重启保留的临时库测试通过。清理选择改为可见ID集合，避免逐项线性查询；批量能力判断不构造完整任务详情。

过程FAIL保留：紧凑详情已不溢出导致旧短备注滚动fixture不成立，现明确展开起止参数建立真实溢出并保留焦点/滚动断言；新增探针曾在多选状态点单项详情，改为先从实际Esc退出；原生搜索去抖在繁忙时晚于350ms，增加模型条件等待并仍限时失败。月份定位诊断确认锚点为10月12日但视口误报就绪、仍显示8月31日至10月4日；保留重建过程的reset意图、验证真实目标周/日期后定位，修复后Release共享工作区连续5次PASS（`ux37-month-trial-1..5.log`），双构建和Windows后端再验通过。自定义时间/重复菜单已纳入同一临时层关闭/Esc门，初始批量能力参数未就绪的Qt类型警告已修正，最终日志无该警告。

文档相对链接91文件、312组颜色对比和diff检查通过；四种主题/对比组合普通文字最低4.617、边界最低3.369，新增文本隐私模式零发现。未操作用户真实库，保留此前打包修订及未跟踪架构提案；未提交/推送/发布，也未覆盖旧便携包。Release可执行文件位于`build/windows-release/bin/daymark.exe`。

### UX-37任务列表复审（2026-10-26，历史ANALYSIS / PROPOSED）

以下为本轮实施前的分析快照；后续已实现内容与当前验收结果以上一节为准。

用户要求分析多选、父子图标和详情跟手问题。本轮核查TaskRow/Main/TaskQuickEditor/TaskEditorSession及工作区缓存/增量模型、批量命令和既有HIG契约，形成[系统分析与下一切片](design/task-list-interaction-review.md)。已确认多选入口隐藏且无Ctrl/Command/Shift选择、批量高亮与当前位置分离、选择/完成/详情/拖动命中冲突、关系缺少稳定图标、原行入口经过完整面板填充后再迁移容器，以及busy时部分动作无反馈。未将这些结构性风险冒充实测延迟；task读取已用缓存、模型已有增量通知，详情是Pane且无长入场动画。拟分选择闭环、关系/行骨架、草稿会话/详情首帧三片；本轮不改产品UI代码。

用户随后追加侧栏与详情紧凑程度不统一，要求全局偏macOS密度。已扩为[UX-37D桌面密度](design/desktop-density-review.md)，核查默认420宽面板内部不可达440双列断点、字段标签上下堆叠、重复头部、空备注与多层padding及局部compact覆盖不足，定义跨导航/任务/详情/日历/选择器/菜单/设置/数据/桌面组件的共同token、属性行与渐进展开。拟定紧凑/舒适行高、字级和命中不以缩字替代，全部为待实施目标，不宣称已修改界面。

本轮隔离合成画面在build/task-list-review/，已查看列表父子行及就近面板；探针在stage27月历目标缺失退出2，保留FAIL，不据部分截图宣称全流程通过。Release增量构建PASS；ux34_workspace/task_workspace两项2/2 PASS（13.69秒），它们不替代修饰键/多选实际入口或首帧测量；此前Windows完整流程及watchdog的FAIL不变。物理多选/详情首帧/IME/读屏未测；未访问真实用户数据，保留本地打包修订及未跟踪架构提案。

### Windows便携候选包（2026-10-26，候选组装 / 正式门禁FAIL）

用户要求将59b1644构建为便携包；本轮仅修正打包校验器仍要求schema4而当前程序/打包元数据已为schema9的不一致，补旧schema元数据拒绝回归，脚本补丁尚未提交。Qt6.10.2 Release增量构建PASS；本轮完整Windows测试20/22 PASS（130.05秒），共享面板及连续日历通过，qml_startup在phase371超时、qml_watchdog_late未到预定末段仍FAIL。包清单/隐私扫描、SDK隔离连续日历及运行期DLL来源检查PASS；严格SDK隔离原生校验的版本核对与DLL来源检查通过，但完整流程phase261超时，正式门禁FAIL，不把启动成功算作完整验收。日志portable-*及独立package-check报告保留在忽略build目录。

本地新目录dist/Daymark-1.0.0-rc.2-windows-x64-portable-candidate-20261026-085643；打包仅收集可归因当前HEAD的二进制、运行依赖和受跟踪文档，不含未跟踪架构提案、个人任务库、日志、SDK或构建缓存。BUILD-INFO/VERIFICATION明确releaseReady=false、二进制源码59b1644及schema9校验工作区补丁；正式脚本的全测试和原生门禁保持不变。最终ZIP 46,600,269字节，SHA256为2b19e32d3c8b74e37d0889e63ea90378cea0987d41f4e3696559239d789ad6eb；ZIP逐文件/侧车校验、解压后清单及SDK隔离连续日历复验均PASS，见build/portable-final-summary.json。不上传、不签名，不声称独立干净机/物理交互通过。免安装目录与用户业务数据目录分离；并非随目录移动数据的模式。

### UX-34～36源码发布校验（2026-10-25，用户已明确授权）

目标为origin/main；拉取后确认本地与远端基线一致，仅提交本轮87个源码、测试及设计文档。Qt6.10.2 Release增量构建成功（无待编译项），ux34_workspace/continuous_calendar两项再验PASS（6.03秒）；此前最终Debug/Release各21/21及原生证据继续有效。待提交文本隐私扫描零发现、diff检查通过，截图/数据库/日志/SDK及构建产物均排除。新提交使用项目身份和UTC元数据，不改写已有远端历史；提交及推送结果以Git记录为准。

### UX-34～36整轮：系统方案与追加交互（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户已授权完整实施系统评估与后续追加；[实现索引](design/ux34-implementation.md)、[连续月历](design/continuous-month-calendar.md)、[新增与密度](design/calendar-creation-and-density.md)记录最终契约。已实现范围/视图分离、同一任务及就近详情/同会话展开、父子任务、日周小时轴的创建/移动/端点调整、连续月历有限周回收/跨月条、年只读导航、设置/数据/菜单/可访问偏好及紧凑侧栏。新增浮窗直接填备注、起止、提醒、循环及分类标签；退出前保存、空标题取消、错误保留，全部属性通过C++一个事务提交。添加成功留在原日历，点击新条目打开同ID详情。默认正文13/任务标题16/页面标题22 DIP，侧栏200、设置560×460、详情约420/新增400宽，32命中与200%滚动保留。设置/备份纯图标。

最终源码Qt6.10.2/macOS15.8.1 arm64：Release21/21 PASS（105.87秒）、Debug21/21 PASS（112.26秒），两构建的两项QML lint均无诊断；日志`ux36-final-{release,debug}*`。旧436阶段、共享详情34阶段、连续月历/完整新增19阶段独立fixture及末段436故障注入均通过，未扩大45秒内部watchdog预算。新建无效起止/循环/标签零残留、备注/时段/归属同事务及重启持久均由临时库验证；C++时间/DST/CAS/树联动旧断言保留。

最终Cocoa三组退出0、warnings=false，共193 PNG：历史164/58.940秒、共享17/18.962秒、连续及新增12/17.982秒；截图`build/ux36-final-{native,contextual,continuous}/`。复看紧凑导航/列表、设置、就近新增、浅深及200%；实际合成指针覆盖跨月滚轮/边界回收、时刻/范围新建、拖入时段/改期/端点/Esc、外部点击保存及同ID详情。三组10秒请求短跑实际11.898/12.087/12.217秒，各1循环/1完整性检查，最大事件循环延迟1/51/50ms，均PASS，userDataAccessed=false。静态本地链接、文本隐私、312组颜色对比与diff检查见`ux36-final-static.json`、`ux36-colors.json`。无schema、新依赖或真实用户库修改；实现阶段未提交/推送，用户随后授权发布，见上。

过程失败保留：新增字段多实例标识重复使旧探针定位隐藏控件，改用实例objectPrefix；Popup不能直接取Window.window，改取contentItem宿主；紧凑工具栏造成切换图标位移，改固定顶对齐；侧栏200替代208的旧几何断言同步目标。日志`ux36-full-release`、`ux36-names-tests`、`ux36-polished-tests`保留FAIL，修正后全套重跑。新增探针QString信号参数错误与文档脚本UTF-8失败均已修正，不将中间结果充作最终PASS。以下UX-34/35段落是历史实施过程，最终状态以前述本段为准；追加前21/21/190图证据仍保留，不混用版本。

Windows当前源码、物理键鼠/触控板惯性/IME/VoiceOver/Narrator、真实系统通知/穿透、多屏/睡眠/8小时和独立部署签名仍UNVERIFIED；合成截图及短跑不替代这些门禁。

### UX-35追加：连续月历与进一步收敛（IMPLEMENTED；追加前AUTO-PASS）

用户在UX-34原生验证后追加：日视图选择时刻/范围直接新建、月历连续滚动多月，以及分类/标签区更紧凑、设置/备份仅图标。先按[连续月历设计](design/continuous-month-calendar.md)分片，保留已有统一任务/保存门/DST/范围和父子规则。UX-34最新Release/Debug界面专项与Cocoa合成均通过；原生180张截图、短跑11.821秒/1循环/完整性1次/最大延迟1ms，见ux34-polished-native/。截图发现并修正设置尾部裁切、200%日历工具重叠与子项面板过高留白；未据旧截图宣称新追加已实现。


UX-35过程验证：共享月格提取后完整探针重新通过；连续周临时库测试覆盖唯一周、跨月范围条、闰日和版本不变。末片专项3/3 PASS（45.44秒），lint无诊断；日志ux35-span-*。Release首轮20/20 PASS（102.59秒）；Debug首轮旧链＋新增场景合跑在45秒末段超时，未放宽预算，改拆为历史/共享详情/连续月历三个独立fixture。拆分后Release界面三项3/3 PASS（43.65秒），最终共享/连续专项2/2 PASS（9.09秒），日志ux35-isolated-*和ux35-final-scenarios-*。原生先发现日期跳转受后台busy/虚拟化焦点影响，保留FAIL日志ux35-accepted-native.log，修复后继续最终复验；以上不是整轮最终PASS。

### UX-34实施：R0→R7（IMPLEMENTED；追加前AUTO-PASS）

2026-10-25用户要求完整实现上述规划，FR-034/035及ADR-020转为本轮实施目标。单代理，保留此前12份设计文档变更。按R0验证保护/子项绘制→R1范围与返回→R2表达→R3共享面板→R4月历→R5日周小时轴→R6全窗口→R7年导航/门禁推进，不自动提交或推送。当前先处理watchdog完成条件、图标首帧及scope/presentation接口；相关UX-01/02/04～14。风险为旧436阶段含旧跳页/保存预期、时间/DST和异步草稿，保留原业务断言并逐片真实构建/测试，真实库不操作。


实施过程证据：R0 Release17/17 PASS（56.09秒，QML43.66秒），日志ux34-r0-build-test.log。R1新增范围保持临时库测试，calendar_service/task_workspace两项PASS（9.87秒）。已接入范围/呈现分离、任务ID锚点、统一摘要/子项完成数、共享面板/展开、内部日期/时间/提醒选择和编辑记录内部页，以及日周小时轴/年导航。新时间服务测试覆盖重叠、午夜排除、DST23/25小时、时长/时区保持和CAS/非法边界零修改。界面仍在VERIFY，尚非整轮PASS。

探针发现并修正应用提前退出可能返回0的缺口：必须有完整完成状态，失败诊断包含阶段。中间1.23秒提前退出曾被CTest报告PASS，已作废。旧436阶段已完整通过，新增29阶段验证日周月年、单层面板、草稿失败保持、实际指针拖入小时轴/调整端点/Esc取消/拖出45分钟新建/月条整段平移。专项4/4 PASS（84.51秒），其中QML37.40秒，末段436注入卡住被45秒内部watchdog以退出3拒绝（45.45秒）；日志ux34-range-probe-*。不放宽45/120秒预算。分类右键被关闭Drawer的输入层遮挡已复现并改为按钮开启的非模态侧面板；一次新探针错误地在日历视图查列表模型，改为查真实时间轴身份/时长，业务断言保留。

用户追加可维护性要求后，将只读投影拆到TaskWorkspaceCalendar.cpp；共享工作区源清单；新建、未安排、日期改期和指针识别分别独立QML，时间命令仍在CalendarService、SQL仍在Repository。模块拆分后专项3/3 PASS（38.50秒，QML37.11秒）、两项lint无诊断，日志ux34-modules-*。当前继续最终布局/架构审查、Debug/Release全套、Cocoa截图和短跑；尚未认定整轮完成。自动测试均临时库。UTF-8脚本初次失败、旧二进制结果作废和输入层/重叠压缩初次FAIL保留，不能把中间结果代作最终源码通过。

### UX-34：全产品系统评估与优化方案（历史设计基线；实施结果见上）

按用户要求系统评估软件哲学、功能及UI/UX，以e1e2ff6/UI-33为基线，覆盖61份QML与查询/时间/树/工作区/探针。形成[72项评审](design/system-ux-review-2026-10.md)，每项区分源码事实、截图、待补验与设计判断，给出优先级、建议和可观察验收；映射P-13/14及UX-01～14，保留六入口、两级分类/任务、父子联动、草稿与数据安全。推荐先可信基线→范围/返回→统一任务表达→就近编辑→月历→日周小时轴→设置/数据/组件分片→年导航和双平台验收，不一次重写整套界面。

用户追加四张macOS日历截图及详情小窗建议，已纳入[日历v2](design/calendar-workspace-v2.md)和[就近任务面板](design/contextual-task-inspector.md)：一套Task、列表执行/日历安排；范围与查看方式分离，日/周小时轴、月分布/跨天、年先导航且零全年物化。默认详情推荐就近面板，根任务详情/子项双tab，子项在同一面板访问/返回，长内容和大字展开同一编辑器；推荐结束编辑前保存、失败不关闭/不导航。FR-034/035和ADR-020明确为目标提案，当前完整页仍显式保存，未因写文档改变任何运行行为。导航/产品哲学/UI基准/验收已整理当前入口并标明历史目标，不恢复领域/项目或双日程对象。

基线复验：Qt6.10.2/macOS Release增量构建成功（无待编译项），CTest17/17 PASS，56.05秒，QML43.55秒，日志ux34-baseline-{build,tests}.log。复看UI-33最终原生子列表、任务菜单、单一起止及月历截图；发现子列表源码有控件但截图仅见标题，列为待复现，未凭几何断言认定视觉通过。源码确认smoke完成436阶段，但内部watchdog在phase>=398时跳过，列为P1先修；外层CTest60秒仍在，本轮PASS不证明末段内部超时保护正确。

静态审查83份Markdown本地链接零缺失、72项编号连续、61个QML文件覆盖无遗漏，12份本轮文本隐私模式零发现、diff检查通过，证据build/ux34-static-review.json。本轮只改设计/规格/进度文档，无运行代码/schema/真实库修改，无提交或推送。Windows、物理键鼠/IME/VoiceOver/Narrator、系统通知/穿透、多屏/睡眠/8h与独立部署仍UNVERIFIED；新方案没有AUTO-PASS。下一实施首片见评估R0，面板迁移前先补共享会话和失败关闭门。

### UI-33：统一起止安排与任务动作（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

按用户三项反馈先形成[设计](design/shared-task-time-and-actions.md)及[ADR-019](ADR/019-shared-task-time.md)，覆盖FR-027/028/030～033、UX-02/04～07/09/11/14。任务只保留一套开始/结束日期，关闭全天后为同一日期指定时间/时区，移除重复日历时段字段；列表、日历横条/标记/议程同ID直达详情。C++统一精确时间与兼容日期投影，日期改期按原时区墙钟处理DST和秒/毫秒，非法范围/时刻零修改；Today/逾期按本机日历日。旧不一致日期只在修改安排时统一，原日期前后值保存在编辑记录，无关备注修改不覆盖旧事实。无schema变化，备份/旧JSON保留。

任务行超过系统拖动阈值直接拖，长按/手柄/Esc及键盘替代保留。普通列表支持合法分组手工排序并重载持久，计划/完成/搜索保持原排序；置顶、同父、全部分类及今天时段限制保留。任务右键/Menu/Shift-F10就近打开详情、置顶、特别关注、取消提醒、完成/重开、移动、排序、删除；取消提醒清周期与相对偏移，删除复用整树软删恢复。分类/标签明确“重命名…”入口，就近名称全选，保留颜色/说明/层级/CAS。详情保存回调仅处理自身发起的保存，不误消费菜单/其他命令或清除草稿。

最终Qt6.10.2/macOS15.8.1 arm64：Debug17/17 PASS（62.31秒，QML44.52秒）、Release17/17 PASS（59.66秒，QML43.74秒），两项QML lint在两构建均无诊断。Release offscreen及Cocoa均436阶段退出0、各164张PNG、窗口2个/QML warnings=false。Cocoa菜单命令2.08296ms、两次呈现上界8ms；10秒请求短跑实际11.788秒、1循环/1完整性检查、最大事件循环延迟21ms，PASS；offscreen短跑12.251秒、1循环/1完整性检查PASS。日志ui33-completed-{debug,release,debug-lint,release-lint,offscreen,native}.log，截图/soak.json在ui33-completed-{offscreen,native}/。已查看最终单一起止、原生右键菜单、就近改名和深色200%时间布局；312对颜色、80份Markdown本地链接、123份变更文本隐私模式和diff检查零失败，见ui33-completed-static.json。

过程FAIL保留：旧测试按首行假定新任务，改按任务身份；双日期旧预期按新契约替换而保留回滚/CAS。原生滚动探针漏originY、旧delegate查找、缩放后尚未产生当前item，改用实际ListView.itemAtIndex、真实滚动原点及有界等待，保留几何/拖放/持久断言。菜单保存误触详情回调曾导致短跑失败，editorSaving隔离后完整重跑通过。锁屏时系统明确拒绝原生激活，LaunchServices尝试在dyld加载前阻塞，经采样只终止本轮新测试进程；解锁后最终直接Cocoa运行通过。激活仅隔离smoke调用本进程AppKit；45/120秒watchdog、100ms菜单预算及旧手势断言未放宽，新增阶段节拍60ms。Qt生成C++未使用参数、offscreen平台和原生IMK诊断保留，不将QML warnings=false说成全系统无诊断。

单代理，测试全为临时库，无真实用户库操作、提交、推送或发布。Windows、物理键鼠/IME/VoiceOver、真实系统通知/穿透恢复、多屏睡眠/8h和独立部署签名仍UNVERIFIED；产物依赖本机Qt Kit。短跑不替代长时验收。

本轮提交发布范围：用户明确授权提交并推送GitHub；将UI-21～33累计源码、schema005～009、测试与设计记录作为一次完整变更提交到当前main。推送前已确认远端同步，暂存123份文本的隐私模式/本地路径/文件类型零发现、diff检查通过；Release增量构建及17/17 CTest通过，证据ui33-prepush-{build,test}.log及ui33-prepush-audit.json；构建产物、截图、数据库与机器配置保持忽略。该操作仅发布源码，不创建Release或上传用户数据。

### UI-32：紧凑导航与子任务直达（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

按用户两图核查窄窗Grid同时容纳列表/详情，详情落到视口外，以及图标底座/行距过重；先形成[设计](design/compact-navigation-and-subtasks.md)及[ADR-018](ADR/018-two-level-tasks.md)，落实FR-025/030/031/032、UX-01/02/04/05/07/09/11/14和P-13/14。导航208起、文字12、智能卡60起/计数16，来源行去白色底座、分区间距收紧；保留16文件夹/标签图形、二级缩进、32命中及200%自然增高，不全局缩小正文。

子任务页现在只列直接子项及连续新增，圆形完成和详情动作分离，点击标题/信息直接进入独立可见详情；保存或返回回父列表，取消离开保留子项草稿。普通子项没有子任务tab/新增下级及“查看此子任务的子任务”按钮，不自动选择第一项产生隐藏编辑对象。Service拒绝子项下新建、归入子项及带下级降级，选择器仅列合法根；失败保持版本/数据/编辑记录。已有深层库保留原关系编辑、访问、整树恢复和备份导入，不静默改层级，不改schema。

最终Qt6.10.2/macOS15.8.1 arm64：Debug17/17 PASS（59.99秒，QML42.84秒）、Release17/17 PASS（58.41秒，QML42.12秒），两项QML lint无诊断。Release Cocoa413阶段退出0、158张PNG、窗口2个/QML warnings=false；菜单命令2.32908ms，两次呈现上界14ms。10秒短跑实际11.883秒、1循环/1完整性检查、最大事件循环延迟1ms，PASS，不替代8h。已查看最终紧凑来源导航、父列表、独立子项详情、深色200%子项/父列表，实际指针点击行、保存返回和脏草稿取消/放弃断言均通过。日志ui32-{debug,release,debug-lint,release-lint,native}.log，截图/soak.json在ui32-native/；颜色312对零失败、78份Markdown链接/120份变更文本隐私模式/diff检查见ui32-static.json。

过程：首片3/3专项及42.66秒新增界面专项通过。旧深层fixture不再调用新Service创建三级，改由Repository重现旧资料，保留整树恢复/事务回滚/历史断言；一处旧测试重新降级带下级任务按新裁决改为先拒绝，再显式移出下级后合法归属。手势、草稿、菜单100ms及45/120秒watchdog均保留。Qt生成未使用参数与IMK诊断保持，不把QML无警告表述为所有系统无诊断。

自动库全部QTemporaryDir，未操作真实用户库或已有进程，无提交/推送/发布。Windows、物理键鼠/IME/VoiceOver、真实通知/穿透恢复、多屏睡眠/8h与独立部署签名仍UNVERIFIED；本机Qt Kit开发产物。

### UI-31：全窗口桌面语言统一（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

按用户五图先分析并形成[设计](design/macos-desktop-language.md)，落实FR-007/010/012/013/018/024～031、UX-01/02/04～07/09～14和P-13/14。收集箱已从分类区移到顶部，与五个智能视图组成六入口；全部改用列表图形避免混淆。分类区只保留自建两级文件夹，标签独立，最近删除与底部设置/备份分层。统一浅深控件、紧凑分段、表单标签/值对齐、主按钮和带分隔的固定尾部；新增分类/标签和局部编辑共用表单，标签颜色可直接选择并持久保存。设置保留三分类及全部偏好，透明度和文字大小收为对齐行；数据、批量、安全确认、日期及独立窗口沿用同一控件语言。快捷日期芯片直接显示日期/动作名称，父子缩进、单草稿、自动保存/显式保存边界与失败保护保留。

日历集中工具与月份翻页，日期右对齐、细网格/淡周末、今日圆点、连续跨天色条及键盘焦点分明；泳道按每周实际内容分配，溢出入口也检查边界。默认留空间给网格，议程按需展开，宽窗右侧/窄窗网格之前；收起后复位网格顶部。日期/时区/循环语义、所有原命令及软删恢复保持，不改schema、不操作真实库。按住任务时交互状态改独立通知，避免stateChanged重入enabled绑定导致手势中断；维护仍在手势结束后合并补做，已补临时库回归。

最终Qt6.10.2/macOS15.8.1 arm64：Debug17/17 PASS（55.01秒，QML42.02秒）、Release17/17 PASS（52.92秒，QML41.36秒），两项QML lint均无诊断。Release Cocoa407阶段退出0、152张PNG、窗口2个/QML warnings=false；菜单命令2.26554ms、两次呈现上界12ms。10秒短跑实际11.933秒、1循环/1完整性检查、事件循环最大延迟1ms，PASS，不替代8h。已查看最终月历回顶、跨周色条、六入口/文件夹、设置、新建标签/分类、深色200%新增及数据、快捷编辑和局部编辑截图。最终日志ui31-verified-{debug,release,debug-lint,release-lint}.log；原生ui31-active-native.log，截图/soak.json在ui31-active-native/。静态颜色312对零失败；文档链接/变更文本隐私模式/diff审查见ui31-verified-static.json。

过程FAIL保留：异步滚动议程竞态改窄窗前置；Repeater的QObject查找不覆盖实际色块，改通过真实delegate点击并验证入库颜色；按住任务的共享通知绑定循环修复；初次新增通知测试把refreshClock主动业务通知也算入，改断言手势切换不额外发业务通知。CLI及系统启动器可能留下无活动窗口，导致Esc无法分派；macOS仅隔离smoke使用本进程AppKit激活，等待系统确认活动窗口再发合成键盘，普通路径不调用。一次系统启动器在dyld加载前阻塞，经采样确认后仅终止本轮新建测试进程，改直接运行测试产物后通过；无旧进程/用户库操作。45/120秒watchdog、100ms菜单预算及旧手势/草稿断言未放宽。Qt生成C++未使用参数及IMK系统诊断保留，不把QML warnings=false说成全系统无诊断。

Windows当前源码、物理键鼠/IME/VoiceOver、真实通知/穿透恢复、多屏/睡眠/8h及独立部署/签名仍UNVERIFIED。产物依赖本机Qt Kit，无提交、推送或发布。

## 当前目标与范围

### UI-30：提醒事项式视图与任务树联动（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

按用户macOS五视图截图及“全面跟随：父子完成、删除和移动联动”，先修订[设计](design/task-list-context.md)、[ADR-017](ADR/017-reminders-views-and-task-trees.md)和FR-031，对应UX-01/02/04/05/07/08/09/11/14及P-13/14。最初的自动补入子树/父删除保留子项提案作废，旧文档相关定义由本裁决覆盖。单代理，保留原工作区变更，未操作真实库或推送发布。

已实施：今天、计划、全部、特别关注、完成五个入口，收集箱为默认未分类列表；今天按时段、计划按日期/月、全部按分类、完成按完成月份组织。智能视图精确匹配，蓝色数量打开子任务tab，普通列表保留树/折叠；搜索不被隐藏偏好漏掉。最近删除按同次删除显示父子展开树，可恢复整组。分类移动联动后代，移动子项跨分类会带走子树并脱离原父项；完成/重新打开及删除/恢复按持久组处理，不误改此前单独完成/删除的成员。批量祖先优先、事务/CAS/失败回滚；循环子项不在已完成/删除父项下生成。schema009先备份再迁移，JSON9及JSON1～8兼容、严格校验与零修改失败；工作区提交后同步整个结果集合，计数缓存，模型仍增量刷新。

界面收尾：列表标题13/元信息12、40起行高、自然增高，简易编辑缩进/列与L线保留，末行和鼠标焦点不多画边框；就近分类/标签小面板保留保存、取消、失败输入与脏草稿保护，标题常驻管理省略号移除，归档恢复经分类标题右键。去旧混合状态/关注选择，日期决定安排，旧暂停任务有明确恢复入口；日期/提醒/实际时间不隐式继承，实际时段不能靠跨组排序悄悄变化。统一浅深ToolTip，智能入口恢复导航顶部，拖放目标实际高亮。

最终Qt6.10.2/macOS arm64 Debug17/17 PASS（54.27秒，qml41.36秒）、Release17/17 PASS（52.37秒，qml40.71秒），两项lint无诊断。Release Cocoa398阶段退出0、145张PNG、双窗口/QML warnings=false；菜单命令2.67746ms、两次呈现上界13ms。10秒请求短跑实际11.968秒、1循环/1完整性检查、最大事件循环延迟1ms，PASS，不替代长时验收。已查看最终拖放高亮、五视图/子任务入口、最近删除树、完整详情、子任务简易编辑及深色200%列表/小面板/提示。颜色312对零失败，75份Markdown链接、104份变更/新增文本隐私模式及diff检查通过。日志ui30-accepted-{debug,release,debug-lint,release-lint,native}.log，截图/soak.json在ui30-accepted-native/，均在忽略build目录。

过程失败保留：嵌套事务、单根缓存刷新、重叠父子批量、旧独立语义及固定空日期断言已按新契约修正；保留父任务防循环、置顶/重要性独立和编辑记录CAS断言。分类拖放因导航变高而需要真正揭示两个目标；智能视图位置恢复后才揭示目标并点击数量入口，未把隐藏控件当通过。截图发现深色提示及删除树不足后继续修订。一次原生固定40ms完成采样失败，改追踪实际prepare的行、afterAnimating状态及frameSwapped呈现；110/80ms动画、减少动效、100ms菜单及45/120秒watchdog均保留，探针节拍90ms。初次/中间PASS不替代最终源码证据。Qt生成代码unused parameter与IMK系统诊断仍保留，不把QML warnings=false说成全运行时零诊断。Windows当前源码、物理输入/IME/读屏、系统通知/穿透、多屏/睡眠/8h及独立部署/签名仍UNVERIFIED；Mac开发产物依赖本机Kit。

### UI-29：紧凑桌面布局与任务信息层级（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户反馈简易编辑丢失子任务缩进、安排/时间/循环关系混乱、导航字体和密度不协调、分类不能实际拖拽排序。先完成[分析与设计](design/desktop-task-inspector.md)，按共享对齐及导航→详情逻辑分组→真实指针上下排序分片实现；对应 FR-024～030、UX-01/02/04/05/07/09/11/14、P-13/14。当前已识别展开行清零左边距及未预留展开列，导航动作撑高行，以及 DropArea 拒绝中部进入后无法继续跟踪边缘位置的风险。保留时间/提醒/重复独立语义、自动保存失败草稿、分类 CAS/原子排序及全部能力，不改 schema，不触碰真实库；最终需 Debug/Release/lint、Cocoa 真实指针合成及浅深/窄窗/200% 截图。之前 UI28 自动通过不能证明用户指出的体验已合格。

已实施：浏览/简易编辑共用层级步长及展开/完成/标题列，备注和快捷动作跟随标题，L线在编辑时保留；导航采用13字号/32起行高，正文14、分区12，品牌/图标/计数协调，减少无效留白；控件/行/面板统一圆角6/7/10。完整详情归为任务状态、日期与时间、分类与标签、任务关系，时间/重复/标记不再独立大卡；文档列居中限宽，去分组重复彩色图标底座。日期改锚定日历，分类/标签小面板按空间向上避让，退出简易保存及完整草稿保护不变。分类拖动持续跟踪非法中部，合法上下缘恢复排序，只显示细插入线；中央合法移入原行显示“移入：名称”，文件夹图标不变。

新增真实指针两方向排序及重载持久化、带子分类一级的非法中央→合法边缘恢复、子任务展开前后实际列坐标回归，普通/Cocoa扩至372阶段。排序先归一临时库初始顺序，确保首个手势真正向上、第二个真正向下，不依赖UUID原有相对顺序。初次分类面板错误读取Popup的Window附加属性导致QML警告，改从父Item读取后修复。截图审查发现旧200%快捷探针只验了隐藏控件几何、嵌套时间分组的旧滚动坐标也失效；已要求真正进入编辑并完整位于视口，时间定位改映射正文坐标且尊重主动滚动。没有删功能/失败断言或放宽45/120秒watchdog；旧截图不能作为新验收证据。

最终Qt6.10.2/macOS arm64：Debug17/17 PASS（56.73秒，qml43.63秒）、Release17/17 PASS（55.33秒，qml43.34秒），两项QML lint无诊断。Release Cocoa372阶段退出0、双窗口/QML warnings=false，136张原生合成截图；菜单命令2.46383ms、两次呈现上界14ms。10秒短跑实际11.823秒、1循环/1完整性检查、最大事件循环延迟1ms，PASS，不能据此替代长时验收。已查看紧凑四组详情、子任务展开对齐、两方向插入线及最终深色200%实际快捷编辑/时间定位/管理页；颜色300对零失败，文档链接/变更文本隐私模式/diff检查通过。最终日志ui29-verified-{debug,release,lint,native}.log，截图及soak.json在ui29-verified-native/；过程ui29-slice/interaction/preview/final/delivery证据保留在忽略build目录。Qt生成C++仍有unused parameter诊断，原生日志有IMK系统诊断，不将QML warnings=false说成全运行时零警告。Windows当前源码、物理键鼠/IME/读屏、通知/穿透恢复、多屏/睡眠/8h及独立部署签名仍UNVERIFIED。本机Kit开发产物，无真实用户库操作、无推送发布。

### UI-28：分类/标签操作链与浏览修订（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户最新追加“删除领域和项目概念，只保留分类”“分类统一文件夹图标”：已移除旧属性面板的日期/执行状态/完成通路，导航、详情、批量操作、错误提示及当前使用说明只使用分类；一级/二级及历史兼容入口均使用folder图标。产品哲学重写为当前单一归属模型，不继续保留旧概念教程。旧隐藏状态在categoryTree统一投影为已归档，UI恢复时归一活动状态并保留日期等历史元数据，新增临时库恢复回归。无schema覆盖或真实库操作。最终证据为ui28-categories-*；此前各阶段日志保留。

用户追加分类/标签保留两个导航但统一管理、新建、层级转换和归属；另修空白点击退出简易编辑、列表子任务、日历背景色、固定视图按钮和导航内部开关。先查明入口独立拼树/创建缺上级、列表只有缩进、标记旧event分支及工具按钮布局根因，再按[设计](design/catalog-and-list-system.md)分片实现。对应FR-024～030及UX-01/02/04/05/07/09/11/14；不改变任务范围判定、不操作真实库，保留草稿和迁移数据安全。

用户后续明确列表子任务开关/极简L线、简易退出即保存、分类拖放排序/移入一级、分类标签右键直接管理。已实现单一C++categoryTree路径和层级投影、共用创建及上级预选、移动预览、两区常驻/一级展开、上下缘插入/中央移入、原图标保留、右键/键盘菜单。服务统一两级校验及源/目标CAS，Repository单事务移属/排序，失败回滚且任务不变；新增注入rank写失败回归。列表全局隐藏偏好不影响Today数量，父子分组不拆树；简易离开先保存、失败保留输入焦点，完整详情仍显式保存。视图位置固定、导航顶端开关/窄轨、全部日历任务标记语义背景已接入。

最终macOS arm64 Qt6.10.2 Debug17/17 PASS（55.22秒，qml42.53秒）、Release17/17 PASS（54.60秒，qml42.17秒），两项lint无诊断。Cocoa Debug361阶段及132张合成截图通过，双窗口/QML warnings=false；菜单1.51196ms、两次呈现上界14ms。10秒短跑实际12.05秒、1循环/1完整性检查、事件循环最大延迟50ms，通过；不等同长时稳定性。已查看最终全文件夹分类树、深色200%管理页、父子列表连接、右键定位和日历标记。分类拖放随机UUID回归连续20次PASS；颜色300对、文档链接/隐私模式/diff审查见最终静态报告。日志ui28-categories-debug.log、ui28-categories-release.log、ui28-categories-lint.log、ui28-categories-native.log及对应截图/soak.json。自动库全部临时目录，物理输入/IME/读屏、Windows当前源码、系统通知/穿透/多屏/睡眠/8h与独立部署签名仍UNVERIFIED。Mac产物依赖本机Kit，不推送发布。

过程FAIL保留：旧折叠探针宽0按新恢复窄轨改为44并验证按钮；probe局部find作用域编译错误已修正；旧简易草稿确认断言按新裁决替换为自动保存成功及失败保留。改名导致搜索不再匹配的场景最初改备注隔离测试，随后确认行重建回调会重新打开编辑，增加inlineAttachWanted会话意图并恢复改名真实回归，保证保存离开后不会重开。分类rank回归曾假定初始UUID排序为创建顺序，改为验证目标前紧邻且保留其他兄弟顺序，独立随机UUID连续20次PASS；不是忽略排序失败。截图发现展开按钮抵消子任务缩进，统一占位列；普通导航重复tooltip及拖拽tooltip已收敛；右键菜单明确锚定对应行，不在内容区中间。lint未限定父delegate的modelData已修正。45/120秒watchdog未放宽。

原生过程补充：ui28-completed-native在减少动效的完成断言失败。探针递归查找同ID时可能取到ListView延迟退场的旧delegate；改为同时匹配当前有效模型索引，并保留完成落库/无过渡断言和失败诊断截图，继续原生门禁。没有提高超时或忽略失败。

分类图标探针曾将同前缀的DropArea当成导航行，改为只检查有symbol属性的分类行；原层级全文件夹断言保留，最终Cocoa通过。

### UI-27：统一任务系统（IMPLEMENTED / AUTO-PASS；集成最终门禁见UI28）

2026-10-25用户提出11项系统改进，追加明确“所有日程转任务，支持循环及起止时间”“删除清单，直接子任务，详情/子任务分tab独立编辑”。已先核查schema4、分类/查询/共享草稿、两种循环及事件关联/提醒/数据工具；完成[分析与设计](design/unified-task-system.md)及ADR-016，不把设计标为实现。按分类→子任务/特别关注/置顶→统一时间/循环/旧事件→编辑交互分片实施，默认单代理，每片构建测试；新需求覆盖旧概念边界，不能仅改名/合并入口。先追加迁移及临时库兼容测试，不操作真实用户库、不推送发布。跨平台/物理交互门禁独立记录。

UI-27A分类已实现并回归：schema5追加，将旧领域迁为分类并保留原项目父子/直接任务关系；服务防自引用/三级/带子分类降级，升降级/归档/安全删除，JSON1～4归一化及冲突回滚。最终Debug16/16（49.76秒）、Release16/16（52.37秒）、两项lint通过，日志ui27a-final-*。首轮旧JSON重复归一化及旧升级测试未真正移除新列失败均已修复，实际旧schema4库迁移/ID冲突/备份回归通过。旧内部area/project API仅兼容历史资料，界面分类统一。

UI-27B及部分27D已实现：schema6迁移旧清单为真正子任务；父子防环、独立完成/软删访问、树形列表，特别关注系统视图及置顶属性；任务详情/子任务tab、独立列表和编辑面板、单一当前草稿及切换保护；右上角保存/更多/真实编辑记录弹层、加入今天开关与白天/晚上选择、日期图标直开日历、备注全文tooltip、明确浅深主题。Debug16/16（52.83秒）、Release16/16（51.74秒）、两项lint PASS；Cocoa333阶段PASS，双窗口/QML warnings=false，菜单2.08842ms/两次呈现13ms，已查看子任务双面板和真实编辑记录。日志ui27b-full-*、ui27b-native.log及截图位于忽略build目录。按新契约将已废止清单拖动/清单草稿断言替换为独立子任务保存、tab切换取消/放弃/父内容保留；日期快捷菜单断言替换为直接日历/当前日期/单字段改变；旧纯查询测试无id被错误当作父关系、旧关注currentIndex探针已修正，原45/120秒watchdog保留。

周期提醒schema7已通过Debug16/16（53.15秒）、Release16/16（52.34秒）、两项lint；本地墙钟/DST/月末、错过多次汇总、重启认领及JSON历史兼容均有临时库回归。

UI-27C/D源码已完成，集成验证结果见UI28：schema8时间组件只有task_id及时间事实，旧事件按原ID迁Task，准备/跟进关系及认领历史保留；循环实例是真正Task，旧事件/清单表冻结兼容，两个独立循环页和清单编辑通路已移除。主/独立日历直接共享详情与完成；源任务开启循环、后续范围/CAS/回滚、历史/手改保护、浏览日历补实例、相对日期及DST已有临时库测试。用户定位“日历+提醒事项”及FR-029删除规划分组已落地为顶部列表/日历图标，返回原列表位置；循环仅普通列表筛选。新增unified_tasks测试，当前17项；提醒历史归实际实例、旧JSON多次预览迁移ID稳定、后续规则不重新生成历史/重复提醒等边界已专项5/5 PASS，最终Debug/Release/lint/Cocoa截图/短跑已集成通过，详见UI28。真实用户库仍未操作，无推送。

过程记录：首次旧事件系列/伪降级fixture失败后，改真实历史schema资源和独立实例断言，保留时间、事务与恢复验证；QML分号和C++auto混型编译失败均修复。一次合成Esc拖动回归暴露隐藏行内编辑控件保留焦点，收起时解除宿主与焦点引用后原断言通过。清单移除后父任务的派生子任务数量改变，失败零修改断言改为创建完关系后的完整快照，不忽略派生字段。日志ui27c-*、ui27-reminder-boundary-*保留，不将旧可执行文件测试当作当前源码通过。

### UI-23A：详情直达、导航拖放、安排生效与跨天任务（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户三图要求已按[设计](design/direct-task-actions.md)、ADR-015及FR-023实施。行尾详情直接打开共享完整草稿；开放任务从非排序列表也能拖到六个任务目的地，统一服务校验/CAS及成功反馈，未来日期选择与收集箱清除确认均可取消。任务安排显式选择收集箱会清除日期/关注草稿，保存后保持一致；任务开始到截止投影连续范围，与事件共用跨周泳道/溢出，隐藏事件不隐藏任务，点击同一任务。无schema变更，真实库未操作、无推送。

2026-10-25最终Debug16/16（48.74秒，qml_startup39.32秒）、Release16/16（47.85秒，qml_startup39.13秒），两项lint无诊断。Release Cocoa333阶段PASS，125张合成截图、双窗口/QML warnings=false；菜单命令3.31712ms，两次呈现上界15ms。已查看任务横条、导航命中预览和深色200%日期标题。颜色296对、69份Markdown链接、48份变更/新增文本隐私模式及diff检查通过。最终日志ui23-delivery-debug.log、ui23-delivery-release.log、ui23-delivery-lint.log、ui23-delivery-native.log及对应截图位于忽略build目录；Mac开发产物仍链接本机Kit。

过程FAIL与修复：嵌套Menu的visible绑定导致Qt构造期崩溃，改enabled；长按层遮住真实手柄，纠正内容z；导航Repeater用视觉树查找并判空；预览热点左裁切及200%日期回顶修正。新增成功提示改变列表高度后，原生探针在视图恢复前定位导致点到列表外，改为布局后定位且校验真实命中点在列表内，再发原指针事件；不删拖动/落库断言，不放宽45/120秒watchdog。IMK系统诊断仍存在；物理输入/IME/读屏、Windows当前源码、独立部署/签名及长时平台门禁仍UNVERIFIED。

### UI-22：全产品复审与简约基线（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户要求基于产品哲学重新审视整个UI/UX与功能，并明确“简约而不简单”。已按FR-001～022、UX-01～14及HIG Simplicity/Purpose/Agency/Sidebars/Modality完成[全产品复审与方案](design/product-experience-review.md)，加入产品哲学及P-14；UI-22作为第一片减负实现。保留草稿、错误、安全确认、现有数据/查询语义和全部能力，不一次性重写所有页面。后续UI-23～26及既有CAL/ENG/平台队列仅完成设计拆分，不记作已实现；物理交互、Windows当前源码及独立部署另记，真实用户库不操作、不推送。

简约切片已实施：页面标题去口号；整理概念只在无数据空态短说明；已有列表/空态按状态保留单一新建，编辑时聚焦保存，窄窗列表填满正文；任务整理去重复解释。后台维护不转圈/冒充保存，空闲不显示“已保存在本机”；“已激活”改为“可执行”，“提交系统提醒”改为“系统通知”，不支持开机启动的平台准确说明。空标签不占侧栏，任务字段与整理页仍可新建；没有减字号、删草稿保护或减少危险确认。

最终源码Debug16/16（46.16秒）、Release16/16（45.15秒），qml_startup分别36.90/36.74秒，两项lint无诊断。Release Cocoa309阶段及121个截图文件PASS：退出0、双窗口/QML warnings=false，菜单命令2.83933ms、两次呈现上界16ms；已查看最终窄窗深色200%整理/规则页面，正文和固定动作可达。颜色296对、67份Markdown本地链接、diff及36份变更/新增文本隐私模式扫描通过。日志ui22-final-gate-debug.log、ui22-final-gate-release.log、ui22-final-gate-lint.log、ui22-final-verified-native.log及对应截图均在忽略build目录。当前可运行Mac Release开发构建位于build/macos-release/bin/daymark.app，仍依赖本机Kit。

过程FAIL均保留：Cocoa备注滚轮事件时间戳缺失及原生动画尚未呈现就检查位移，补单调时间戳并等待两次真实frameSwapped；保留原位移/选择/焦点及45秒普通/120秒截图总超时，不重写滚动、不提高阈值。一次Release15/16的长按取消失败，复查发现setSearch没通知QML排序许可，已补即时stateChanged且保留250ms列表防抖；新增真实QQmlComponent绑定回归，最终全测覆盖。一次原生页面几何读到布局前状态，验证前先polish并保留具体几何诊断/失败截图，不放宽边界。草稿场景改为实际可见新建和页面离开，放弃后退出编辑，避免显示已放弃表单。原生日志另有IMK及QUnifiedTimer诊断，不能将QML warnings=false写成全运行时零警告。Windows当前源码、物理输入/读屏/IME、真实通知/穿透恢复、多屏睡眠/8h、独立部署/签名仍UNVERIFIED。

### UI-21：导航目的地与主内容页面（IMPLEMENTED / AUTO-PASS；macOS原生合成通过）

用户要求先分析截图中的弹窗及入口存在理由，再系统修改。按[设计](design/navigation-and-pages.md)落实FR-006/009/014/015/018/019/021、UX-01/02/05/07/09/11/14及HIG Sidebars/Modality：重复任务/循环日程合并发现入口并进入主内容区，提醒历史移到工具栏，组织管理变页面，创建/管理分离。保留不同重复业务、所有能力和安全确认，不改schema与查询语义。分片构建回归，重点统一导航草稿保护、选中/返回、日历入口、隐藏任务工具、窄窗/200%与键盘；自动库均临时目录，保留此前macOS构建改动，不操作真实用户库/旧进程、不推送。当前Mac可执行arm64 Debug构建/CTest；Windows和物理交互门禁独立记录。

用户追加要求重新审视领域/项目/标签及软件哲学。已先补[产品哲学](product-philosophy.md)：收集→澄清→安排→执行→回顾为主线，归属（领域/项目）、横向筛选（标签）、时间/状态为不同维度，均不强迫快速录入先分类。继续据此分开侧栏归属与标签、呈现领域下项目层级、管理页按概念组织与上下文项目分组；保留PRD的侧栏标签目的地和当前视图标签筛选，不删除能力或改数据关系。

上述代码已实施。最终macOS Debug16/16（45.01秒）、Release16/16（44.33秒），两项QML lint无诊断；Release Cocoa原生309阶段全截图退出0、双窗口/warnings=false，菜单命令2.95512ms，两次呈现上界11ms。已查看归属层级/标签分区、标签整理及窄窗深色200%页面。自动数据全部临时库，截图未含真实任务。新增维护延后及导航投影Qt Test；保留原271阶段与草稿/滚动/长按断言。中途单参数QML元调用、维护打断长按、空态拉伸/任务页尾泄漏、原生光标回调拉回手动滚动均按原断言修正；最终源码以上述完整回归覆盖，不隐去初次失败。详细日志与范围见[验证记录](verification-report.md)。

200%截图也暴露了常驻概念解释、重复标题/创建占用过多空间，故按用户“简约而不简单”继续UI-22减负，不能据自动通过声称全UI体验已完成。Windows当前源码、物理输入/IME/读屏、多屏睡眠/8h、独立部署/签名仍UNVERIFIED。

### MAC-BUILD-01：macOS arm64 首次本机构建（DONE：Debug构建与自动验证）

2026-10-24，用户要求构建。范围为 Qt 6.10.2 / Apple Clang 的 macos-debug 构建、CTest 及 QML 静态检查；对应工程基线 F-01、M0-01/02 与 NFR 跨平台约束，不增加产品功能。环境为 macOS15.8.1 / Apple M3 Pro、Xcode15.4 / Apple Clang15.0.0、CMake3.31.10 / Ninja1.13.0。最初PATH缺构建工具，借助相邻项目已有安装器只读下载安装Qt6.10.2的qtbase/qtdeclarative/qtsvg/qttools/qttranslations；另在本仓库安装独立Python构建工具环境。Kit和工具均位于忽略的build/toolchain，不改相邻项目，不提交本机绝对路径。

按仓库入口完成Debug构建。首轮14/16，single_instance与qml_startup失败；实际Unix socket探针显示默认临时目录加完整摘要端点为121字节，超过macOS的sun_path[104]限制，导致开库前失败。仅macOS将端点摘要缩至128位，保留默认用户临时目录、用户权限、QLockFile及Windows端点规则；新增独立数据目录双主实例回归，既有真实第二进程激活继续验证。没有通过改TMPDIR或放宽断言规避问题。

最终Debug CTest **16/16 PASS（46.09秒）**，其中QML合成流程38.01秒，最终双窗口/warnings=false；两项QML lint无诊断，版本输出1.0.0-rc.2，file确认arm64。Windows专用package_validation不在macOS注册，因此不是少跑一项。10k混合数据加载/搜索/月索引113/4/23ms，菜单50对查询均值695.962μs。编译有1项既有未使用lambda捕获和12项Qt生成代码未使用参数警告，未压制。日志在build/macos-debug-build.log、build/macos-debug-build-retry.log和build/macos-debug-qmllint.log。

产物为build/macos-debug/bin/daymark.app；本机复建先 `source build/toolchain/env.sh`，再 `bash scripts/build-macos.sh macos-debug`。全部自动测试使用临时数据，未启动真实用户库，未推送或发布。只验证链接本机Kit的Debug构建；Release、独立部署包、原生交互/截图、Intel、签名、公证与完整平台门禁保持UNVERIFIED。恢复入口与具体命令见[环境](environment.md)及[验证报告](verification-report.md)。

### UI-20 / CAL-06A：连续日程与全局交互修订（IMPLEMENTED / AUTO-PASS；原生完整验收UNVERIFIED）

按[设计](design/calendar-visual-refresh.md)完成FR-002/007/010/014/019～022、UX-01/02/04～14：跨日连续色条/分周泳道与溢出、独立循环日程入口和实际年度实例/时间预览；任务长按/手柄拖动及Esc/Ctrl替代；关注三段单选、直接可空日期、提醒单层时分一次原子提交；侧栏三个分区独立折叠/持久化；清单拖动/取消/统一保存；分组记录/结束动作和校验错误范围。全局浅深配色、圆角、字号/焦点线统一。无schema/依赖/云变更，单代理，真实任务数据库未操作。

最终Release17/17（54.79秒），包含Windows系统字体目录及Segoe UI的离屏布局；271阶段Release全截图离屏流程PASS，双窗口/warnings=false，已查看正常中文字体的折叠侧栏、清单拖动、记录/结束分组和精简提醒。Debug完整17/17曾通过（60.21秒），后续QML专项40.70秒通过；最终代码另由Release完整回归覆盖。两项QML lint无诊断，颜色296对PASS（普通文字最低4.617:1、必要边界最低3.369:1），文档链接检查结果见本轮最终检查。离屏无系统字体时的方框/异常字体截图不作为视觉通过证据；测试脚本注入系统字体目录，smoke离屏仅使用Segoe UI，正常原生应用保持OS字体。

早期Windows原生截图已查看连续跨周色条、年度/定时日程、浅色有序详情及深色200%。后续完整原生流程未完成：出现剪贴板占用提示、首次布局/视口断言和菜单呈现信号等待；已补可见菜单求高/布局、受控分段绑定/放弃重开、列表高度变化后揭示及有界渲染准备。保留原几何断言，菜单两次呈现100ms预算不放宽，探针临时驱动两帧后停止；271阶段全截图总watchdog为120s，普通45s。用户确认电脑熄屏且暂不能操作，原生呈现/跟手度保持UNVERIFIED，不把离屏结果当作真实桌面或物理输入通过，不重复尝试此门禁。

本轮不更新已验证的UI-19正式本地候选包，不创建GitHub Release。当前代码可构建、可离屏回归；真实键鼠/IME/剪贴板/读屏、亮屏后的原生呈现、多屏/睡眠/8h/macOS及签名许可保持UNVERIFIED。

### SOURCE-PUBLISH：隐私清理与GitHub提交（DONE）

用户已明确授权源码提交到GitHub。确认远端初始为空后，已先创建并验证仅保留在忽略build目录中的完整恢复bundle，再将50个提交的作者/提交者及本地标签署名改为项目身份，日期归一UTC；匿名化前后源码树完全相同。当前文件、历史提交内容及795个历史blob扫描未发现个人路径/凭据/大型二进制，补充环境文件/证书/bundle忽略规则。既有Codex内部快照保留，不上传；原始历史仅本地恢复备份使用。

通过既有SSH认证将源码main推送到GitHub，首个公开实现提交`bc53a333d674368597d60b1814b7f2368b5d1461`，已读取远端HEAD比对一致，确认未上传本地标签；不创建Release，不上传build/dist、测试数据、日志、SDK或备份。另从远端独立克隆复核：50个提交均为匿名项目身份，218个源码/文档/资源文件，不含构建产物、数据库、日志或恢复bundle。其他工作未纳入本轮的架构提案保留未提交、不修改其内容。旧验证记录的源码散列属于匿名化前历史，作为原始证据保留，不篡改为新散列。


### UI-19：原行展开、整理与即时反馈（DONE：Windows本地候选范围）

按[设计](design/inline-task-experience.md)先映射HIG及用户四图反馈，再实现原行轻展开/核心图标、字号层级、整理及就近新建、详情动作、完成反馈、菜单响应和侧栏架构。FR-002/006/007/008/014/018，UX-01/04～11/14；单代理，无schema/迁移/外部依赖，不动真实库/旧进程，不推送。

原TaskRow展开为14DIP标题、12DIP一行备注及日期/截止/标签/项目/提醒五个圆形图标；信息图标继续同一草稿。多行备注显示只读摘要，完整内容在详情编辑。详情固定左返回/右保存、末尾文字危险动作；“整理”解释可选项目/领域/标签，CatalogService成功返回真实ID后选入草稿，取消任务不删除独立创建的资料。侧栏规划/资料库具名，底部固定设置及数据备份。主窗菜单Popup.Item，小窗Popup.Window；移动可用性复用当前C++模型，不重排全任务，已激活项不显示无效果激活动作。成功完成才110ms勾选＋80ms收拢，减少动态效果可关闭。共享浅深/增强对比色、线条、圆角及图形绘制尺寸统一。

最终Debug17/17（45.36秒）、两项QML lint无诊断；颜色288对PASS（普通文字最低4.608:1、必要边界最低3.261:1），63文件文档链接PASS。Windows原生235阶段全截图PASS：退出0/双窗口/warnings=false/无未完成创建。已查看五个圆形核心图标、完整原行展开、整理新建、完成勾选、同窗菜单及深色200%摘要/焦点；用例覆盖草稿转移、取消零修改、新建资料独立持久化及原子任务保存、减少动态效果。10k任务50次上下可用性查询对均值680.924μs；最终菜单命令1.4643ms，渲染线程两次frameSwapped上界9ms。上述不是物理输入端到端或全设备承诺。

失败与修复：offscreen大字日期焦点越界和首图展开底部裁切，均保留几何断言，改为编辑时关闭ListView自动跟随高亮、渲染稳定后整卡/焦点揭示，手动滚动优先；Theme遗漏pressed转发由lint捕获后修复。直接ctest缺Kit PATH失败后改规范入口。误并行构建/原生探针导致EXE锁定及277ms，串行仍229ms，进一步定位PNG读回阻塞UI计时回调；现渲染信号线程计时/原子传回，采集后截图，未放宽两次呈现100ms门槛。早期0.545ms/25ms含旧UI队列计时，以最终结果为准。

最终源码011d0f2711608d0cd9a9f1e5cafd6427a0279c63冻结后严格Release17/17（49.25秒），SDK隔离235阶段PASS：退出0/双窗口/warnings=false/无未完成创建；包内菜单命令0.5069ms、渲染信号两次呈现上界5ms。33个运行库来源、实际EXE/Qt6.10.2/schema4元数据、1475文件清单、ZIP逐项及SHA侧车PASS，报告b50eb26ebaae48b3b87c35573e7d119a保留于忽略build。包运行探针报告userDataAccessed=false；真实库和旧进程不动，未推送/上传。

本地包`Daymark-1.0.0-rc.2-windows-x64-local-20261024-144610.zip`，45371756字节，SHA256 `ad744f7872b2501e635b73f4fae0f8eafe4d166fea0654b559bafdd08a9f695d`。包内文档对应冻结源码，最终交付证据追加于本轮文档提交。启动新包前先从托盘退出旧版。本轮仍Windows rc.2本地候选；真实键鼠/IME/读屏/物理DPI/多屏/睡眠/8h/独立干净机/macOS与签名许可UNVERIFIED。

### UI-18：就近编辑与渐进呈现（DONE：Windows技术验证；快速卡形式由UI-19修订）

按[设计](design/direct-editing.md)从三张参考图提取快速编辑、日期开关/同组日历、日期快捷菜单、时间选择、当前值勾选与明确放弃保护，扩展共享组件和适用页面。FR-002/007/014/018/019/021、UX-01/04～07/09～11；保持单一草稿、C++时间语义/CAS、单轴及固定动作。风险为快速→完整草稿转移、弹层Esc优先级、内联日历自然布局和200%面板尺寸。先共享控件纵切构建，再快速编辑与事件迁移、全回归/原生截图。无schema/依赖变化，单代理，不动真实库/旧进程，不推送。

实现Main同一草稿映射的非模态快速编辑卡（标题/自然增长备注/两日期），完整详情不重载，关闭/切页/主窗退出共用三选草稿保护。共享CalendarPicker供内联/弹层复用，日期图标快捷菜单含当前勾选，DateField手输和键盘保留；开关关闭清除并隐藏空日期框，同组最多一个内联日历。新增TimeField和ReminderField，选择面板确认才更新输入；事件重复/提醒同行，全部ComboBox标示当前值。六类表单统一灰画布/白组/16圆角；规则启用/领域归档用开关，清单/多选保留勾选；事件/组织/规则的放弃改为明确文字和危险色。无服务/schema/SQL改动。

构建与最终两项QML lint无诊断，颜色260对及62文件文档链接PASS。Windows原生219阶段全截图退出0/双窗口/warnings=false且无未完成创建，已查看快速卡、内联日历、独立菜单窗口勾选、事件时分、浅色分组/设置和深色200%固定动作。最终Debug17/17 CTest通过（42.70秒）；严格本地包待冻结执行。新增路径覆盖快速→完整原草稿、关闭取消/放弃零修改、快速保存、只改开始日期/开关清除、提醒日期时分、取消不改非法手输和事件明确放弃。原单轴/短备注滚轮/CAS/关系回归保留。真实IME/读屏/macOS仍UNVERIFIED。

首次快速卡原生尺寸检查FAIL，发现Layout拥有的implicitHeight不能用父布局绑定替代，正文被压为0；改为正文提供自然implicitHeight和最小视口，保留断言复验PASS。首次新增组件存在QML分号语法/attached Window静态访问诊断，已修正并最终lint无警告；未压制日志。Qt工具偶有非阻断本机许可服务版本诊断，未修改SDK或绕过检查。早期原生202→218阶段及219阶段以最后结果为准；最终交付记录追加于冻结后证据提交。

首包ee89562严格Release17/17（43.47秒）及SDK隔离219阶段、清单/ZIP校验通过。最后追加长备注200%键盘焦点用例发现日期位于正文视口外（y1891、视口418），原qml_startup专项FAIL；已补两日期获得焦点时的单轴揭示，保留原几何断言，扩展为223阶段；最新Debug17/17（42.51秒）及两项lint无诊断通过，待223阶段原生截图和重新冻结包。本轮以最终包为交付，不把首包当成追加验收已通过。

最终源码5270bd56fd455801d259579c16f941c7dcf022cc：Debug17/17（42.51秒）、Release17/17（42.93秒），原生223阶段全截图与SDK隔离223阶段PASS，退出0/双窗口/warnings=false/无未完成创建；已查看长备注200%日期焦点与固定操作栏。33模块/1474文件/实际Qt6.10.2与schema4、ZIP/SHA均PASS，报告9712e6aeb501491ab87a2f3df1f0c711。本地包Daymark-1.0.0-rc.2-windows-x64-local-20261024-113239.zip，45302415字节，SHA256 5036305b750dadcc9ff65e755d5c764dcbf41f773346da054428e18ca59845f3。用户后续明确要求原任务行展开，进入UI-19修订，技术PASS不视为用户体验已获认可。未推送，真实库/旧进程不动，平台人工门禁不变。

### UI-17：iOS分组、圆角和开关（DONE：Windows本地候选范围）

先按[设计](design/ios-grouped-details.md)提取圆角层级、灰底白组、图标色底、滑动开关和同行关系，再实现任务详情和设置。移除顶部无单击行为的拖放伪按钮，保留真实菜单分组和拖放排序；不更改TaskPatch/服务/schema。验证构建、CTest/lint、原生浅深与200%及本地包；真实用户库/进程不动，单代理不推送。

实现灰画布/浅色白内容组/深色分组表面，标题备注同组，详情组16/控件8/选中10圆角；开关胶囊以滑块位置和成功色表达状态，图标按今天/月亮/对比/列表/置顶/穿透/锁定/通知等语义原创绘制。今天/今晚和设置11项布尔值统一SettingSwitch，清单/标签仍勾选；日期/归属同行及单草稿/单轴不变。顶部拖放Label已移除，真实任务菜单“安排到今天/今晚”仍接原命令。

Debug17/17（37.93秒）、两项QML lint无诊断、260对颜色检查及61文件链接检查通过；原生202阶段及浅深/200%截图PASS：退出0/双窗口/warnings=false/无未完成创建，已查看任务内容组、开关同行、深色大字与设置开关。后续仅补月亮和设置语义图形；最终Release/部署与最终图形截图待冻结验证。无schema/依赖/服务变化，真实拖放/读屏/macOS未验证。

最终源码f9d039effe27bd097ad9e099b91112f073e926cb冻结后Release17/17（39.12秒），最终图形Debug构建/两项lint与202阶段全截图PASS，退出0/双窗口/warnings=false且无未完成创建。已复核月亮图形、浅色圆角组/两开关同行、深色200%和设置布局；SDK隔离202阶段、33个实际运行库来源、EXE/Qt6.10.2/schema4、1473文件清单/ZIP/SHA侧车均PASS。报告ffbd8ec279e8462ead94f1f6700cab9a在忽略build。

本地包`Daymark-1.0.0-rc.2-windows-x64-local-20261024-105830.zip`，45207723字节，SHA256 `eec3f8efb4f04e201433408252e3772dbfaae9f813657ab7cbde21a430230f41`。保持rc.2本地候选，未远端发布；启动前退出旧托盘程序。未操作真实库/旧进程。系统读屏/物理拖放/多屏/DPI/macOS与签名许可门禁仍UNVERIFIED。包内文档对应冻结源码，最终部署证据追加于当前文档提交。

### UI-16：任务详情的信息架构与行布局（DONE：Windows本地候选范围）

按[任务详情布局设计](design/task-detail-layout.md)实施FR-002/006/007/008/009/014/018、UX-01/04～07/09～11。先定义同行关系与显示条件，再实现：紧凑内容、同组安排/今晚/提醒、互斥归属单选＋条件分组、标签同行、直接步骤与按内容出现的关联、页尾只读信息。替代逐字段等权卡片，保留单草稿/单轴/固定保存和原子事务。风险是归属/分组切换数据保持、移动今晚/提醒后的保存契约、焦点追随与窄栏/200%重排。单代理，不改schema/服务/真实库/旧进程，不推送；实际结果待构建/CTest/lint/原生/本地包验证。

实现采用DetailSection连续表面/区段线，空备注64→40DIP自然增长；安排同行、两日期并排、提醒归入安排；归属单选按kind/id映射，仅适用分组显示；标签同行、步骤直接添加、关联按内容及只读信息显式展开。单个TaskPatch仍经原saveTaskWithDetails，不改C++查询/服务/SQL。安排今晚作为显式意图同时激活/关注今天，取消关注仍用todayAfterRemovingFocus的C++判断，保留日期确认取消时的今晚草稿。

最终Debug17/17（38.38秒）、应用/样式lint无QML诊断、颜色260对及check-docs60文件PASS。原生202阶段全截图PASS（退出0、双窗口、warnings=false、无未完成创建），包含项目＋分组实际保存/重开→领域互斥清空、非法提醒失败保留旧归属、今晚/本地提醒提交和清除、展开信息跨缩放保持、宽页同行/窄栏及200%无横向裁切、无日期任务移除关注不弹多余确认。已查看宽页、项目分组、深色200%、窄栏截图，既有草稿/单轴/短备注滚轮回归保留。严格Release包结果如下。

源码dfec4251f573fe30f1977e55a7ac68f34cd90884冻结后严格Release重新构建17/17（40.62秒），SDK隔离202阶段PASS：退出0、双窗口、warnings=false且无未完成创建；33个运行库来源、实际EXE/Qt6.10.2/schema4元数据、1472文件清单、ZIP逐项及SHA侧车均PASS。报告2870dfd512954e8ea2755c97527bdf43保留于忽略build。本地包`Daymark-1.0.0-rc.2-windows-x64-local-20261024-104050.zip`，45165023字节，SHA256 `36df8fc7b4f5aaeee34877742c5bca051f18c6227aff0bfab0cfc5e7329c124e`。仍为rc.2本地候选，无远端发布；包内文档归属冻结源码，最终校验记录在本轮证据提交。运行新包前退出既有托盘程序；未操作真实库或旧进程。

首次专项FAIL定位到未进入今天的任务evening按既有业务规范化为false，随后先修订交互设计为“安排今晚”显式激活/关注，再验证实际点击与落库；保留规则/断言，未改变QueryService。审查中保留取消关注的原C++日期判断并追加无日期确认回归。初次Qt元对象工具输出非阻断许可服务版本诊断，未改SDK或绕过检查；后续最终lint与构建正常退出。macOS、物理IME/读屏/DPI与完整平台门禁仍UNVERIFIED，Windows布局证据不替代上述实测。

### UI-15：全界面动作图标与语义配色（DONE：Windows本地候选范围）

按[动作审计和配色设计](design/actions-and-color.md)覆盖全部UI入口：常见/低风险动作仅图标，进入创建/编辑/数据操作图文，确认/取消/复杂范围保留文字；中性表面＋少量蓝强调，浅深角色成对，今日和危险色分离。覆盖FR-001/002/007/010/012/013/017/018/019/020/022、UX-01/03～07/09～11/13/14。先共享组件/色彩、再全页迁移、最后构建/对比/原生/严格包；不改schema/业务/真实库/旧进程，单代理，不推送。风险为图标识别、纯图标名称、200%宽度和新配色所有前景背景对比。

实现与本地验收已完成：ActionButton/IconToolButton集中图文/名称/键盘提示，原创新增编辑/刷新/窗口/数据/撤销/显示等图形；主窗、日历、详情、组织、规则、数据和设置按表迁移。语义token覆盖浅深与显式增强对比度，今日不再复用danger；透明度偏好保留，增强时实色并禁用调节，有解释文字。图标原命令/草稿/查询契约保留，未改变软件文件标识。

最终Debug构建、17/17 CTest（36.70秒）、应用/样式lint无诊断；颜色脚本260对PASS，普通文字最低4.586:1、必要边界3.074:1（以原始值判定）；check-docs 59文件PASS。原生191阶段全截图PASS：退出0、双窗口、warnings=false，无未完成创建；新增纯图标添加并清空、增强开关/实色和透明度恢复，既有日期/菜单/草稿/单轴滚动链保留。已查看浅深动作、增强200%、详情、日历与420×480独立200%底部动作等合成截图。首次CMake在修改清单期间构建未包含IconToolButton，qml_startup FAIL；重新生成稳定清单后修正并全测；初lint未使用import已移除。严格Release包结果如下。

源码cead7b05aa94388747116862d110ae305658f40f冻结后严格重新构建Release17/17（39.82秒），SDK隔离191阶段PASS（退出0、双窗口、warnings=false、无未完成创建）；32个实际运行库来源/EXE与Qt6.10.2/schema4元数据/1471文件清单/ZIP逐条及SHA侧车均PASS。报告bd61df7ffbea48f297fb3480584c8944保留于忽略build。最终包`Daymark-1.0.0-rc.2-windows-x64-local-20261024-091143.zip`，45136863字节，SHA256 `e89e635b769b4c092f34d40d0f2b6450754dd14a3042a13920d4996b9784b5f5`。仍为rc.2本地候选，未远端发布；包内文档归属冻结源码，最终校验记录在本轮文档提交。运行新包前先退出既有托盘进程。

macOS、真实读屏/IME/物理DPI/多屏/睡眠/8h和系统高对比自动跟随未验证；本轮增强对比度为应用显式偏好。透明组件任意壁纸对比不作PASS，增强模式使用实色。无外部依赖/schema变化，不动真实库或旧进程，不推送/上传。

### CAL-05：系统评估与日程浏览定位（DONE：Windows本地候选范围）

先完成[RC2系统评估](review-rc2.md)及分级路线，再按[浏览定位设计](design/calendar-browsing.md)实施FR-022，覆盖FR-010/019/020/021和UX-01/02/05/06/07/09/10/11。切片为实例映射/排序与测试→只读预览及明确编辑范围→直接跳转日期→构建/原生/严格本地部署。无schema变化，固定Qt6.10.2，单代理；不操作真实库/旧进程，不推送。风险为跨日/循环实例定位、双窗来源和非模态预览退出/200%布局；平台门禁独立保留。

领域与界面已集成：同日事件按全天/实例UTC/标题/ID稳定排序；只读预览精确校验定位键，明确实例/系列、循环/提醒，失效/删除显示空态；显式编辑进入原CAS表单，支持主/独立窗直接跳日。新增临时库测试覆盖旧锚点排序、后续实例、只读不写版本、无效key与改期/删除失效。Debug17/17（35.18秒），两项lint无警告；原生180阶段与默认/深色200%截图PASS，退出0/双窗/warnings=false且无未完成创建。最终独立窄窗与严格部署证据如下。

首轮QML弹层放在ColumnLayout下触发布局静态警告，已按既有弹层放置到ScrollView的非布局上下文；未压制lint。首轮原生失败在原备注滚轮探针，焦点改变同一回调立即投递事件，现等待渲染/焦点稳定后投递并保留原位移、短备注和下拉不改值断言；新诊断显示事件接受且正文发生位移。没有据此宣称物理输入设备已验收。截图使用合成数据，用户库/旧进程不动，无schema变化或推送。

最终源码b5ee06bf3217a9653b0259088279f59156b76c7e（主体7d3bb5e，b5ee06b补明确420×480而非沿用旧大窗口的验收断言）。最新Debug17/17（35.43秒），最终严格包重新构建Release17/17（35.90秒）；两项lint无警告。SDK隔离与最终Release全截图180阶段均PASS，退出0/双窗口/warnings=false，无未完成创建；已查看主窗默认/深色200%及真实420×480逻辑尺寸独立窗200%预览，正文单滚动且固定关闭/编辑动作可见。窗口PNG为630×720物理像素，不混淆DPI与文字放大。

最终包Daymark-1.0.0-rc.2-windows-x64-local-20261024-000729.zip，45085523字节，SHA256：6358eca750fa7b4565f1b5f49805bee33b98f889fd45820fd2643b70bb9777cc。1470文件清单、实际EXE版本/Qt6.10.2/schema4元数据、33个运行库来源、ZIP逐条和SHA侧车PASS，报告a3c5d143175b429da785118533ef29ae保留于忽略的build。首轮000434包也通过，但窄窗阶段未固定最低尺寸，最终以000729为交付。

当前适合Windows本地候选试用；macOS、真实通知送达、物理IME/读屏/DPI/多屏/睡眠/8h、独立干净机与分发许可/签名仍UNVERIFIED。未更改schema4、真实库或旧进程，未推送/上传；运行新包前退出旧托盘进程。下一计划为CAL-06查找/日程密度，按评估路线先定义范围与性能预算；其余路线不宣称已经运行或完成。

### CAL-04：月历精修、循环事件及提醒（DONE：Windows本地候选范围）

按design/event-series.md推进FR-021；先领域/schema4/JSON兼容与去重测试，再月格色条/今日圆标及事件表单和提醒中心，最后构建/原生/严格部署。范围为系列编辑/删除，无单次例外；无后台服务，不动真实库/旧进程，不推送。

领域/schema4/JSON与表单/提醒中心已集成；Repository集中SQL，EventService集中规则/时间计算。最新Debug17/17（32.62秒），两项QML lint通过，最新Release构建成功（此前领域重构Release17/17：33.62秒）。最终原生172阶段与全截图PASS：实际保存每周/提前5分钟并重开保持，退出0、双窗口、warnings=false，无未完成创建；已查看默认/深色200%循环表单、月格事件色条和独立日历截图，系列分组可见断言通过。最终严格部署记录如下；真实系统通知送达不作PASS。

中途DST间隙测试FAIL，已先校验当地时间再toUTC并保留原用例复验；全天提醒导出统一UTC已覆盖。原生短备注滚轮曾被布局/光标回调吸回，现手动滚动暂停自动定位，焦点/编辑恢复，原断言保留并通过。截图定位读取字号变动前旧几何的错误，经有界渲染等待及可见断言修正；普通45秒/截图90秒预算未放宽。全部测试临时库，真实库与既有进程未操作，无推送/上传。最终部署与限制如下。

最终源码617d4f08814f9918b258757292ed17229c926f91（主体bfb7dc7，设置提醒分组名称同步617d4f0）。严格打包重新构建Release17/17（32.98秒），SDK隔离172阶段PASS，退出0、双窗口、warnings=false且无未完成创建诊断；33个部署运行库来源、实际EXE版本/Qt6.10.2/schema4元数据、1468文件清单、ZIP逐条及SHA侧车均PASS。报告0d2239a7ed76419f90fcd3e6921daf37保留于忽略的build。

本地包Daymark-1.0.0-rc.2-windows-x64-local-20261023-232556.zip，45043071字节，SHA256：60386aa7c2cc6e29eff152a9119b4dda3650c90206b32ff1fc41fc7e00620efc。程序仍1.0.0-rc.2，未远端发布。规则/提醒及去重历史由schema004保存，schema1–3升级前自动一致快照；不覆盖历史迁移，JSON1–4兼容。首次功能包232311也通过，最终232556额外校正设置分组为“任务与事件提醒”，以最终包为交付。

范围：系列级编辑/删除，没有单次例外；应用运行时检查，退出无后台服务，全天基准09:00。macOS、真实OS通知送达、物理IME/读屏/DPI/多屏/睡眠/8h、独立干净机及公开分发许可/签名仍UNVERIFIED。没有操作真实库或旧进程，运行新包前先从托盘退出旧版；旧程序不能打开schema4，回退须独立目录使用升级前快照。

### CAL-03：日历精修与独立桌面日历（DONE：Windows本地候选范围）

FR-020/010/012/018/019、UX-01/02/05/06/09/10/11/13，设计见[独立日历](design/desktop-calendar.md)。新增独立锚点/月周/选日的桌面窗口，和主窗/任务小窗/Mini同时存在；共用CalendarService/EventService查询映射和写命令，无第二数据库。背景alpha50–100%、独立置顶、隐藏/恢复、拖动/右下角缩放、几何与显示偏好持久化；主窗设置/日历有恢复入口，事件草稿保护覆盖隐藏/关闭。月格周末层级、周行细线和选中字重精修，任务编辑回主窗并保留草稿保护。

最新Debug17/17（30.59秒），两项lint通过；严格打包Release17/17（32.04秒）。原生167阶段及最终Release全截图复核PASS，三窗并存/独立翻页/单独隐藏断言通过，退出0、最终双窗口、warnings=false，无未完成创建诊断。查看默认/200%独立日历截图；背景RGBA采样179/255符合70%设置。新增同源查询/闰日/偏好范围与重启、独立几何命名空间测试通过。初次跨窗口同名控件定位失败已按主面板限定查找并保留原断言；测试helper翻页后恢复选日已修正，最终截图议程与选日一致。

源码e2a3c8277d641b64c6bb2a6718d5c8d5931b0f86，包Daymark-1.0.0-rc.2-windows-x64-local-20261023-214427.zip，44990300字节，SHA256：b4458b60d5f19a178e4b4b588c05e222dcfa4ee9657a181ae127c5801c75f213。严格SDK隔离、DLL来源审计、实际版本/Qt6.10.2/schema3、1466文件清单和ZIP逐文件/侧车校验PASS，报告3bd379fc保留于忽略的build。

自动数据QTemporaryDir，无schema变化、未触碰真实库/旧进程、不推送。macOS、物理拖动/缩放/多屏/DPI/桌面合成及遮挡层级、IME/读屏/8h/独立干净机与许可签名仍UNVERIFIED；本轮不将PNG alpha或窗口flag当作完整OS视觉验收。运行新包前退出旧版托盘进程。
### UI-14：Apple HIG全界面重设计（DONE：Windows本地候选范围）

按[全界面设计](design/hig-redesign.md)覆盖全部既有入口，FR-001～019、UX-01～14。三切片完成：共享分组/分段/空态，任务/规划/资料库导航及独立三类设置；任务、组织、重复、事件、批量、数据页统一标签/分组/日期与固定提交区；日历议程、提醒中心、Floating/Mini和共享控件同步精修。系统字体、字号、留白、圆角、图标/线条与选中/焦点表达统一。保留所有旧设置、原子保存和恢复路径；规则新建/切换/退出统一草稿保护，详情准确显示未保存状态。

最终Debug17/17（29.45秒），两项QML lint通过；最终严格打包重新构建Release17/17（31.39秒）。连续3次Release原生及最新全截图162阶段均PASS；SDK隔离的最终包再次PASS：退出0、双窗口、warnings=false，无未完成创建/DelegateModel取消诊断，33个部署运行库模块来源审计通过。清单1465文件、实际程序版本/Qt6.10.2/schema3、最终ZIP逐条内容与SHA侧车全部校验通过，报告443bfe63保留于忽略的build。新源码普通探针仍45秒，全截图单独90秒有界预算；未弱化业务/滚轮/焦点断言。

实现提交5d27e52及修正433ac102eb990ac2b4a83ffe4f4f8c0e84199c80。最终本地包Daymark-1.0.0-rc.2-windows-x64-local-20261023-205422.zip，44960270字节，SHA256：b236aa2a4aec704b991981c49120393c0f1eff52df6a7c8dc11eb66d58284812。源码快照Daymark-1.0.0-rc.2-ui14-source-20261023-205346.zip，485548字节，SHA256：104a69a978814e6b934a40c22c9194f122285001c59e4c7d09e9e70137c5efbe；敏感文件扫描0项。

首轮171729严格部署因未完成创建2项FAIL，未生成ZIP，失败目录及报告89301ddc保留，不能作为交付。经验证采用ListView cacheBuffer=0、下拉弹层独立非零视口，并取消手动滚动时的过期焦点定位；无Qt私有API/日志压制。其他中途点击定位、构建并发锁、滚轮与截图时限失败及修正完整见验证报告，不把源码环境PASS冒充部署PASS。

全部自动数据QTemporaryDir，未操作真实库/启动项/旧进程，无推送/上传；版本仍1.0.0-rc.2。macOS、真实IME/读屏/高对比/物理DPI、多屏/睡眠/8h、独立干净机、许可与签名仍UNVERIFIED；复杂10k委托滚动帧率未测。运行新包前从托盘退出旧版，避免激活已有旧实例。
### UI-13：视觉关系及月格事项（DONE：Windows本地候选范围）

按[视觉系统](design/visual-system.md)先定义字体/字号/布局/圆角/颜色/线框层级，再统一共享字段、按钮与详情文档表面；紧凑月格显示具体事项标题，不再仅计数。FR-007/010/018、UX-01/05/06/09/10/11/12。验证浅深、宽窄、200%、真实标题delegate/焦点/草稿/日期/滚轮，再Debug/Release/lint及原生截图；用户数据/旧进程不动，不推送。


用户追加：Apple HIG已升级为强制设计依据P-13，写入AGENTS、工程流程、UI基准、验收模板及调研；官方中文入口和同站英文Markdown已复核。具体字体/数值/线框仍由Daymark定义，平台习惯与实测边界保留。

实现：普通字段使用共享FieldSurface（浅填充/底线/焦点轮廓）；标题20 demi与备注文档式无框；页面/详情外框删除，仅停靠时左侧1DIP分隔；正文最多680×文字比例，宽页日期并列、窄/大字重排。平台系统字体、正文/标签/分组/页面字号和6/8/12圆角分工明确。月格紧凑布局显示实际标题、余项+N，完整提示/名称、点击和改期路径保留。

最新Debug17/17（28.43秒）、两项lint通过；Windows原生152阶段退出0、双窗口、warnings=false且无创建退出诊断，1000宽默认紧凑和200%真实标题/格内高度断言通过；截图查看浅深、详情宽窄/200%、日历标题、控件状态。此前Release17/17（29.82秒），最终包将重新构建测试最新探针。底线/填充浅深对比3.23/4.14，焦点/填充5.77/6.91，次级文字/填充5.11/7.03。中途新增标题校验缓存绑定缺少count依赖导致FAIL，已修正并原样复跑通过。真实触摸板/IME/读屏/macOS仍UNVERIFIED，不碰用户库/进程。

最终交付：源码f0392c6d49de70dae739a80938dde08c1234d469，严格打包重新构建Release及17/17 CTest（28.62秒），SDK隔离152阶段退出0、双窗口、warnings=false且无创建诊断；运行库来源审计、清单1464文件、最终ZIP逐条内容和SHA侧车校验通过。没有重新运行独立干净机或平台长时矩阵。

本地包Daymark-1.0.0-rc.2-windows-x64-local-20261023-161619.zip，44885877字节，SHA256：7829f453ad1ef69a15d5c7dea7acbfc5f7425458761bc9843ab1ffb0f3824597。源码快照Daymark-1.0.0-rc.2-ui13-source-20261023-161557.zip，468017字节，SHA256：578c7a88378a08a53e47115bd0be722cae716873a070eb01f13327df390e674e，敏感文件扫描0项。Qt6.10.2、schema3、版本仍1.0.0-rc.2；无推送/上传，保留旧包。运行新包前退出旧版托盘进程。

### UI-12：整体精修（DONE：Windows本地候选范围）

按[设计](design/ui-polish.md)统一密度、间距、图标比例/反馈和应用文件图标。FR-002/007/010/012/013/018、UX-01/04/05/09/10/11/13/14；先小批次修改共享样式及关键页，再Debug/Release、lint、原生截图和PE图标检查。保持单代理、真实库/旧进程不动；V-03W退出诊断仍单列待解决，不放宽发布门禁。


实现：32DIP起控件、20DIP页边、10DIP字段间距，紧凑模式字段8DIP；标题24DIP、详情默认344DIP，表单边界保留outline对比。任务元信息合并、Today不重复今日标记；完成圆固定18DIP、32DIP命中区，搜索/筛选中文提示图标。原创SVG及多尺寸PNG/ICO/ICNS；EXE、应用窗口/任务栏与托盘共用标识，未新增运行时依赖。

验证：Debug17/17（29.47秒）、Release17/17（29.90秒），两项QML lint通过。Windows原生152阶段退出0、双窗口、warnings=false，当前Debug无创建退出诊断；默认/880窄/1440宽、浅深/200%、详情/日历/复杂表单/小窗/Mini所检截图通过。实际Release PE图标提取非空，8个尺寸16/20/24/32/48/64/128/256全部存在；ICNS结构/7个PNG块验证通过，但macOS实机未验。正文/边界对比和辅助技术完整矩阵不因局部截图通过而整体标PASS。最终本地部署及中文/空格解压路径复验均通过；不触碰用户库/进程，不推送。

### UI-11：滚动与备注输入（DONE：Windows代码与自动验证；交付包待验）

FR-007/018、UX-05/09/11/12，设计见[滚动交互](design/scroll-interaction.md)。统一表单、列表与组件的滚动：边界停止，惯性速度1200DIP/s、减速度6000DIP/s²；内容未溢出则不可拖动/滚动，不显示多余滚动条。备注自然增高，仅外层正文滚动；焦点已可见不挪动，手动滚动不吸回。下拉框/数字框不因滚轮改值。

追加FR-012/013、UX-11/13：按用户最新决定，展开态小窗表头常驻置顶图钉，蓝色选中表示实际置顶；Mini只保留摘要与展开，继承同一状态。复用既有命令、错误反馈与持久化，未新增数据状态。

最新Debug17/17（28.39秒）、Release17/17（28.49秒），两项QML lint通过。152阶段包含短备注滚轮转交、焦点保持、短列表不可滚动、窄窗/200%及置顶按钮点击往返；Windows原生152阶段退出0、双窗口、warnings=false，默认/200%小窗及Mini截图检查通过，当前Debug退出未见创建诊断；Release部署仍按下节FAIL记录。物理触摸板/IME/读屏/macOS未验证。没有改动用户库或既有用户进程。

### V-03W：Windows离线交付验收（DONE：工程与本地部署；完整V-03门禁未完成）

M7-06/08、NFR-06/07/08/11/12，设计见[交付验收](design/windows-delivery.md)。只读目录/元数据/SHA/敏感文件校验、SDK隔离原生测试、DLL来源审计、ZIP逐文件/侧车校验、版本及干净工作区守卫已实现；合成篡改测试通过。唯一OS运行库例外为精确System32/D3DCompiler_47，其他Qt/MinGW来源限包内。

当前2c34932候选包：Release重建17/17（28.04秒）；严格SDK隔离原生152阶段通过，退出0、2窗口、warnings=false，日志无未完成创建诊断。最终ZIP逐条SHA/长度、清单1463文件及侧车校验通过。另将最终ZIP解压至新中文/空格路径，从包内verification脚本使用默认PackageDirectory独立复验，通过且PATH/QT_PLUGIN_PATH恢复正确。实际运行程序版本、Qt6.10.2和schema3元数据一致，无用户库访问。

本地包：Daymark-1.0.0-rc.2-windows-x64-local-20261023-155600.zip，44852604字节；SHA256：cf08e97c44bbb2e58222d114e744a0a635dcbe211c40562e4301bb1bee39d96e。源码修订2c349320082d49af8f6792e99cc2a9562467b813。源码快照Daymark-1.0.0-rc.2-ui12-source-20261023-155551.zip，459625字节，SHA256：d104ba66ea2f3e0fe19ba94508836f17b8dcc6017e9ff30f831b400acaa1b73e；敏感文件扫描0项。

早期7b5658c阶段153556严格验收FAIL（Qt未完成创建6项），以及123917/124722的失败记录保留，均不作为最终交付。当前两次原生部署干净退出，只证明此候选的实测结果，不宣称Qt生命周期根因已普遍修复。

独立干净机、许可、签名、完整8h/真实睡眠/多屏和macOS仍UNVERIFIED；无推送/上传，版本仍1.0.0-rc.2。运行新包前先从托盘退出旧版，避免激活旧实例。完整V-03/V-04不因工程工具通过而自动完成。

### V-02W：Windows稳定性与恢复验收基础（DONE：实现与短跑；完整V-02门禁未完成）

下一里程碑按M7/V-02推进，设计见design/windows-stability.md。补系统事件合并/忙碌期间维护补偿、主窗工作区变化恢复和同进程隔离长跑工具，覆盖FR-003/014/015/018及NFR-04/05/06/08/10。先实现测试再Debug/Release与短时原生实跑；完整8h、真实睡眠/多屏/干净机/macOS保持UNVERIFIED，不以短跑替代。默认单代理，不修改用户数据库、启动项或托盘进程，不推送。

实现：时间/设置/唤醒/激活事件合并排队；写入期间保留一次维护请求，结束后补做。后台维护不禁用文本表单、不清除校验错误，写入仍互斥。主窗工作区变化后可达，保持隐藏/最小化。新增只读SQLite完整性检查、同进程隔离业务循环和CPU/内存CSV与JSON报告；工具拒绝用户data-dir/覆盖已有报告。正文显式Flickable，布局完成后只在焦点几何变化时追随，单一滚动轴及200%保持。

验证：最终Debug 16/16（26.61秒）、Release 16/16（26.90秒），两个QML lint无警告；原生恢复测试6/6。原生148阶段启动、长备注/后台维护/宽窄/200%及菜单截图检查PASS，双窗口、warnings=false。Debug请求30秒实跑33.997秒、3循环/3完整性检查；Release请求60秒实跑67.383秒、6循环/6检查，退出0、无长暂停，最大事件循环延迟383ms。Release含启动的资源峰值：私有508,616,704字节、工作集496,939,008字节；这是活跃合成负载，尚不证明闲置CPU、150–250MB优化目标或无泄漏。10k混合Release加载/搜索/月索引129/1/8ms（i7-13700HX）。完整记录见verification-report.md；8h、真实睡眠/物理多屏/macOS仍UNVERIFIED。

本地包：Daymark-1.0.0-rc.2-windows-x64-local-20261023-120756.zip，44,756,544字节、manifest1457文件，源码修订30ff589。SHA256：a0a285432ee78e47f62833faed4fb4274b81805219e974a462bbef64a18c0153。源码快照Daymark-1.0.0-rc.2-v02w-source-20261023-120836.zip，330,489字节，SHA256：8ae64317ec832a5700494aa9129c713896ee37a3ca62b606f90dc5b99897c1eb。SDK隔离部署原生148阶段PASS，Qt/MinGW均来自包内；manifest逐文件校验、ZIP校验和与敏感扩展/日志/Git扫描通过。保留旧包/标签和旧用户进程，无推送/上传；运行新包前从托盘退出旧版，避免激活已有旧实例。

下一步：按[验收指南](stability-guide.md)执行8h和真实系统场景、记录闲置CPU/长时内存趋势并评估优化，补独立干净机；完整V-02/V-03/V-04不因本切片交付自动通过。


### UI-10：单一任务编辑面板（DONE：Windows本地候选范围）

针对菜单隐藏项留白和任务二级详情/多重滚动问题，按design/unified-task-editor.md实施。FR-002/007/008/009/014、UX-01/04/05/06/07/09/11；宽窗并排、窄窗主内容页，共用常驻字段、单一正文滚动、单一草稿和原子保存。测试重点：全字段成功/失败零修改、离开保护、滚动/焦点与200%适应。保留schema3，不触碰真实库、不推送。

实现：归属/标签/清单/提醒常驻同一Pane，删除二级详情；备注自然增高、正文单一ScrollView，固定标题/返回/保存和错误区。窄窗替换主内容而不弹窗；草稿含未按Enter的清单输入，一次TaskPatch/CAS/事务保存。新增viewSection模型投影避免隐藏列表时动态切换分组属性。Debug 14/14（25.74秒）、Release 14/14（25.57秒），两个QML lint无警告；Windows原生148阶段合成流程PASS，退出0、2窗口、warnings=false；默认/880窄窗/1440宽窗及200%下单一滚动正文、焦点字段可见、宽窄草稿保护、菜单无空行断言和截图通过。SDK隔离部署PASS，运行库均来自包内，原生148阶段smoke退出0、2窗口、无QML警告，真实IME/读屏/macOS仍UNVERIFIED。


本地包：Daymark-1.0.0-rc.2-windows-x64-local-20261023-113823.zip，44,728,085字节、manifest 1455文件，源码修订2d8f418，Qt6.10.2、schema3。SHA256：d60c9d63cf599832f2562b6a32ec31538cfac601777758f86c999f7672aa1cc0。独立源码快照Daymark-1.0.0-rc.2-ui10-source-20261023-113840.zip，313,989字节。两个ZIP敏感扩展/日志/Git元数据扫描0项；二进制SHA与侧车一致。保留旧包/标签和用户进程，没有推送或远程发布；运行新包前请从托盘退出旧程序，避免单实例激活旧版本。

### UI-09：详情与工具区精修（DONE：Windows本地候选范围）

用户四处截图反馈：精简详情/Today，日期输入集成可编辑日历，修复浮窗空菜单。需求FR-001/002/007/010/012，UX-01/03/04/05/06/08/09/11/13；设计见design/ui-detail-refinement.md。保留显式保存/脏草稿与业务日期校验，不改schema。已实施紧凑详情/渐进搜索筛选、更多菜单/按需拖放提示、可编辑DateField及组件独立菜单窗口。无效排序在C++统一判定并禁用，禁止搜索/筛选/跨组无效移动；日期手输/选择不抢焦点、不改另一日期。Debug 14/14（21.35秒）、Release 14/14（21.53秒），两个QML lint无警告；原生132阶段合成检查PASS（退出0、2窗口、warnings=false），默认/窄窗/200%日期、搜索和菜单截图检查通过；SDK隔离部署PASS，运行库均来自包内，退出0、2窗口、无QML警告。真实IME/读屏/macOS保持UNVERIFIED。


本地包：Daymark-1.0.0-rc.2-windows-x64-local-20261023-101439.zip，44,698,570字节，manifest 1454文件；源码修订8bd266d，Qt6.10.2、schema3。SHA256：7cc2d75c6369599115cee3db8bfa8ef7045edb0567f7697e1a025fac4aba151c。独立源码快照Daymark-1.0.0-rc.2-ui09-source-20261023-101510.zip，306,841字节。两个ZIP敏感扩展/日志/Git元数据扫描0项；二进制SHA与侧车一致。保留所有旧候选包/标签，没有推送或远程发布。

### CAL-01/02：任务议程与本地事件关联（DONE：Windows本地候选范围）

用户已授权完成UI-08提出的两个阶段：任务日历直接完成/响应式议程；本地独立事件及任务关联。需求FR-010/011增强、新增FR-019（本地事件）、UX-04/05/06/07/09/11。顺序：冻结事件日期/时区/关系契约 → schema003及服务/导入导出/迁移测试 → 当天议程/事件编辑/关联界面 → Windows Debug/Release、原生合成截图及本地包。不加入外部账户、同步、小时轴。风险：已交付schema2升级前必须WAL快照；事件起止与任务日期独立；删除保留另一方、失败零修改；200%议程与脏草稿保护。

实现：当天议程直接完成/恢复、类型显示过滤、日期键盘路径、宽窄/200%滚动及议程直达；独立EventDialog、本地全天/定时事件、准备/跟进多对多关联、双向进入、版本冲突/草稿保护、软删和8秒撤销。事件与任务改期/完成/删除相互独立。追加003及schema3 JSON/一致快照，旧1/2兼容并先备份升级；不覆盖001/002。版本1.0.0-rc.2。

最终Debug 14/14（19.56秒）、Release 14/14（20.51秒），两个QML lint无警告；114阶段原生合成检查退出0、2窗口、warnings=false，宽窄/浅深/200%截图通过所检布局。10k全天事件Release加载20ms、可见日期索引<1ms；完整证据见验证报告。SDK隔离部署PASS，运行库来自包内，退出0、2窗口、无QML警告。未触碰真实用户库，没有推送或远程发布；macOS与真实输入法/读屏/多屏/8h等仍UNVERIFIED。

本地包：Daymark-1.0.0-rc.2-windows-x64-local-20261023-094938.zip，44,665,532字节，清单1453文件。SHA256：2a06f85a47b5cb008d0f49c007c1861c879f3adbb15b53db04fd3f1f29cae7a4。源码修订e0aac5b，Qt6.10.2，schema3；源码快照Daymark-1.0.0-rc.2-source.zip，299,590字节。两个ZIP敏感扩展/日志/Git元数据扫描均0项，二进制ZIP重新计算SHA与侧车一致。保留旧包与标签，不移动RC1标签；升级/回退见RC2说明。
### UI-08：组件降干扰与日历关联再评估（DONE：Windows组件实现＋日历方案评估）

用户反馈组件按钮过大，并要求参考iOS日历/提醒事项重新评估日历与事件关系。需求FR-010/011/012/013、UX-01/04/06/09/11/13：本轮实施浮窗单行紧凑工具区、Mini图标展开；保留添加/隐藏/打开主窗/穿透外部恢复，保留键盘焦点和32DIP命中区。日历部分先输出官方来源、现状差距、任务/事件语义、分阶段设计，不擅自引入事件数据库或外部账户。

验证：Debug/Release构建及CTest、QML lint，Floating/Mini浅深/260宽/200%合成原生截图；真实鼠标/读屏/多屏仍UNVERIFIED。设计与评估见[组件与日历评估](design/widget-calendar-review.md)。

实现与评估已完成：浮窗两排大按钮收为一排图标工具区，隐藏/打开主窗保留在菜单；Mini图标展开。Debug 13/13（16.05秒）、Release 13/13（16.14秒）、两个QML lint目标通过，96阶段Windows原生合成smoke退出0、2窗口、warnings=false。260宽及200%按钮命中/列表可用空间/收起展开恢复断言和浅深截图通过。预览包SDK隔离部署PASS，运行库来自包内，退出0、2窗口、无QML警告。

本轮本地包：Daymark-1.0.0-rc.1-windows-x64-local-20261023-090503.zip，44,560,065字节，清单1451文件；源码修订005cc1c，版本仍为1.0.0-rc.1。SHA256：fbeddd710f981b66fd62915cd75f1202e934320af6848f9aded0fdd7fd548d13。ZIP敏感扩展/日志/Git扫描0项；保留旧包/标签，没有推送或远程发布。

日历评估结论：优先CAL-01当天议程/直接完成/响应式布局；独立事件与显式任务关联列为CAL-02，需先补产品范围、ADR及新迁移。当前没有新增事件模型、小时轴或外部账户；日历方案不是已交付功能。

### UI-07：全界面视觉统一（DONE：Windows本地实现及所列验证范围）

用户要求先统一所有布局、按钮等可视元素。本轮采用独立DaymarkStyle控件样式及共享设计token，覆盖主窗/详情/组织/重复/批量/数据/日期/日历/桌面组件；保留业务和草稿语义，不改schema。需求FR-001～018展示层、UX-01/02/04/05/06/07/09/10/11/13。

顺序：统一全局基础控件并构建回归 → 全页布局/动作层级迁移 → 浅深/宽窄/200%原生合成截图、Debug/Release/CTest/lint与本地预览包。真实读屏、IME、macOS和物理多屏仍UNVERIFIED。设计见[视觉统一规范](design/visual-unification.md)。

已实施：DaymarkStyle统一15类控件、DesignTokens、独立线性图标、固定且可换行的ActionBar、分区表单和滚动正文；覆盖全部现有QML功能页与Floating/Mini，保留原生系统对话框及托盘。输入框占位文字绘制、标准按钮中文、长菜单自适应宽度已补齐；组织/重复表单随文字与宽度切换为上下布局。新增控件样板与90阶段合成smoke，检查下拉提交/关闭、勾选切换、数值增减。最终Debug 13/13（15.96秒）、Release 13/13（16.24秒），两个QML lint目标无警告；Windows原生及部署smoke退出0、2窗口、warnings=false，SDK隔离后运行库均来自包内。旧候选包和标签保留，完整UX平台门禁尚未整体通过。

本地UI预览包：Daymark-1.0.0-rc.1-windows-x64-local-20261023-085636.zip，44,544,250字节，清单1450文件。SHA256：43861147ccc8735550c3142ca686d61bbbbb7a070813ea7d97107297f5edf6c0。源码修订2fa4c9b；沿用1.0.0-rc.1版本标识，以时间和BUILD-INFO修订区分预览，不移动旧标签。ZIP敏感扩展/日志/Git元数据扫描0项。没有推送或远程发布；macOS、真实辅助技术/输入法/多屏等保持UNVERIFIED。

### V1-RC：Windows V1候选版（DONE：用户确认的本地候选范围）

用户要求完成V1，已明确暂缺macOS、先交付Windows V1候选版。保留原始V1双平台门禁，不将候选版称为正式双平台V1。

执行队列：V1-A批量事务操作及筛选/日历补齐 → V1-B导航/设置/平台启动及可访问性 → V1-C完整回归、版本与候选包。需求FR-001～018、UX-02/04/05/08/09/11；归档补齐使用新002迁移。设计与逐项审计见[候选版设计](design/v1-candidate.md)。

V1-A/B已实施并自动验证。新增002迁移，升级前WAL一致快照、备份失败阻止迁移；JSON兼容schema1输入，输出schema2；不覆盖001。最终Debug 13/13（14.71秒）、Release 13/13（14.40秒）、原生smoke及lint通过。窄窗200%工具区/侧栏可滚动，列表及导航恢复已加回归；混合10k Release加载/搜索/月索引155/2/11ms。V1-C本地部署通过：隔离SDK路径、运行库来自包内，78阶段合成流程退出0、2窗口、无QML警告。完整范围见[候选说明](release-notes-v1-rc.md)及[需求审计](v1-feature-matrix.md)。

交付：Daymark-1.0.0-rc.1-windows-x64-local-20261022-201647.zip，44,182,548字节（约42.1MiB），1449个文件清单。SHA256：418a88f85a6a4436c2611d1c6f777396a8aa3d2e65782077087da8dc55cf1839。包内BUILD-INFO记录源码修订ee66204、Qt6.10.2、schema2及部署检查。源代码快照为Daymark-1.0.0-rc.1-source.zip，本地候选标签v1.0.0-rc.1-local；无远程推送。正式双平台V1门禁仍未完成。

后续仅为正式发行门槛及候选试用反馈：macOS构建/8h/睡眠多屏、Windows真实多屏/8h及干净机、公开再分发许可保持UNVERIFIED；macOS开机启动尚未实现。没有触碰真实用户库、覆盖旧包、登录或上传用户数据。

### UI-03A / M3-01：响应式详情（本轮DONE，自动验证通过）

- 需求：FR-002/007、PRD §3.2、UX-02/04/05/07/09。宽窗右侧详情，窄窗覆盖；不复制编辑状态，不改数据模式和业务查询。
- 接口：Main.qml统一openEditor/changeView/navigateSafely命令入口；TaskWorkspace保存接口保持不变。右侧详情预留列表阅读宽度，大字按可用空间回退覆盖。
- 测试：同一脏草稿宽窄往返不丢失；取消切换保持原任务；放弃后切换不写库；保存后导航使用最新版本；完成任务先处理草稿；1120/880/1440及200%布局、Debug/Release构建、CTest和QML lint。
- 风险：非模态详情使导航、工具、拖动和完成入口可同时触达，必须统一离开保护；真实输入法/读屏/macOS保持UNVERIFIED。本轮不推送、不改写既有v0.1包。
- 结果：共享草稿停靠/覆盖详情、文字缩放宽度预算及统一离开保护已实施；最终Debug 12/12（9.60秒）、Release 12/12（9.41秒），QML lint无警告。Windows原生合成smoke退出0、2窗口、warnings=false；1120/880/1440及200%截图检查通过。设计见[响应式详情](design/responsive-details.md)。

用户要求实现核心能力并发布v0.1，随后明确“先交付本地版本”。本轮交付范围因此为Windows x64本地可运行包及源码；不推送GitHub，不创建远程Release。项目仍使用C++20、Qt 6.10.2、SQLite本地优先。

此前RELEASE-01：DONE（本地范围）。最终Release验证、原生截图检查、带运行库打包及包外SDK隔离启动均通过。实施契约见[发行设计](design/release-v0.1.md)，产品边界见[发行说明](release-notes-v0.1.md)。以下发行记录保留原包范围；当前新增功能见上方UI-03A。

## 本轮实现

| 范围 | 状态与实际实现 |
|---|---|
| FR-001/002/007 | 任务CRUD、版本冲突、软删与8秒撤销、显式保存/草稿保护、键盘列表操作、日期选择器、只读时间 |
| FR-003/004/005/016 | 六视图、Today与Tonight、独立排序、拖动与项目排序、Upcoming日期分组/30天展开/独立截止提示、搜索历史选项、上下文标签过滤 |
| FR-006/008/009 | CatalogRepository/Service；领域、项目、Heading、标签、Checklist；互斥归属，项目完成隐藏/恢复，安全迁出删除事务 |
| FR-010/011 | CalendarService；月42格/周7日、开始/截止合并标记、当天任务侧栏、按日创建、单字段改期与冲突回滚、周首日 |
| FR-012/013/018 | 托盘与穿透外部恢复、浮窗/Mini/置顶/拖动/缩放/锁定、背景alpha、关闭策略、几何设置、主题/文字大小；真实OS交互仍待人工验收 |
| FR-014/015 | 本地日历规则、月末/闰日跳过、幂等实例、手改保护、停止规则；完成后N天后继；UTC提醒认领、应用内记录和单次托盘汇总 |
| FR-017 | JSON/CSV导出、临时库严格预览、事务追加/双确认替换、先备份再恢复、WAL一致每日快照保留7份 |
| NFR-性能/数据正确 | 新增真实SQLite临时库回滚/外键/重复键/冲突测试及10k加载、搜索、月索引基准；全部自动测试隔离于QTemporaryDir |
| UI-01/02/04/05 | Theme、NavRow、TaskRow，连续列表表面、菜单归类、浅深主题、日期/组织/清单/日历/组件；UI目标尚未整体验收 |

没有引入账号、云端、遥测或应用运行时第三方依赖。未覆盖历史migration，仍使用已交付schema v1；新增功能使用既有表字段。

## 已执行验证

分批Debug CTest从6项扩展到12项。窗口切片6/6、组织7/7、日历8/8、重复9/9、提醒10/10、导入11/11、排序/10k 12/12分别通过。最终Debug 12/12通过（12.01秒），Release 12/12通过（8.04秒），QML静态检查无警告；实际结果见验证报告。

Windows原生后端扩展smoke逐个打开月/周、归属清单、组织、重复、设置、数据工具、日期选择器、深色和200%文字详情；退出0，QQmlEngine警告=false。截图仅合成数据，存忽略的build目录。修复了扩展测试发现的QML方法重名、Flow只读属性、确认弹窗绑定循环，以及截图发现的星期对齐和大字导航宽度问题；不将初次失败隐去冒充一次通过。

本地文档链接检查34份通过。浅/深主题正文、次级、选中正文、强调、危险文案的已计算对比分别全部高于4.5:1；该计算不覆盖所有Qt控件、透明桌面背景或系统高对比状态。

## 未验证与后续队列

macOS编译/运行/签名，真实多屏热插拔、DPI、系统层级/焦点、穿透外部恢复、中文输入法/读屏、OS午夜/睡眠/时区事件及8h稳定性均UNVERIFIED。Windows原生探针只证明QWindow/Qt状态和界面启动，不能替代这些验收。

每视图滚动位置、系统高对比/减少透明度和全部200%文字场景仍属UI目标后续验收；响应式详情已完成本轮布局和草稿自动验证，原生焦点/读屏仍待验收。批量编辑/开机启动等P1增强未纳入本地版。GitHub双平台CI配置已准备但未推送/运行。

Qt工具的许可服务版本诊断仍存在，未绕过检查或修改系统许可配置。本机安装有商业许可协议；包只交付本地验证，公开再分发授权审查未完成，见[第三方记录](third-party-notices.md)。

## 恢复入口

先读本文件、发行说明、验证报告和ADR-011。本地RELEASE-01已完成，仅按用户新的发布安排处理远程推送；不要误把本地包视为GitHub发布完成，也不要把未测平台批量改为PASS。



## 本地产物

UI-03A后续开发试用包：Daymark-0.1.0-windows-x64-local-20261022-194445.zip（44,089,671字节）；SHA256：c203c8cf6eb3e21671c662e5e7f1ad148905129a3d375ae1ee50fe5c853f78d2。隔离SDK搜索路径的部署验证通过；此包包含新响应式详情，未替换v0.1.0-local标签及原始发行包。源码目录中的本轮验证记录包含最终测试时间及初次失败说明。

Windows包：

- Daymark-0.1.0-windows-x64-local-20261022-192938.zip（44,074,547字节，约42.0 MiB）。
- SHA256：4111e1cefb9f2bf3ce1855120c9dcb1ba6647b9205bec41fe31437e22e703f24。
- 1443个文件清单；去除SDK PATH/插件搜索环境后，包内加载Qt/MinGW模块，原生smoke退出0、2窗口、无QML警告。
- 原始包、清单、截图与日志保留在忽略的dist/build；源码通过本地Git archive另打包，不含运行库。
