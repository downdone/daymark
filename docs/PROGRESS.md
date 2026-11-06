# Daymark 当前进度

更新：2026-11-06。**2.0.2 已发布 / schema10 / HTTP JSON v1 / Qt6.10.2**。当前状态只维护在本页；原始过程、失败及修复保留于[历史记录](PROGRESS-HISTORY.md)。

## 已完成范围

用户授权完整实现Windows功能、启用GitHub并验证实际更新，选择项目自签名，允许官方地址通过受保护CI Secret编译。当前功能收尾CLOSE-01、发行启用UPD-06均完成；Linux/macOS按用户指示跳过。未覆盖用户数据库、不上传真实任务/隐私日志、不移动旧tag/资产，保持单代理。

| 功能 | 当前结果 | 证据与边界 |
|---|---|---|
| 本地任务/分类/标签/日历/重复/提醒/备份 | IMPLEMENTED / WINDOWS-PASS | 当前完整Windows CTest 40/40包含仓库、事务、查询、时间、工作区和导入导出 |
| 主窗/小窗/Mini/球及分类快查 | IMPLEMENTED / WINDOWS-PASS | 分类默认全部未完成/可选今天；快速添加/排序/菜单/穿透恢复及统一呈现已实现，相关真实QML/原生回归已通过 |
| 账号/同步/完成面板筛选 | IMPLEMENTED / WINDOWS-PASS | 官方/自定义URL、8字符密码、本地优先、资料隔离、冲突及分类/标签交集；真实Qt→Go TLS同步CI成功 |
| 全局日志与诊断 | IMPLEMENTED / WINDOWS-PASS | 本地有界脱敏日志、请求ID、Sxxx-Cxxx、Qt/TLS底层证据、复制详情/日志入口；不上传 |
| Windows在线更新 | PUBLISHED / PUBLIC-GITHUB-PASS | 原生SDK/CLI/Win32薄UI、自动prepare、保存退出、受限启动/COMMIT/RUNNING/清理；真实2.0.1→2.0.2通过 |
| 已实施性能优化 | IMPLEMENTED | 已交付索引/快照/增量模型/提醒判断等，现行测试通过；整应用/GPU/长期SLA未宣称已验收 |

## UPD-06 完成队列

| 事项 | 状态 |
|---|---|
| G0 仓库/权限与不可变源码核对 | DONE：明确downdone账号，API确认admin；旧tag未改，源码身份分开记录 |
| G1 自有签名/受保护环境/独立部署 | DONE：P-256公钥内置；四程序自签名与固定指纹验证；独立Windows实际Qt/CRT来源验证；不导入Root/TrustedPublisher |
| G2 两版正式资源发布 | DONE：2.0.1与2.0.2各有ZIP、ZIP.sha256、SHA256SUMS、更新JSON及签名五项公开资源 |
| G3 真实GitHub升级 | DONE：独立Windows从匿名公开资源升级，生产Qt保存重启、持久提交/RUNNING、删旧程序与保留任务全部通过 |

## 发行身份与资源

