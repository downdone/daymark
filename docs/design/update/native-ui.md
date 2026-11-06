# SDK之上的轻量UI与CLI

用户补充要求：底层应为SDK，界面可独立选择，也能直接CLI运行；并希望Windows/Linux/macOS都能快速接轻量UI。本设计选择**共享C++20 SDK + 各平台薄UI + 独立CLI**。Daymark主界面继续Qt，不因更新器选型更换应用框架。

## 1. 平台UI路线

| 平台 | 建议实现 | 运行依赖/适配量 | 当前范围 |
|---|---|---|---|
| Windows | Win32原生窗口/文字/进度/按钮，系统DPI与可访问性 | OS控件，无Qt运行库；薄presenter+事件投递 | 首期实现/实测 |
| macOS | AppKit，Objective-C++桥接SDK，NSWindow/NSProgressIndicator/NSButton | 系统框架，无需为此引入Qt或SwiftUI部署要求；主线程更新控件 | 设计接口，随后独立实施 |
| Linux | GTK3.24小窗口/进度/按钮；GTK目标可选构建 | GTK不是Linux内核自带；需目标发行版提供GTK及对应GDK后端；SDK/CLI不链接GTK | 先CLI/能力检测，GUI按目标桌面验证 |
| 无图形会话 | CLI：文本或JSONL | 不启动图形工具包，不要求DISPLAY/Wayland | 具备平台安装端口后可独立运行 |

AppKit是macOS应用框架；GTK提供窗口、进度和X11/Wayland后端能力。[AppKit](https://developer.apple.com/documentation/appkit)、[GTK窗口](https://docs.gtk.org/gtk3/class.Window.html)、[进度](https://docs.gtk.org/gtk3/class.ProgressBar.html)、[Wayland](https://docs.gtk.org/gtk3/wayland.html)、[X11](https://docs.gtk.org/gtk3/x11.html)。GTK实际后端取决于分发构建和桌面环境，不能只测XWayland就标原生Wayland通过。

Linux首选GTK3.24是本项目追求广泛现有桌面环境兼容的工程取舍，不声称它比GTK4更轻或每台Linux已安装。GUI依赖、许可/SBOM和发行打包在Linux切片冻结；GTK缺失/无会话时由CLI报告GuiUnavailable并给文本操作，不为画一个窗口再引入浏览器或强行启动X11。单独的GTK可执行文件不可用不影响SDK/CLI装载。

## 2. 共享的薄界面契约

前端只消费Snapshot与UiPresenter结果：当前/目标版本、阶段文字、下载字节、可取消/可重启、错误与恢复动作。发出Cancel/Restart/Later/Retry/OpenRelease/Close意图；不接收原始下载URL、DB、签名私钥或任意文件操作权。版本比较、限流、重试、清理和事务只在SDK实现。

界面只需三个状态页/区域：进度（阶段+真实进度+取消）、Ready（重启生效+立即/稍后）、失败（原因+重试/发布说明）。后台准备默认不独立弹窗；Daymark关于页是主要状态入口，用户主动查看独立更新器时才显示。切换期间旧Qt主进程已退出，轻量UI仍可独立展示有界进度和错误。

使用平台控件、系统字体、自然文本尺寸和原生按钮排序；不复制主应用整套主题/玻璃效果。HIG按项目P-13映射内容优先、真实进度、用户可控、可访问，Windows/Linux同时尊重平台键盘和控件习惯。不同前端保留同一动作含义，不要求像素完全一致。仅做布局适配，不给每个平台复制状态机。

事件投递：SDK工作队列→Windows消息/AppKit主队列/GLib主上下文→render(snapshot)。每个前端按operation/revision拒绝过期事件，关闭前退订，不能在后台线程直接改控件。减少动态使用不定进度的系统表现，取消/成功不能由动画结束决定。

## 3. CLI与GUI的关系

独立CLI是完整产品入口，不依赖GUI“代跑”。它可检查、准备、查询、取消、请求应用Ready；用户CLI apply仍受活动应用保存门限制。GUI和CLI都用同一个SDK，已有执行者时另一个只观察/发受控意图，不启动第二轮安装。Windows另设console目标以保证脚本退出码/stdout语义。

Qt更新器UI以后也能接同一SDK/Session；当前不同时实现Qt与原生两套独立更新器窗口，以免重复维护。主应用Qt适配器本轮设计已保留，相同SDK Snapshot即可同时支持关于页和原生小窗口。

## 4. “快速适配”的范围

可复用的是全部业务规则、清单/状态/错误/CLI和大部分界面状态映射；每个平台UI只需适配控件和主线程投递。不能据此承诺整个跨平台更新器同样快速：macOS签名/公证、.app/LaunchServices与Linux包管理/权限/桌面入口的安装端口都要独立设计验证。

首期Windows SDK/CLI/Win32 UI闭环；macOS先AppKit界面状态演示+SDK端口验证，再打通实际安装；Linux先明确支持的用户归档模式/发行版及CLI，再GTK X11/Wayland实测。平台安装端口未完成时显示手动下载/UnsupportedPlatform，UI存在不代表能自动安装。

轻量验收用实测说话：SDK/CLI导入不含Qt/GTK/AppKit，GUI独立测包体增量、启动时间、私有内存和依赖。Windows/macOS不另捆绑GUI框架；LinuxGTK可复用系统依赖但不能把其系统占用统计为零。本轮不承诺未经测量的包体/内存数字。
