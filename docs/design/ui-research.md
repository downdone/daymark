# Things 3与Apple UI/UX调研

UX-48图形机制补核：[Qt 6.10 Shape](https://doc.qt.io/qt-6.10/qml-qtquick-shapes-shape.html)区分RHI的Geometry/Curve与软件QPainter渲染，Curve可请求高质量抗锯齿，软件后端仍由Qt选择SoftwareRenderer。Daymark保留两条后端，软件路径加小范围有界层处理嵌套滚动裁剪，并用真实像素与截图验证；离屏结果不宣称原生GPU/DPI完成。

UX-48依据[HIG Layout](https://developer.apple.com/design/human-interface-guidelines/layout)的对齐/适应性、[Color](https://developer.apple.com/design/human-interface-guidelines/color)的语义与浅深高对比配套、[Labels](https://developer.apple.com/design/human-interface-guidelines/labels)的系统字体与文字层级，以及[Apple Fonts](https://developer.apple.com/documentation/technologyoverviews/fonts)的系统API与禁止打包系统字体说明。Daymark使用Qt平台GeneralFont及自己的矢量图形/数值，不采用Apple专有字体或符号资产；实施规则见[全局系统](global-visual-system.md)。

UX-47补核（2026-10-27）：[Apple Settings](https://developer.apple.com/design/human-interface-guidelines/settings)用于影响整体体验的偏好；[Lists and tables](https://developer.apple.com/design/human-interface-guidelines/lists-and-tables)以设置列表表达层级并以chevron进入下一层；[Segmented controls](https://developer.apple.com/design/human-interface-guidelines/segmented-controls)表达紧密相关选项。Daymark将四类设置迁为分组目录，任务详情保留独立内容页签，数值/色彩为自主设计，见[UX-47](settings-navigation-and-inspector-tabs.md)。

调研日期：2026-10-22。对象：Things for Mac为主，Apple桌面HIG为原则参考；移动端交互仅用于解释，不直接搬到Windows。产出是Daymark设计依据，不是Things客户端体验测试或像素复刻报告。

## 1. 方法与证据边界

只使用Cultured Code和Apple公开官方资料，未登录、安装或抓取商业软件数据，未提取图标、字体、动画或二进制资源。官方截图仅保留来源链接，不进入产品资源包。本轮阅读官网和支持文章；没有以实际Things客户端测量字号、间距或操作耗时。下文区分“来源事实”“设计解释”和“Daymark决策”。

Things Features页明确说明主体功能介绍来自3.0，并另外链接后续更新，因此不能将2017截图等同于当前所有版本。侧栏、搜索以现行支持页补充，字号能力以2023年3.18官方文章补充。没有证据的行为不推断为Things现状。

Apple当前HIG已包含Liquid Glass，设计原则页标注2026-06-08重新引入。它是动态文档；当前网页HTML依赖JavaScript，本轮使用页面自己提供的同域`.md` alternate正文读取，不使用第三方HIG镜像。原则在跨平台应用中的具体落地需要工程判断。

## 2. Things：可借鉴的设计机制

| 来源事实 | 设计解释（本项目判断） | Daymark采用方式 |
|---|---|---|
| 任务可展开为编辑区域，附加字段按需出现 [T1] | 阅读任务与编辑属性有不同信息密度 | 默认突出标题；选中后展示详情，日期/标签/清单渐进呈现 |
| Today与Evening分组，Upcoming按未来安排组织 [T1] | 以执行时机建立层次 | 用今日/今晚和日期标题组织内容，筛选仍遵守PRD |
| 项目支持Heading和任务内Checklist [T1] | 分组能减少长列表的认知负担 | Heading表达项目结构，Checklist保留在任务内部 |
| Quick Find同时承担搜索与导航，继续搜索扩展范围 [T2] | 搜索能成为跨位置入口 | 明确区分全局/当前范围，先保持可靠的标题备注搜索 |
| Jump Start集中常用排期选项 [T1] | 高频日期决策应少跳转 | 提供“今天/明天/选择日期/清除”，开始与截止独立 |
| Slim Mode隐藏侧栏，仍能导航 [T3] | 减少干扰必须保留回路 | 未来可折叠侧栏；隐藏后保留显式导航按钮和键盘入口 |
| Mac提供丰富键盘命令 [T4] | 高效操作不依赖鼠标 | 实现Daymark自身快捷键契约，不照搬Things键位 |
| 3.18支持调整文字，图标和布局共同缩放 [T5] | 可读性是系统性布局问题 | 用语义字体、可伸缩行高和矢量图标，不仅放大字 |

T1为功能机制来源；“设计解释”和所有Daymark落地为独立判断，并非Cultured Code建议。

公开视觉参考：[Today旧版Mac示例](https://static.culturedcode.com/things/videos/2017-05-18-website-videos/2-today-mac.png)、[Heading旧版示例](https://static.culturedcode.com/things/videos/2017-05-18-website-videos/4-headings-mac.png)、[Quick Find旧版示例](https://static.culturedcode.com/things/videos/2017-05-18-website-videos/7-quickfind-mac.png)、[2023 Mac默认字号示例](https://culturedcode.com/frozen/2023/09/things-mac-text-size-100-io60.jpg)。只作历史参考，不声称测得精确token。

## 3. Apple：原则到Daymark的映射

Apple现行Design principles列出Purpose、Agency、Responsibility、Familiarity、Flexibility、Simplicity、Craft、Delight八项 [A1]。以下中文落地由Daymark定义，不是官方逐字翻译：

| 原则 | Daymark目标 | 设计评审问题 |
|---|---|---|
| Purpose / 目的 | 帮助收集、安排和执行任务 | 每个常驻控件是否服务高频主任务？ |
| Agency / 自主 | 易撤销，可退出编辑、筛选及穿透 | 用户能否找回上下文与未保存内容？ |
| Responsibility / 责任 | 本地隐私与真实保存反馈 | 失败时是否保留数据并明确下一步？ |
| Familiarity / 熟悉 | 平台窗口、菜单和键盘习惯 | 看起来相似的操作是否行为一致？ |
| Flexibility / 适应 | 窄窗、缩放、深浅主题和多输入 | 变大文字后关键动作是否仍可达？ |
| Simplicity / 简明 | 内容优先、按需呈现 | 是否用层次解决复杂性，而非删掉必要能力？ |
| Craft / 完整 | 基线对齐、焦点和全部状态齐备 | 正常/失败/空态是否同样经过设计？ |
| Delight / 感受 | 平静、安心、顺畅 | 动效和反馈是否帮助用户继续工作？ |

补充桌面依据：macOS指导强调可调整窗口、键盘、菜单和减少无谓模态 [A2]；布局指导支持对齐、分组、渐进呈现与适应窗口变化 [A3]；可访问性要求多种感知/操作途径、字号适应和对比度 [A4]；排版、侧栏、按钮、动效、撤销分别见[A5～A10]。

## 4. 不直接迁移的内容

- Things的外部日历事件、云、自然语言、全局快速录入、多主窗口或移动Magic Plus不因调研进入V1。Daymark保留月/周任务日历和单实例悬浮窗自身价值。
- Things的Quick Find分层搜索与Daymark当前全局标题/备注搜索并不相同；不为相似外观偷改PRD搜索范围。Things快捷键导航编号也与PRD不同，以Daymark契约为准。
- Apple最新材料指导包含原生系统材质。Qt Quick 6.10.2、Windows10/11和macOS13+不能据此承诺同样效果；Daymark先用可读的不透明表面和细边界。玻璃/模糊是能力允许时的可选增强，不成为UI合格条件。
- Apple字体和SF Symbols不作为可随意跨平台打包的资源。优先系统字体回退、自绘独立图标或许可明确资产；引入外部图标库先按仓库依赖政策写ADR。
- 本次不把旧版“清晰、服从、深度”等概括当作现行HIG完整原则清单；用[A1]的当前分类，并在产品中落实为可验证规则。

## 5. 对现有Daymark的差距判断

依据当前Main.qml、WidgetWindow.qml和BASIC-01合成截图，属于本项目审查结论：

| 当前表现 | 影响 | 改善目标 |
|---|---|---|
| 侧栏和任务行动使用大块普通按钮 | 导航/操作权重接近，任务文字被挤压 | 导航行＋克制选中背景；完成小控件，标题占主要区域 |
| 每任务白卡、70DIP行、大“编辑”按钮 | 多任务扫描成本高 | 单一列表表面，40～48DIP标准行；详情从行选择进入 |
| 搜索、新增、说明和状态文案常驻 | 工程解释占用首屏 | 工具栏聚合命令，提示按需出现，搜索有范围和退出入口 |
| 常规编辑总用模态对话框 | 中断列表上下文 | 宽窗右侧详情，窄窗抽屉/覆盖，统一草稿保护 |
| 日期必须手输ISO，状态为工程枚举映射 | 计划动作认知较重 | 语义排期操作＋日期选择器，保留键盘输入能力 |
| 无系统化色彩/字体token | 主题和状态难统一 | 先定义语义token，再逐组件替换，不全量重写 |
| 字符圆圈/恢复箭头代替正式图形 | 字体与焦点表现不稳定 | 独立矢量完成/恢复图形，扩大命中区并提供可访问名称 |

本轮仅建立目标，未改变上述界面。功能完整性、保存失败与草稿保护优先于视觉精简。

## 6. 官方来源登记

所有链接访问于2026-10-22；文章年代或页面动态性单列。建议变更相关原则时复核对应来源，不在运行时访问这些网站。

| ID | 官方资料 | 用途/时间边界 |
|---|---|---|
| T1 | [Things Features](https://culturedcode.com/things/features/) | 官方3.0功能介绍及后续说明；历史机制依据 |
| T2 | [Searching and Navigating with Quick Find](https://culturedcode.com/things/support/articles/2803584/) | 当前搜索/导航支持说明 |
| T3 | [Show and Hide Things’ Sidebar](https://culturedcode.com/things/support/articles/3238254/) | Slim Mode及恢复导航 |
| T4 | [Keyboard Shortcuts for Mac](https://culturedcode.com/things/support/articles/2785159/) | 证明完整键盘通路；不复制其键位表 |
| T5 | [Things Big and Small](https://culturedcode.com/things/blog/2023/09/things-big-and-small/) | 2023-09-06，3.18字号/布局缩放 |
| A1 | [Design principles](https://developer.apple.com/design/human-interface-guidelines/design-principles) | 现行8原则；[官方Markdown](https://developer.apple.com/design/human-interface-guidelines/design-principles.md) |
| A2 | [Designing for macOS](https://developer.apple.com/design/human-interface-guidelines/designing-for-macos) | 桌面平台特点；[正文](https://developer.apple.com/design/human-interface-guidelines/designing-for-macos.md) |
| A3 | [Layout](https://developer.apple.com/design/human-interface-guidelines/layout) | 层次/分组/适应；[正文](https://developer.apple.com/design/human-interface-guidelines/layout.md) |
| A4 | [Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility) | 对比、输入、文字；[正文](https://developer.apple.com/design/human-interface-guidelines/accessibility.md) |
| A5 | [Typography](https://developer.apple.com/design/human-interface-guidelines/typography) | 字体层次/平台差异；[正文](https://developer.apple.com/design/human-interface-guidelines/typography.md) |
| A6 | [Materials](https://developer.apple.com/design/human-interface-guidelines/materials) | 当前材质与可读性；[正文](https://developer.apple.com/design/human-interface-guidelines/materials.md) |
| A7 | [Sidebars](https://developer.apple.com/design/human-interface-guidelines/sidebars) | 分组和导航行为；[正文](https://developer.apple.com/design/human-interface-guidelines/sidebars.md) |
| A8 | [Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons) | 主要动作及破坏性动作；[正文](https://developer.apple.com/design/human-interface-guidelines/buttons.md) |
| A9 | [Motion](https://developer.apple.com/design/human-interface-guidelines/motion) | 有目的、可减少的动效；[正文](https://developer.apple.com/design/human-interface-guidelines/motion.md) |
| A10 | [Undo and redo](https://developer.apple.com/design/human-interface-guidelines/undo-and-redo) | 可预测恢复；[正文](https://developer.apple.com/design/human-interface-guidelines/undo-and-redo.md) |

进一步目标和token见 [UI/UX设计基准](ui-baseline.md)，实施门禁见 [UI验收](ui-acceptance.md)。

## 2026-10-23：HIG升级为项目原则

用户指定[中文Getting started](https://developer.apple.com/cn/design/human-interface-guidelines/getting-started)作为项目依据；执行约束见[原则P-13](../principles.md)。中文页正文依赖JavaScript，本轮从Apple同站英文官方Markdown复核[Getting started](https://developer.apple.com/design/human-interface-guidelines/getting-started.md)、[Design principles](https://developer.apple.com/design/human-interface-guidelines/design-principles.md)、[Layout](https://developer.apple.com/design/human-interface-guidelines/layout.md)、[Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility.md)。现行原则仍为上文8项；布局强调信息层次、对齐/分组与窗口/字号适应，可访问性强调可感知、可操作和适应。项目采用原则并验证具体实现，不将自主尺寸、线框形式或Windows材质声称为Apple强制规范。

## UI-15官方依据复核（2026-10-24）

[Apple Buttons](https://developer.apple.com/design/human-interface-guidelines/buttons)强调动作可辨识、合适命中区域及交互状态；[Icons](https://developer.apple.com/design/human-interface-guidelines/icons)强调熟悉含义和统一图形语言；[Color](https://developer.apple.com/design/human-interface-guidelines/color)要求语义一致并适配浅深/增强对比度。动态网页以官方同域.md正文读取。Daymark的按钮三分类、32DIP起命中、原创线性图标与具体配色是本项目的平台设计决定，不能写成Apple规定值，也不作视觉/辅助技术认证。落实见[动作与配色](actions-and-color.md)。


## UX-39 日历操作官方依据（2026-10-26核对）

[Apple日期与视图导航](https://support.apple.com/en-md/guide/calendar/icl1010/mac)说明月/周日期双击进入日、年月份双击进月；[Apple日历键盘](https://support.apple.com/en-ae/guide/calendar/ical002/mac)列出Command数字视图、今天/跳日期/前后、新建与编辑入口。Daymark的实际对照和取舍见[UX-39](macos-calendar-interaction.md)，不能据Windows合成事件宣称macOS实机操作全通过。

[Qt QKeySequence](https://doc.qt.io/qt-6/qkeysequence.html)明确便携Ctrl/ControlModifier在Apple平台对应Command，Meta对应物理Control；日历和原任务导航按此表达，不手写Meta数字作为Command。未复制Apple字体、图标或其他专有资产。


## UX-40 辅助提示与菜单（2026-10-26核对）

[Apple Offering help](https://developer.apple.com/design/human-interface-guidelines/offering-help)要求提示简洁、针对当前控件、避免重复名称；[Qt ToolTip](https://doc.qt.io/qt-6/qml-qtquick-controls-tooltip.html)说明共享提示和显式延迟等机制。当前工程6.10.2不使用6.12新增policy。Daymark准入、600ms/5000ms、12/13字级及28起菜单行均为自主实现数值，具体见[UX-40](help-tags-and-menu-type.md)，并非Apple给出的量化要求。


## UX-41 侧栏与品牌参考


[Apple Sidebars](https://developer.apple.com/design/human-interface-guidelines/sidebars)、[The menu bar](https://developer.apple.com/design/human-interface-guidelines/the-menu-bar)用于平台入口关系；[小米官方2021Q1材料](https://ir.mi.com/static-files/9f0e2af7-4564-4540-a52f-b85ab64ef6d0)确认视觉参考对象。Daymark没有采用小米资产/字体或宣称字形符合其内部标准，而是自行定义圆弧、笔画及留白，见[UX-41](sidebar-and-wordmark.md)。Qt图形机制见[Shape 6.10文档](https://doc.qt.io/qt-6.10/qml-qtquick-shapes-shape.html)，实际固定构建仍为6.10.2，平台兼容修正以PROGRESS证据为准。

## UX-44 菜单与可调整窗口

UX-44补核（2026-10-27）：[Pop-up buttons](https://developer.apple.com/design/human-interface-guidelines/pop-up-buttons)针对互斥选择，明确建议多选及命令使用[Pull-down buttons](https://developer.apple.com/design/human-interface-guidelines/pull-down-buttons)；[Windows](https://developer.apple.com/design/human-interface-guidelines/windows)要求内容适应尺寸并优先平台窗口交互。Daymark据此将多标签实现为就近勾选命令菜单、底部分组新建，独立日历调用Qt系统缩放，项目值及无框组件边界见[UX-44](year-tags-and-calendar-sizing.md)，不把自定token当Apple规定。

### UX-55菜单来源（2026-10-27）

[Apple Menus](https://developer.apple.com/design/human-interface-guidelines/menus)提出相关操作成组、以分隔区分、慎用子菜单，标签清楚表达动作与状态；图标应服务识别，同组视觉处理一致。[Pop-up buttons](https://developer.apple.com/design/human-interface-guidelines/pop-up-buttons)用于同一属性的选择。Daymark据此定义[独立菜单契约](menu-system-review.md)，组顺序、9 DIP分隔占位、快捷键列及托盘分区是项目选择，非Apple量化要求。Windows托盘QMenu和macOS原生菜单保留平台样式与角色。
