# CHROME-01 / UX-79 可切换的Windows标题栏

状态：设计与单代理自审通过，实施及验收事实见PROGRESS。覆盖FR-013，并按用户2026-11-08要求取代FR-039“Windows始终系统标题栏”的限制。

## 1 功能定义与实例

Windows默认使用融入界面的Daymark窗口顶区：主窗复用侧栏字标与页面标题/工具，设置复用“设置/当前分类”标题，账号复用现有品牌组合；不再添加独立的图标/窗口名横条、跨窗灰底或分隔线。主窗有最小化/最大化（还原）/关闭，工具窗只有关闭。设置→外观→显示→标题栏选择“Daymark / 系统原生”，即时保存并应用所有这三类窗口；其他平台保留OS原生且不展示本设置。已有小窗/Mini/球、桌面日历及原生对话框不变。

选择原生后仅更改装饰，不重建QML引擎、资料、worker或编辑会话；再选Daymark恢复相同几何/状态。关闭仍走原有保存、退出/托盘与账号密码清理；失败保留原选择并有可操作错误。重启、切资料、退出账号均保留同一设备选择。

## 2 现状与技术选型

Qt固定6.10.2、C++20、Quick；不用Qt Widgets/浏览器/新增第三方库。当前主窗、设置和账号是ApplicationWindow，系统边框与WindowStateStore独立管理。Qt6.9+扩展客户区可融合背景但会依赖平台caption覆盖/安全区；本轮需要可控的Daymark按钮/主题及明确命中，选择QML融合顶区＋保留Qt标准窗口flags＋隔离Win32非客户区适配。Qt扩展客户区仅作取证原型，不作为发布实现。初次原生桌面抓图被遮挡/黑屏不计为视觉PASS，纯Qt客户区捕获仅证明QML绘制。

