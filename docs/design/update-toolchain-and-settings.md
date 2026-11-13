# UPD-09 安装关闭确认、原生工具链更新与设置入口

2026-11-13，按新增能力标准设计及单代理自审；实际状态只见PROGRESS。用户追加三项设置调整，本页统一范围后分片实施。

## 1 功能

交互安装检测到目标安装的Daymark正在运行时，显示原生确认“强制关闭并继续 / 取消安装”，说明未保存编辑可能丢失；默认取消。确认后强制结束已验证属于该安装的应用，再以原安装事务继续。静默安装不弹窗也不强杀，继续返回占用。正在提交/准备更新时不强杀事务执行者。

更新应同时交付应用与更新器；日常原生SDK/CLI/GUI变更不再要求完整重装。设置保留更新操作、移除主侧栏更新按钮及常规的退出整组，新增账号与同步分类；已有独立账号窗口和任务保存/重启门保留。

## 2 选型

复用C++20/Win32、NSIS3.13、Qt6.10.2及现有P-256签名/文件索引，无新服务、RPC、SDK依赖。采用不可变版本原生工具链和固定入口分派，不原地覆盖正在执行的EXE，不用taskkill按名字全杀。

## 3 架构

安装向导→安装工作线程在解包前调用setup-helper原生确认→SetupManager精确进程关闭→原有签名/租约/替换事务。不能在页面PRE/UI线程用nsExec同步等待带向导父窗口的TaskDialog，否则父窗口消息与子进程等待相互阻塞；SDK安装事务仍再次取租约。固定启动器/根CLI/GUI→已验签的当前工具链→原更新SDK。应用与工具链共用current.json的提交事实，准备阶段不改变运行者；失败不先激活新工具链。

## 4 模块

SetupManager持job/prepare/launch排他门，验证目标索引、进程映像/用户并持进程句柄，回调询问后结束并等待。向导只在非静默路径调用确认命令。NativeTools负责从签名seed/FILES解析版本工具、校验路径/散列和分派；Engine负责暂存/提交；Qt只沿既有控制器发命令。

## 5 协议及兼容

新增本地confirm-close-running命令，结果仍JSON format1；取消退出50，活动事务/权限失败保留具体错误。版本工具协议native-toolchain-v1：工具目录与已提交payload版本键一致，工具元数据由现有BootstrapPayload.v1签名绑定。旧根宿主未实现分派，首次获得此基线可能需要完整安装一次，不能声称已分发的3.0.2自动拥有新能力。后续普通工具升级不改稳定分派协议；未知签名/协议仍安全拒绝。数据schema/读写ABI升级继续独立评审迁移，不能以更新器自更新放宽旧数据保护。

## 6 接口与结构

SetupManager.closeRunning(target, confirm(count))：只结束持有句柄、映像属于可信索引且同当前用户的Daymark应用/空闲GUI；取消零终止。NativeTools.resolve(settings, role)返回经验证的工具路径或初装根回退；工具角色固定CLI/GUI，不能传任意EXE。当前版本有工具元数据时，缺失/损坏即失败，不能静默执行旧工具。工作进程PID/创建时间/管道身份继续校验，Qt接纳当前已选择的CLI路径。

设置追加稳定分类ID Account=6，显示顺序常规/账号与同步/任务与提醒/桌面组件/外观/数据与隐私/关于与帮助。账号页呈现当前身份、同步状态和管理账号入口，复用现有独立账号窗口中的完整安全操作，不复制密码表单或同步规则。

## 7 数据库与事务

不改任务schema11、同步协议或偏好key。工具只保存签名seed、FILES、有限原生EXE及其update-licenses许可，不含用户数据。准备先验证并落到不可变版本目录，current提交前仍使用旧工具；旧程序清理仅在新应用RUNNING后。未知/修改文件保留；工具被占用时延后清理，不破坏成功提交。

## 8 UX/HIG