| 版本 | 精确生产源码 | 构建/最终发布 |
|---|---|---|
| [2.0.1](https://github.com/downdone/daymark/releases/tag/v2.0.1) | `9e5ead18f7e4d2a990f0eccbe8096dec48dfd0e1` | 固定源码[重建42160035601](https://github.com/downdone/daymark/actions/runs/42160035601)；[签名/独立部署/发布42161946172](https://github.com/downdone/daymark/actions/runs/42161946172) |
| [2.0.2](https://github.com/downdone/daymark/releases/tag/v2.0.2) | `0bdaca92b5a1158df035c830dd926307464125d6` | [标签构建42161583632](https://github.com/downdone/daymark/actions/runs/42161583632)；[签名/独立部署/发布42164523268](https://github.com/downdone/daymark/actions/runs/42164523268) |

最新[Windows x64便携包](https://github.com/downdone/daymark/releases/download/v2.0.2/Daymark-2.0.2-windows-x64.zip)：44,041,439字节，assetId `715946759`，SHA256 `0b94cb2245b4070e164336570e05300aa269f5b45b866959100f9388aa06798b`。解压完整目录，运行根目录daymark.exe；versions内的payload不单独移动。无更新器的旧平铺包需先手动安装一次引导布局。

首次tag候选的打包失败未上传资产；恢复使用同一2.0.1源码、独立记录的已审查main工具SHA和run身份，重新完整测试，没有移动tag或把工具SHA冒充源码。2.0.1内部README保留原候选文案的问题已在2.0.2最终说明中更正，旧包没有覆盖。

## 真实验证

- **2.0.2完整Windows CTest 40/40，0失败/0跳过，389.75秒**；[桌面CI](https://github.com/downdone/daymark/actions/runs/42161582762)和[服务/真实Qt同步CI](https://github.com/downdone/daymark/actions/runs/42161582820)成功。本地2.0.2 Release链接完成，显式隔离版本探针退出0；更新组件4/4与公共验收包装器的原本地夹具回归通过。
- **实际公开GitHub 2.0.1→2.0.2 PASS**：[Windows验收42164528281](https://github.com/downdone/daymark/actions/runs/42164528281)、[脱敏结构化报告](evidence/windows-update-2.0.1-to-2.0.2.json)。源控证书pin、生产CLI内置公钥、匿名API/真实ZIP/真实Qt均使用正式资源；无fixture/API/信任覆盖。全部SQLite位于QTemporaryDir，未登录账号或上传任务。
- 实际Qt升级阶段**22.44秒**；完整验收94.439秒（包含60.631秒等待新版资源公开、1.23秒引导包下载及验证）。旧/新各29个Qt/SQL/CRT模块均观察到来自对应签名payload，子进程清除SDK/Qt搜索路径和CI凭据；新进程COMMIT/RUNNING与清理完成，旧exe删除，未知/修改文件保留，合成任务/SQLite完整性保留。
- 自签名验证：四个实际PE签名通过固定SHA256证书pin与独立CustomRootTrust校验；错pin、未签名、改PE、改清单/载荷、错key和错签名域均拒绝。改PE得到`TRUST_E_BAD_DIGEST 0x80096010`；系统/用户Root库不变。`signed`旧公共签名字段仍false，`authenticode.policy=self-signed-pinned/publicTrust=false`明确，不伪造Windows公共信任。
- 工程修复：MSVC显式标准头、独立UTF8、原生manifest去重；CRT十成员（含vcruntime140_threads.dll）逐个微软签名/版本/x64/散列校验；官方URL输入保留至所有CMake构建/测试/打包结束再清理，实际EXE必须包含配置；签名/部署报告与源SHA、工具SHA、触发run、全部文件散列绑定。原始失败均保留于历史与忽略的build证据。
- 收尾验证：发布元数据/隐私守卫15项通过，覆盖强制跟踪本地配置/密钥及大小写变体；198份Markdown相对链接和12项固定依赖来源散列通过。历史文件保留原字节前缀，再追加本轮原始过程。

## 密钥与GitHub配置

受保护Environment `windows-update-release`限制main/v*；更新私钥、代码PFX/密码、官方URL均为加密Secrets，公钥配置与托管开关已启用。源码仅含公钥、证书指纹与URL示例。自有私钥从临时build目录移入忽略的`config/signing.local`，owner/SYSTEM-only ACL和CurrentUser DPAPI验证通过，密钥身份未变，无明文私钥遗留；签名证书仅CurrentUser/My，不在信任根库。DPAPI绑定用户/机器，不能把普通文件复制当跨机恢复。

清单/索引签名认证更新来源；项目自签名EXE**不是Windows公共发布者证书**，仍可能有SmartScreen/未知发布者提示。见[自签名契约](design/update/self-signing.md)、[受控重建与GitHub配置](design/update/github-prerequisites.md)。

## 明确保留的外部边界

| 项目 | 真实状态 |
|---|---|
| 本机GitHub CDN | 首次匿名下载发生90秒读超时，未启动Qt。一次64KB有界范围读取HTTP206/7.207秒；未改TLS/代理、未盲目重跑。本次公网全链PASS来自独立Windows，不能冒充本机CDN恢复 |
| 另一台机器注册超时 | 诊断功能已实现；现场根因未确认，仍需该机新版详情/日志 |
| 物理DPI/多屏/读屏/睡眠/8h及Windows10最低build | 本轮没有相应实机验收；托管windows-2022和当前Windows局部验证不能替代，不阻塞已获授权的Windows功能/CI范围发布 |
| Linux/macOS | 按用户指示跳过，不实施、不构建、不宣称通过 |

[CLOSE-01与UPD-06完整原始历史](PROGRESS-HISTORY.md) · [文档导航](README.md) · [更新实现](design/update/implementation.md) · [诊断契约](design/runtime-diagnostics.md)。本地开发入口仍为build/Launch-Daymark.cmd，依赖开发Kit；可转发发行包请使用上述正式完整ZIP，不复制build/bin单个EXE。