Win32保留WS_CAPTION/SYSMENU/THICKFRAME及对应min/max能力，WM_NCCALCSIZE去除原生标题栏，WM_NCHITTEST让边缘、标题拖动、最大化按钮保留系统语义；不手写拖动坐标或最大化铺屏。Windows11 Snap布局依赖HTMAXBUTTON，物理悬停菜单单独标记验收边界。主窗最小宽度仍为880DIP，窄屏或高DPI下部分贴靠分区可能小于现有最小尺寸；本轮不扩大为主工作区响应式改版，不承诺所有分区均可容纳。参考[Qt窗口flags](https://doc.qt.io/qt-6.10/qt.html#WindowType-enum)、[ApplicationWindow安全区](https://doc.qt.io/qt-6.10/qml-qtquick-controls-applicationwindow.html#safe-areas)、[微软Snap要求](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/apply-snap-layout-menu)。在线Qt页现为6.10.3，实际仅用本地6.10.2公共头与构建验证。

## 3 架构与数据流

AppRuntime在加载QML前拥有一个WindowChromeController（设备ui.ini，不随资料切换）。Main/Settings/Account各拥有一个WindowChrome绑定及一个惰性DaymarkTitleBar。Controller负责偏好和跨窗口应用/回滚；绑定负责窗口生命周期/逻辑区域；Win32文件负责样式、消息、DWM与系统菜单。QML仅绘制和发送窗口命令，不持久化、不写业务库。

## 4 模块与生命周期

Controller寿命覆盖QML引擎；绑定QObject由对应QML窗口拥有，以QPointer持有窗口及标题/按钮区域，销毁时解除注册/Win32 subclass。窗口SurfaceCreated重新应用原生样式，Surface销毁清空句柄。以SetWindowSubclass挂到这三类明确注册的窗口，覆盖同步SendMessage和派发消息；不是只依赖Qt全局事件过滤。只为注册窗口处理，不影响组件和Popup；GUI线程串行，不新增线程/轮询/每帧扫描。

## 5 协议与兼容

只有GUI线程内调用及Qt通知，无网络、IPC、服务器或更新协议变更。titleBarMode为稳定整数0系统/1Daymark，未知配置回退Windows默认Daymark；非Windows强制有效原生。restricted更新阶段只读设置，COMMIT后才开放写入，防止破坏更新写门。

## 6 接口与结构

WindowChromeController提供supported、mode、errorText/errorKey/原生数值、requestMode(int):bool；非法值/只读拒绝，无变更。对已注册绑定先应用，全部成功才sync设备设置；失败恢复先前模式及窗口几何/状态，公开错误；重复相同值幂等。WindowChrome提供window、controller、titleBar、buttonStrip、maximizeButton、customEnabled，窗口命令toggleMaximize/showSystemMenu；close/minimize仍调用目标Window的标准方法。

布局坐标为DIP；Win32消息坐标是有符号物理屏幕坐标，先ScreenToClient再除目标DPR。命中优先级：可见最大化按钮HTMAXBUTTON→按钮条HTCLIENT（保持完整命中高度）→非最大化边缘/角→标题空白HTCAPTION→客户区。隐藏/失效item不命中。最大化只取当前monitor工作区；全屏不显示标题栏，不拦截边缘。

## 7 数据库与事务

不涉及SQLite/schema/迁移：窗口装饰是设备偏好，沿根ui.ini新增window/titleBarMode。QSettings sync成功后才更新公开mode；写失败恢复运行态和旧存储值，不更改任务/同步资料。旧程序忽略此新增key，业务schema10不变。

## 8 HIG/UX与完整状态

映射HIG的一致性、内容优先、适应上下文和用户控制（P-13/17/20/21，UX-09～13/79）：同一标题条/控件槽、浅深/高对比统一token、标题省略但不侵入按钮；高度max(36,caption+16)，按钮46DIP宽，不引入常驻装饰工具。活动/非活动、hover/pressed、键盘焦点、禁用、最大化/还原均明确。关闭hover用Windows危险操作习惯，按钮有Accessible.name，无普通tooltip。

拖动空白、双击最大化/还原、右击/Alt+Space系统菜单、Alt+F4和Win+方向键沿系统。按钮Tab/Space/Enter可达，切模式不丢编辑/IME/焦点。设置选项在窄窗/200%重排，错误可换行。标题不另复制侧栏字标或工具栏。

HIG来源沿项目[UI调研](ui-research.md)，[Windows](https://developer.apple.com/design/human-interface-guidelines/windows)明确遵守运行平台窗口习惯；不复制macOS交通灯或Apple资产。

## 9 可靠性、隐私与性能

不删除/迁移用户库；所有新测试用QTemporaryDir。非客户区切换捕获可见性、物理外框、原生WINDOWPLACEMENT与Qt state；最大化/最小化不强制showNormal，避免把中间态写成常态。原生失败报稳定key及Win32码，UI可恢复原生模式；DWM装饰是可降级辅助，不把缺少圆角当数据错误。无透明背景/模糊/离屏层/逐帧resize；只加载可用标题栏和少量按钮，不撤销MEM-04的内容回收策略。

## 10 分层验收

规则/偏好：非法/损坏配置、持久化、重复/只读、存储失败零公开变更、设备/资料独立。实际Qt/Win32集成：反复原生↔Daymark，普通/最大化/隐藏，几何与句柄/状态，边角/负坐标命中、按钮区域与系统菜单，resize及关闭标准事件。QML用户链：设置实际切换、三窗一致、脏任务和账号草稿保留、浅深100/200%、窗口控制按钮/键盘；隔离原生截图与frame记录。全套CTest、QML lint、正式签名/独立部署/安装和2.1.1→2.2.0公网更新后发布。物理多屏/DPI/读屏、Snap悬停菜单和macOS/Linux未实测时仍UNVERIFIED。

## 11 自审问题与准入

单代理自审，非独立评审。修订：①不只Frameless而丢失系统resize/Snap；②不把偏好放在账号资料而切换账号时乱跳；③不重建QML或硬关闭丢草稿；④不自己计算最大化高度忽略任务栏；⑤native最大化区域的按下/释放与QML键盘仅各执行一次；⑥restricted写门及存储失败回滚；⑦不把离屏客户区图当原生按钮/DWM通过。以上接口、时序和测试闭合，准入READY；单切片实现，发现原生回归先修，不以延长超时放行。

## 12 队列与交付

A偏好/绑定/Win32规则与单测→B三窗共享标题栏及设置入口→C隔离用户链/原生验证→D完整回归/文档→E2.2.0源码、正式构建/签名/安装与实际更新发布。用户已追加授权完成后发布新版本。实际PASS/FAIL/UNVERIFIED只写PROGRESS；不动既有tag/正式资产或无关build-request/品牌草稿。

## v2 融入界面复审（用户2026-11-08补充）

用户指出第一版独立标题横条仍割裂界面。修订为内容即标题区：

- 主窗将既有sidebarBrandHeader/pageToolbar移到一个共享顶区，左侧背景延续侧栏、右侧延续内容；取消重复图标和Daymark小字。既有页面工具与窗口按钮横向分槽，字标/页面名仍在原横轴。顶区高度由页面标题与既有工具自然高度决定，默认不增加空白行；大字按自然高度增长。
- 设置将既有“设置”和当前分类/说明移到同一顶区，关闭按钮固定右上槽；账号将现有品牌组合置于顶区，不再额外显示“账号·Daymark”。品牌资产和正文输入不重建。
- 三个窗口的应用顶区始终存在，OS原生模式仅隐藏自绘窗口按钮、恢复OS装饰；切模式不再销毁整条应用工具区或改变其高度，保留窗口按钮槽避免页面工具跳动。自绘窗口按钮仍惰性创建/隐藏后释放。
- Native命中排除实际可交互的折叠导航/返回/页面工具区域；这些弱引用有界保存，不遍历QML树，即使工具禁用也不把其区域当拖动。标题、字标和空白可拖动，正文/输入保持客户区。模态覆盖和禁用沿原有Overlay与关闭保护，不额外禁用迁移后的工具；Alt+F4仍经过关闭保护。
- 正文、详情高度及弹层锚点统一使用ApplicationWindow.contentItem的可用区域，避免把窗口全高误作去除共享顶区后的高度。Dialog在正文区域居中，菜单Window沿原生来源坐标，不硬改Overlay私有几何。

自审：修复第一版重复品牌/独立灰条、交互区被HTCAPTION吞掉和正文/弹层高度边界。新增interactiveItems为QVariantList输入，内部转为QPointer弱引用，销毁后不悬挂；需要Native/Qt用户链及浅深100/200%重新验收。此前原生切片6项通过仍只证明后端消息和偏好，不将其等同v2布局通过。第一版用户链在最大化切换后的下一事件轮失败，必须保留这个用例并修复，不通过改设计隐藏失败。

原生最大化根因复审：Qt FramelessWindowHint会走几何模拟最大化，Qt缓存为Maximized但HWND的IsZoomed为false、placement仍Normal；随后改flags产生迟到Normal事件。最终不再切换Qt frameless标记，保留Qt标准窗口契约，仅由已注册HWND subclass控制NCCALCSIZE/命中；原生placement与正常还原矩形在切换中保存/恢复。新增下一事件轮、OS最大化及最小化/正常位置回归，不用延迟重试掩盖。

正文坐标复审：列表来源直接映射到Main.contentItem；日历/时间轴对外锚点明确为scene坐标（mapToItem(null)），进入主窗详情时只在边界转换一次，避免Window.window的基类contentItem与ApplicationWindow正文混用。箭头验收统一mapToScene后比较，不能用正文y直接比场景y。短备注能完整容纳时按完整字段最小滚动，长备注仍跟随光标；新顶区缩小正文不应留下旧字段的滚动偏移。CI与本地标准测试入口均从Windows系统字体目录读取字体，不复制字体资产；offscreen不是原生焦点证明。
