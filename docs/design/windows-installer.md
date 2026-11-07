# SETUP-01 Windows完整安装向导与SDK事务

2026-11-06，用户授权“基于更新逻辑制作完整安装包”，遵循[新增能力标准](new-capability-standard.md)。FR-012/018、UF-01～16、UX-09/11/13。当前验证事实只见PROGRESS。

## 1 功能定义

Windows x64完整离线Setup.exe：欢迎→许可→安装位置→快捷方式选项→文件安装进度→完成/可选运行。默认当前用户LocalAppData/Programs/Daymark，不请求管理员，不安装常驻服务。界面中文，支持/S静默及/D目标目录（NSIS标准）。同一安装器可供首次安装及关闭应用后的完整引导层重装/升级；拒绝正在运行、降级和不受管的非空目录。

安装后的固定入口daymark.exe、根更新器/CLI与versions/<version-source>继续使用现有更新协议。数据目录仍为原有Qt应用数据位置；不迁移、不清空账号/任务。开始菜单/可选桌面快捷方式指向根入口；设置→应用可卸载，卸载保留个人数据及用户追加/修改的文件。

## 2 技术选型

NSIS3.13 Unicode Modern UI2仅作为构建时依赖，生成系统原生向导/卸载器，不引入Qt安装器运行库。NSIS主体及本次启用的zlib压缩模块采用zlib/libpng许可，完整COPYING随编译工具记录，产物包括安装器组件许可；不使用社区网络插件。工具用固定官方版本/散列从官方来源下载到忽略的build/toolchain，不全局安装。Inno/WiX备选会增加许可或工具链/运行环境配置，本次选择NSIS薄壳。

安装/删除/校验不由NSIS重复实现：新增纯C++Setup SDK（复用更新Engine/CNG/文件/租约），console helper提供JSON结果，向导只呈现意图与注册快捷方式。Qt应用不执行安装SQL。正式CI继续MSVC2022；本地工具链/分发候选的实际边界如实披露，不伪造干净机门禁。

## 3 架构

NSIS UI→静态原生setup-helper→SetupManager→现有Engine可信索引/签名、FileLock、受控文件端口。已验证managed离线目录→同卷新暂存目录→验证/初始化→受限替换安装根。安装器重装负责根宿主升级，普通自动更新继续只切payload；不放宽编译器runtime/schema/ABI的在线升级兼容门禁。

## 4 模块与生命周期

BundleVerifier：验证内嵌种子签名、FILES索引及每个文件散列，拒绝额外私有文件/重解析/不受管路径。SetupManager：目标本地NTFS/非系统目录、非相互嵌套、空间/权限、运行租约、同卷暂存/备份/回滚。Engine::ownedFiles：只从可信索引收集所有受管版本的候选程序文件；不扫描数据库内容。

Uninstaller：签名索引范围内且散列匹配才删除；未知/修改文件不删。InstallationCleanup：新版有真实存活RUNNING收据后清理上一安装的程序文件，备份中未知用户文件保留。Installer frontend：目录选择、可操作错误、当前用户快捷方式/卸载登记、安装完成运行选择；升级不强杀后台进程。

## 5 协议

本地CLI协议format=1，install --source <离线bundle> --root <目标> / uninstall --root / finish-install --root；输出JSON唯一终态，稳定错误key/Win32码/退出码，拒绝未知参数。无新网络服务/RPC。NSIS只根据成功退出值登记/删除自身注册。注册表HKCU限定Daymark自身键；不写HKLM/Root信任/文件关联。

## 6 接口/数据

SetupResult{version,installed,backupRetained,removed,preserved}，SetupManager.install(source,target)、remove(target)、finish(target)。Engine::ownedFiles返回(path,size,sha256)且来自已验签描述，不允许上层传任意删除列表。

install-receipt.json：format、source/version、前一安装的同父相对备份名、phase、root宿主指纹信息。备份名固定“目标名.previous-32位随机hex”，拒绝任意绝对/上跳路径。RUNNING确认须PID+创建时间+映像物理路径/当前generation一致，不仅信PID。

## 7 数据库与事务

无新schema/表，数据schema10/HTTP JSON v1不变。新离线目录使用现有current初始化（校验签名后生成install_id/generation）。旧程序完整保留至新目录提交，失败恢复旧目录；既有用户资料零写，安装预检仅合成资料。卸载不递归删除用户目录。状态/文件均在安装根和其同父受控暂存/备份内；硬崩溃残留保留可恢复旧目录，下一次安装明确恢复，不猜最高版本。

## 8 UX/HIG

