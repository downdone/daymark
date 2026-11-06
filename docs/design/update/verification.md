# 验证与分片开发门槛

以下是验证契约，Windows实现与已执行用例见[实现入口](implementation.md)和[当前进度](../../PROGRESS.md)，不能再把早期“只设计”当当前状态，也不能把部分PASS外推为整张矩阵通过。单元测试证明规则，组件测试证明真实库/IO/进程协议，真实Windows与无SDK/干净机证明平台行为；不能互相冒充。2026-11-06用户明确本轮跳过Linux/macOS、先完成Windows功能。

## 1. 单元与模块测试

| 编号 | 输入/故障 | 必须结果 |
|---|---|---|
| UT-01 VersionPolicy | 2.10/2.9、前导零、负数、溢出、prerelease、相等版本 | 数值正确，无静默截断/同版本覆盖 |
| UT-02 Eligibility | runtime/OS/arch/ABI/schema/bootstrap单项错、半发布、重复资产 | 对应ManualRequired/发布错误，零prepare |
| UT-03 StrictJSON | 重复key、错型、坏UTF8/BOM、过深/超长/大整数 | 原生严格拒绝，不产生可信对象 |
| UT-04 Signing | 两个签名域交叉、改一字节、错key/签名长、P1363边界 | 原生CNG实际向量验签；Qt适配一致 |
| UT-05 State/clock | 取消后旧回调、revision冲突、时钟回拨、冷却到期 | 一操作/单调超时/正确取消，不重复下载 |
| UT-06 RecoveryPlanner | current/journal/last-good各状态、提交存在消息缺失 | 提交前可恢复，提交后绝不自动退旧 |
| UT-07 Cleanup | 无RUNNING、目录有未知/修改文件、已部分删除 | 不提前删除，不递归删用户文件，幂等 |
| UT-08 Presenter/CLI | 所有Snapshot/错误/locale/JSON输出 | 语义一致、动作正确，无任务/URL泄露 |

所有注入端口不接真实用户目录；数据库使用QTemporaryDir合成资料。测试不只断言getter/镜像实现，要证明失败不变量与外部行为。

## 2. 真实组件集成

| 编号 | 组合 | 检查点 |
|---|---|---|
| CT-01 HTTP | 受控TLS夹具+Qt/WinHTTP两适配 | 200/302/304、缓存坏/丢失、403/429、预算、断网/慢包、代理/PAC、HTTP/恶意重定向拒绝 |
| CT-02 Resume | 实际part/收据+Range响应 | 200不能追加、206偏移/总量/ETag、416哈希、资产换代/过期CDN URL |
| CT-03 Archive | 实际miniz+真实磁盘 | ZipSlip/ADS/保留名/链接/大小写重名/炸弹/截断/额外文件；计数/字节/索引精确一致 |
| CT-04 StateStore | 原生写盘+拒写/磁盘满/替换失败注入 | current保持旧完整值或新完整值，坏状态不猜测，CAS不丢取消 |
| CT-05 Locks/IPC | 多个独立原生子进程 | prepare唯一、apply gate阻新启动、runtime交接不死锁、坏DACL/nonce/PID复用/残帧拒绝 |
| CT-06 StartupGate | 实际Qt入口+临时多资料DB | COMMIT前正常open/备份/同步/提醒/物化均未启动；只读失败零业务修改 |
| CT-07 Hosts | 同SDK的CLI/Win32UI/Qt适配 | 同一状态/错误/退出码，GUI不在worker线程更新，关闭观察者不杀apply |
| CT-08 Preflight | 实际payload在SyntheticPreflight | QTemporaryDir与禁同步、隐藏QML/插件/合成SQLite资源（真实帧在受限启动单独验证），退出码和收据校验 |

本地TLS夹具只注入测试信任，不在生产启用忽略TLS的开关。终止/磁盘故障注入仅测试目标可用。不能把断言失败改为重开菜单/重试网络直到通过，预算变更须有实际时序依据。

## 3. Windows端到端与故障点

E2E-01：从签名N/N+1合成发行包开始，真实旧应用加载Qt，RuntimeReady触发检查；后台prepare时旧程序可读写任务且原DLL不变；Ready后执行宿主退出；稍后点击重启/下一次正常启动均能切换。截图、真实进程/模块来源与状态记录对应。

E2E-02：有草稿/保存冲突/断网outbox、其他资料进程/同资料另一安装、取消待应用、关闭自动更新、只启动托盘、低空间及目录只读；每个失败仍能正常用旧版且不会丢任务。

E2E-03：在以下边界前后独立终止进程并从根入口重新启动：Ready落盘、新目录提交、SwitchPending、旧进程退出、runtime独占释放、新进程获取共享租约、READY、current committed、COMMIT发送/接收、生产初始化、RUNNING、CleanupEligible、CleanupStarted、每个旧文件删除、previous清空。逐次断言当前唯一版本、资料完整、无假成功/混版；不是只模拟函数返回false。

E2E-04：被杀毒/其他进程占用旧DLL、旧目录含用户文件/修改的Qt库、current/journal损坏、worker副本丢失、IPC断开/写权限丢失；清理只能延期/跳过，不提权/强杀/误删。COMMIT后启动失败保留旧文件但不自动恢复旧DB或降级写者。

