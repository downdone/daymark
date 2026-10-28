# UX-48 全局视觉与排版统一

2026-10-27；FR-018/035/037，UX-01/02/04/05/09/10/11/12/13/14，P-13/14。承接UX-44～47；真实状态只见PROGRESS。

## 依据与审视结论

以Apple HIG [Layout](https://developer.apple.com/design/human-interface-guidelines/layout)、[Color](https://developer.apple.com/design/human-interface-guidelines/color)、[Labels](https://developer.apple.com/design/human-interface-guidelines/labels)和[Fonts](https://developer.apple.com/documentation/technologyoverviews/fonts)的层级/对齐、成对色彩、系统字体与文字角色为依据。保留桌面信息密度、系统窗口、Ctrl/Command与原数据/草稿机制；不复制Apple字体、符号或材质。

已有语义token，但旧页面还混有深边框、不同半径、字体符号箭头、24格Canvas图标与独立指示器，字段/按钮/菜单的状态不完全一致。需要统一设计源和实际全表面迁移，而不是每页另挑一种风格。

## 唯一视觉规则

| 项目 | Daymark规则（DIP，均非Apple指定数值） |
|---|---|
| 字体 | UX-60：Windows中文优先Microsoft YaHei UI统一中西文/数字，缺失回退Segoe UI/系统字体，Quick保留Qt文字渲染并优先抗锯齿；macOS及其他平台保留系统GeneralFont。正式与探针共管线。正文正常字重，标题/组名/选中DemiBold；无额外字体嵌入 |
| 文字角色 | 页面28（UX-59）、普通标题20、面板/任务标题17、列表标题15（UX-60）、分组/正文/导航/菜单13、辅助12、时间刻度等密集标注11。全部乘100～200%文字比例，禁止在局部再用caption-1/body+2等推导字号 |
| 色彩 | 浅色白内容/柔和中性侧栏与分组、深色分层炭灰；蓝用于操作/焦点/选中，绿完成/开关、红危险/今天、橙旗标、紫收集箱。内容不用品牌色铺满；4组浅深/高对比配对继续计算 |
| 布局 | 4/8/12/16/24节奏，UX-60正文16页边起、页面区域16随字放大，文字块允许2 DIP光学校准；表单同行按内容最小宽重排，关系靠对齐/缩进、分组靠留白/细分隔。弹窗头/错误/操作可达，正文一条滚动轴；不以缩小字体实现紧凑 |
| 密度 | 交互区32起、列表36起、菜单28起、设置行40起；文字放大按内容增高。紧凑/舒适共一套语义，舒适只加行/字段留白。时轴比例与几何坐标不作为页面间距改写 |
| 圆角 | 控件/轻胶囊7、选中行8、菜单/分段外壳10、内容组12、覆盖面板/组件14、事件条4。圆形完成圈/色点保持圆；真正连续页面不包大圆框 |
| 线框 | 装饰分隔浅1；字段平时浅填充，悬停/输入焦点或高对比才出现1px必要轮廓；就近属性维持透明/轻悬停。普通按钮中性填充、不叠深框；危险/主要/轻量状态语义保持 |
| 图标 | 原创24格矢量路径、统一光学内边距/圆端点/圆转角，小图标适度补偿线宽；工具16～18、标注14、放大上限24。去掉字体勾/箭头及独立Canvas指示器，菜单/复选/下拉/微调与业务图标共用Glyph；不靠图标颜色单独表达意义 |

应用标识保留原创“日历勾”身份，按新强调色校准，与UI资源分工清楚。截图看齐光学大小，不把字形占满按钮；命中仍取控件大小。辅助提示沿UX-40，不新增重复文字tooltip。

## 覆盖与边界

1. 主窗侧栏/智能范围/列表/原行/共享详情/双页签；日周月年、独立日历、拖选/范围小窗；设置目录/四子页。
2. 日期/时间/标签/分类选择器，所有菜单/确认/批量/资料/数据/提醒/规则管理，通用空态/错误/忙碌与样式库。
3. 小窗和Mini沿同一字级/矢量图标/半径/线框，保留可恢复穿透、透明度、层级、尺寸和完成动画。

| 表面 | 实现来源与验证路径 |
|---|---|
| 主窗/侧栏/任务/原行 | Main、NavSection/NavRow/SmartViewTile、TaskRowSummary/TaskQuickEditor；正文/页面字级、框线与轻胶囊；task_list_interaction、desktop_surfaces |
| 日/周/月/年与独立日历 | CalendarTimeline/MonthGrid/MonthStream/YearCalendar、DesktopCalendarWindow；事件半径、刻度字级、状态矢量；contextual_workspace、continuous_calendar、calendar_interaction |
| 详情/新增/选时段 | PopoverSurface、TaskDetails/Timing、InspectorTabs、CalendarCreatePopup/TimeRangePicker；共用面板半径/焦点/字体与草稿链 |
| 设置/关于 | SettingsGroup/Row/Toggle/Link；外部标题、正文行、透明度禁用、200%/高对比；desktop_surfaces及旧完整表单几何门 |
| 分类/标签/批量/数据/规则/提醒 | FormGroup、FormRow、Label、Button/FieldSurface、Dialog/Menu；共享分组/字级/轮廓；既有完整链及资料/批量探针，不在页面另铺一套配色 |
| 小窗/Mini | WidgetWindow/WidgetTaskRow、Glyph；与主窗相同任务字级、缩放上限和面板关系；desktop_surfaces |

极小圆点、复选框、滑杆/滚动条及范围条端部属于几何例外，保留圆形/小角，不将所有形状强制改成同一个半径。软件Shape的短圆端线改为显式圆路径；软件后端启用有界小层防止图标越过滚动裁剪，RHI请求CurveRenderer。测试验证可见图标有真实像素、裁剪外没有图标像素，后者不能以元素存在替代。

统一共享控件/token并逐个检查专用容器。无业务筛选、持久化或schema变更；不因视觉更新增加保存动作、删除功能或改私有数据。旧全量门禁超时按真实结果保留，不能通过删测试做绿。

## 验证

Qt6.10.2 Release构建、双QML lint、完整CTest尝试；既有UI五入口覆盖主/独立、列表/详情、设置、组件/Mini、选择器及原草稿/拖选/批量动作。补真实矢量图标的非空、颜色更新、细小/大字尺寸与代表性不同形状截图，不以对象存在冒充有绘制。沿样式库展示全部图标和字级/状态，四色对比、全QML字号与半径审计、文档链接检查。修复本轮回归后按受影响入口复验；原生合成阴影、实际DPI/IME/读屏/macOS无实测仍UNVERIFIED。