HIG清晰反馈/用户控制/可恢复；Windows安装向导采用其原生控件和中文。安装位置可选，显示所需空间；重装要求先从托盘退出。失败显示具体原因和可重试操作，不输出个人绝对路径至公开日志。完成页可取消运行；卸载始终明确“个人任务和账号资料保留”。Tab/Enter/Esc、取消、100/200%及屏幕可用区分别验收，非Windows平台不冒称支持。

## 9 可靠性与安全

拒绝未验证bundle、NTFS外/UNC/重解析/受保护目录/磁盘根、源目标嵌套、未知非空目标、运行中程序和降级。目标当前应用、prepare、launch和runtime租约阻止安装与自动更新竞争。同父安装锁由启动器/Qt应用/更新器共享持有、安装器独占；确认内部租约空闲后释放内部句柄，在外锁保护下改名，避免Windows对子目录打开句柄的改名限制；根启动器没有放宽信任。

安装前复制到唯一同卷暂存并验证；替换采用旧根→备份、新暂存→目标，第二步失败回滚旧根，不先删旧程序。注册/快捷方式失败可重跑安装修复，程序与数据事实不依赖注册表。正常RUNNING后才清理备份的原索引程序；未运行保持备份。卸载只删散列匹配程序和明确归属的元数据，未知/修改文件保留并反馈，不使用RMDir /r用户根。

## 10 验证

QTemporaryDir内实际Windows/CNG签名夹具：新装、同版修复、升级、降级/非受管拒绝、运行租约占用、篡改/额外文件/重解析、提交失败回滚、未知/修改文件及合成数据库保留。真实NSIS生成程序：/S安装到隔离目录、快捷方式/卸载项、真实Qt首次启动、后台准备保持兼容、退出后重装、/S卸载、用户数据不变。向导真实页/取消/完成选择用原生窗口验收。正式CI来源/签名/部署门禁不被本地夹具代替。

## 11 设计自审

单代理技术自审：不把安装器当新更新服务；不修改数据库/账号归属；锁/签名/回滚复用已有端口；重装才替换根宿主；关闭应用而非强杀；卸载不递归删用户文件；已签名发布资产不可覆盖。关键风险为安装期间并发启动、目录交换后恢复和备份清理，须在SDK测试后才能标DONE。设计准入通过进入实施，实际支持/产物边界以PROGRESS为准。

## 12 交付

先修复组件交互；实现Setup SDK和离线NSIS壳/打包命令，生成安装候选及散列；运行受影响/完整测试和真实安装链，再补用户说明。安装器保留自签名/公共信任边界，未经请求不推tag或Release。CI生产安装器复用正式签名managed ZIP，不能把本地/测试bundle标为已通过独立干净机。

依据：[NSIS用户权限](https://nsis.sourceforge.io/Reference/RequestExecutionLevel)、[脚本参考](https://nsis.sourceforge.io/Docs/Chapter4.html)、[许可](https://nsis.sourceforge.io/Docs/AppendixI.html)、[Windows安装上下文](https://learn.microsoft.com/en-us/windows/win32/msi/installation-context)。

实现补充：反复重装但尚未启动时，通过有界 ancestors 列表保留全部前序备份引用，RUNNING 后统一清理程序；未知文件不删。安装器/内嵌卸载器均采用固定 pin 自签名。修复损坏程序只需可信元数据，不要求旧 EXE 散列正常。

### 2.1正式流水线准入评审

用户已授权测试后发布。沿现有Windows-only链扩展第五个原生setup helper，不另建服务：不可变tag完整CTest/QML lint→五宿主签名→无SDK/无私钥的独立runner部署验证→受保护发布job从同一签名bundle制作安装向导→QTemporaryDir实际新装、Qt运行来源审计、占用拒绝、修复和保留资料卸载→上传ZIP/Setup及校验和→最后上传ready manifest。Setup或其真实安装验证失败时不得先上传任何程序资产。

构建额外留存测试宿主及QtCore/QtTest依赖，绑定已核验candidate_run下载，不能拿别的run测试替代。发布job临时导入同一固定pin的PFX，仅存My证书库，finally删除；不导入Root。NSIS完整ZIP及编译器均校验ADR-035固定散列。源码release仅扩展精确命名的setup.exe及其sha256白名单；无覆盖已有tag/资产、无测试门禁降级。完成须GitHub真实构建、独立部署、安装及公网2.0.2→2.1.0更新证据，局部本地PASS不视为正式链通过。

单代理自审通过进入CI接线；保留正式更新包MSVC143与本地MinGW131隔离。安装与更新协议、schema10、HTTP JSON v1保持不变。
