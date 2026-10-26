# UX-42：年视图今日定位、桌面层级与范围配色

2026-10-26用户要求：进入年视图定位今天；独立日历、小窗和Mini条可固定于桌面、位于其他应用窗口下；收集箱避免与今天同色。涉及FR-034/039及新增FR-040，UX-02/06/09/11/12/14、P-04/07/08/09/13/14。

## 操作契约

HIG映射：一致与熟悉（窗口菜单/设置同名同状态）、用户控制（固定/置顶互斥且可恢复）、清晰与可访问（蓝/紫范围仍保留图标和文字，开关有名称及勾选状态）、适应上下文（Mini就近菜单，设置正文在200%时滚动）。沿用[设计基准](ui-baseline.md)的HIG来源及既有token，不新增一套视觉规则。

- 切换到年视图时，用本机今天作为日期锚点及选择，显示今年；年内翻页仍可看其他年份，不能因普通刷新反复跳回今天。主窗与独立日历同规则，经既有草稿离开保护，不改变任务范围。按钮、菜单、快捷键入口一致。
- 窗口层级为普通、置顶、固定在桌面，后两者互斥。固定在桌面保持在壁纸平面之上、普通应用窗口之下；点击、拖动、Mini切换、显示/隐藏与穿透切换后仍保持。关闭固定恢复普通层级；不改变位置、尺寸、可见性或锁定拖动/穿透。主窗永远不降到桌面层。
- 小窗与Mini是同一窗口的两种尺寸，继承同一个桌面选项。独立日历单独保存选项。现有菜单与设置提供同名开关，状态一致、键盘可达，失败显示错误且不保存假成功；设置与托盘保留恢复入口。
- 收集箱使用语义紫色，今天继续蓝色，名称/图形不变；浅深及增强对比模式复用现有token并检查文字/图形对比，不新增装饰。
- 设置增加“关于”分区，版本取应用元数据，品牌和介绍从通用页移入，不重复铺陈。QQ交流群1057825113、反馈邮箱409962@qq.com由用户指定；只读可选文本及带即时反馈的复制按钮，沿用FormGroup/FormRow，宽窄/200%可滚动。独立SettingsAboutPage承载内容，不扩大设置主文件。

## 工程和系统依据

QML负责显示选项；共用窗口平台适配负责互斥、几何/可见性保持、失败回滚及原生句柄重建后的恢复；设置仍存现有ui.ini，不改数据库。独立日历的设置写入与窗口实际应用保持一致。

Qt文档指出[WindowStaysOnBottomHint](https://doc.qt.io/qt-6/qt.html#WindowType-enum)存在平台限制，不能以标志接受代替原生层级验证。macOS使用公开[NSWindow.level](https://developer.apple.com/documentation/appkit/nswindow/level-swift.property)与[桌面窗口级别](https://developer.apple.com/documentation/coregraphics/cgwindowlevelkey/desktopiconwindow)，保留可交互桌面平面并低于普通窗口。Windows用公开[SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos)的HWND_BOTTOM，并在[WM_WINDOWPOSCHANGING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanging)约束本进程目标窗口的Z序；不注入Explorer、不改系统设置。平台代码独立文件、无新依赖；离屏只验证命令/状态，不能声称系统桌面实测。

## 验收

临时设置库：互斥、重启恢复、Mini尺寸、穿透/显隐保持、原生应用失败回滚；日历/组件各自独立。年视图从其他年份及已选日切入后今日/今年正确，重复刷新不重置年内导航；原有草稿保护继续覆盖。原生探针检查桌面/普通窗口实际层级及点击/重新显示；Windows/macOS分别记录，不能把mock当实机。浅深、200%、键盘/可访问名称、颜色对比；Debug/Release构建、CTest与lint，已有门禁失败保留并明确报告。