HIG用户控制、明确危险动作、渐进呈现及UX-09/11/13：确认写清强制关闭及草稿风险，取消默认/Esc，原生高DPI与键盘；拒绝时保留安装目录。设置沿原双栏/单滚动/即时保存，七分类字级100～200%、浅深/窄窗、焦点/滚动恢复；主窗不常驻更新按钮。更新就绪反馈和原安全重启保存链保留。

## 9 可靠性

不杀其他安装/同名无关进程，不信裸PID，不关闭其他用户程序；从探测到结束持句柄防PID复用。安装SDK最终重新取租约，防确认后并发启动。活动更新事务拒绝；强杀只在用户确认后执行。工具签名/索引/版本必须一致，不接受本地指针提升信任；当前版本选择失败不自动降级数据或执行未知二进制。

## 10 验证

真实临时签名bundle：运行中拒绝、取消零杀、确认终止、同名不同目录保留、活动job拒绝、结束后修复/数据保留。版本工具A→B→C、根入口分派、CLI/GUI身份、篡改/丢失拒绝、提交前失败旧工具保留、提交后故障恢复及清理。Qt设置原生键盘/七分类/账号管理入口/更新单一归属、无退出组/主侧栏按钮。所有资料临时合成；macOS/Linux跳过，物理读屏/DPI另记UNVERIFIED。

实际NSIS验收沿`test_installer_flow`已有新装/修复/迁移/卸载链，Windows设置`DAYMARK_INSTALLER_ARTIFACT`为签名候选，再设置`DAYMARK_INSTALLER_INTERACTIVE_CLOSE=1`启用真实向导取消/确认路径。此模式要求没有现存Daymark卸载注册，强制`/NOINTEGRATION`并用QTemporaryDir；完成页取消运行勾选，只由夹具使用显式合成data-dir启动。校验取消时原进程存活、确认后结束及修复成功、两次操作后SQLite quick_check和合成任务均保留。不在macOS提供等价命令，未实现对应安装器。该选项是本地集成验收开关，不是生产强杀绕过参数。

## 11 自审

通过后按切片实施。修订：不能把所有占用统一当作可强杀应用；不能原地替换正在运行的更新器；不能用工具升级绕过schema/ABI；不能假称旧客户端已具新协议。固定入口仍是信任/分派小内核，其协议或密码算法破坏性变化须兼容桥接或完整安装，不能承诺无限自升级。单代理自审，非独立评审。

## 12 交付

先设置入口与用户链，再安装确认与SDK/原生测试，最后版本工具链与真实更新集成。每片构建/受影响测试，完成后文档记录；设计实施阶段未要求新发行，不自动推送发布。用户随后明确授权发布，现以3.1.0交付；不覆盖3.0.2，正式结果及首次完整安装边界见PROGRESS。功能完成与公开安装包验证仍分别记录。

依据：[Microsoft TaskDialogIndirect](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-taskdialogindirect)规定对话框所有者和取消结果；[TerminateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess)为异步终止，因此必须持句柄等待退出；[进程权限](https://learn.microsoft.com/en-us/windows/win32/procthread/process-security-and-access-rights)采用最小查询/同步/终止权限，不请求全权限或提权。

集成验收依据：[TDM_CLICK_BUTTON](https://learn.microsoft.com/en-us/windows/win32/controls/tdm-click-button)用于TaskDialog自定义按钮；[BM_CLICK](https://learn.microsoft.com/en-us/windows/win32/controls/bm-click)说明未激活对话框可能不响应模拟点击，因此后台QA发向自身向导的BN_CLICKED，不抢用户焦点。[NSIS3.13安装线程](https://github.com/NSIS-Dev/nsis/blob/v313/Source/exehead/Ui.c)与[nsExec等待实现](https://github.com/NSIS-Dev/nsis/blob/v313/Contrib/nsExec/nsexec.c)分别支持安装工作线程和同步等待边界的评审。