E2E-05：匿名真实GitHub API/ZIP/CDN重定向，清单分时上传、实际限流/代理；一次有界验证保留失败。无SDK环境验证引导器/CLI无Qt依赖，GUI系统控件正常，payload DLL均来自当前版本或允许系统目录。正式最低Windows/VC运行库缺失/具备分别用干净机证明。

真实公开发行的显式验收入口（Windows，两个版本均已完成发布后运行）：

```powershell
python tests/update/github_flow.py --work <新的隔离证据目录> --old-version 2.0.1 --new-version 2.0.2
```

它不读GitHub凭据、不注入测试信任/API/测试payload，匿名下载既有正式引导资产并核对GitHub散列及已审查的自签名证书指纹，再由生产CLI内置P-256公钥验证引导。真实Qt实例仅使用该新证据目录下的合成SQLite，启动自动prepare实际公网新版，公开CLI请求Qt保存退出/重启，断言COMMIT/RUNNING、旧exe删除、未知/修改文件保留及合成任务完整性；report绑定实际releaseId/assetId/sourceRevision。测试结束仅结束该隔离安装根中的自有进程，保留报告与失败状态，不重试至通过。该脚本不加入普通CTest/PR，防止常规测试主动联网或启动公开版本；未执行不得写PASS。本地夹具与此入口分别记录。

UI验收包括浅深/100～200%/窄窗/键盘/辅助名称、主窗隐藏与原生窗口、物理DPI/读屏；GTK X11与Wayland、AppKit各后续单列，不能把Windows截图当三端通过。

## 4. 实施顺序

需求追踪：UF01/02→UT01/02、CT01、E2E01；UF03/04→CT02/03/07/08；UF05/06→E2E01/03；UF07→CT06/E2E02；UF08/09→UT06/07、CT04/05、E2E03/04；UF10/11/12→UT05/08、CT01/07、E2E02；UF13→CT05/E2E02；UF14→CT03/04、E2E04；UF15/16→UT02、CT06、E2E02/05。每个需求同时检查对应功能表的失败不变量，不只证明happy path。

| 切片 | 输出 | 进入下一片的门槛 |
|---|---|---|
| U1 SDK规则/发现 | Qt-free类型、严格JSON/验签/策略/状态；check/status CLI；Qt只读适配 | UT01～05，真实CNG向量/受控HTTP；核心与CLI编译 |
| U2 根引导与状态 | 稳定启动器/两个宿主、租约/IPC/安全写盘、签名初装描述 | CT04/05，真实双进程正常启动/坏状态恢复，无Qt导入 |
| U3 自动后台准备 | WinHTTP/ZIP/流式校验/Ready/取消；RuntimeReady接线 | CT01～03/08、prepare完成无驻留、不改current，异常包零安装 |
| U4 切换/数据写门/清理 | Qt受限启动、保存退出、commit/RUNNING/恢复/清理 | CT06、E2E01～04、完整资料/保存/同步回归 |
| U5 薄UI/CLI完整 | Win32进度/Ready/错误、CLI取消/apply/退出码、关于页状态 | CT07，实际输入/浅深/200%/窄窗、无SDK运行 |
| U6 发行链 | 清单签名生成/资产白名单、固定版本、发布说明/操作指南 | 完整构建/CTest/lint/许可/签名/干净机、E2E05，明确授权才发布 |

macOS/Linux按[轻量UI设计](native-ui.md)后续独立交付，不在Windows切片中堆空实现。SDK规则可在两平台编译/单测，但缺平台安装端口须返回UnsupportedPlatform。Qt界面未来可接同SDK，本期不并行维护两套独立更新器GUI。

## 5. 文档与实施证据

每片记录源码SHA、工具链、用例/输出、平台/失败边界到PROGRESS；不在多份设计文档维护当前状态。Windows沿既有build-windows/test-windows入口，macOS沿build-macos.sh；具体target接入后写可复制命令。性能同配置比较GUI/CLI包体/启动/峰值私有内存、任务输入延迟，不用理论数字冒充实测。

本表定义目标矩阵，不代表全部已通过；实际已运行、失败保留及外部未验证项只记录于PROGRESS。

显式Qt Test入口通过`DAYMARK_UPDATE_PUBLIC_GITHUB=1`选择公共验收，`DAYMARK_UPDATE_PUBLIC_REPORT_DIR`指定新的证据目录，再运行`test_update_flow signedWindowsTransaction`；全部SQLite位于其QTemporaryDir。默认不设该变量时继续原本地夹具，180秒预算不变。公共模式包含两次真实网络下载及资源发布等待，外层最多90分钟，独立工作流整体45分钟；记录下载、等待和实际Qt切换时间，不将等待计作产品切换性能。工作流`.github/workflows/verify-windows-update.yml`不使用发布Environment/Secrets，实际生产子进程清除SDK/Qt路径和CI Token，并审计Qt/SQL/CRT的映像来源。导出的ZIP必须来自实际下载并匹配签名清单，合成DB不进入artifact。
