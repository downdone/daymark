# Daymark 当前进度

更新：2026-11-06。当前版本 **2.0.1源码候选 / schema10 / HTTP JSON v1 / Qt6.10.2**。本文件只维护现行状态；逐轮原始证据保留在[历史记录](PROGRESS-HISTORY.md)，旧过程标题不再充当未完成任务。

## 本轮范围

用户要求完成PROGRESS任务，随后明确：**Linux、macOS跳过，先完成Windows功能性事项**。本轮不降级当前2.0源码、不覆盖历史tag/资产、不把正式签名发布、干净机/物理设备/8h性能测量混成业务功能欠账；不读取真实任务库，不上传私有资料，保持单代理。

用户随后明确授权“GitHub直接启用，全流程验证更新”。新增 **UPD-06**：配置真实仓库发布信任与Environment，以两个不可变递增版本验证公开GitHub下载、真实Qt重启、数据保留和旧版清理。授权覆盖必要的源码推送、仓库配置及新Release；不覆盖旧tag/资产，不上传本地私有资料。

## UPD-06 启用队列

| 顺序 | 事项 | 状态 |
|---|---|---|
| G0 | 核对远端、认证与正式发布缺口 | DONE：已明确选择downdone账号，API确认admin权限；远端main与四个旧tag已复核，2.0.1/2.0.2未占用 |
| G1 | 信任密钥、受控环境与干净Windows候选验证 | IN_PROGRESS：自有P-256和自签名代码证书已生成并真实验证；独立Windows CI候选与受保护Secrets配置待继续，未冒充干净机/公共信任 |
| G2 | 两个递增版本构建与不可变发布 | TODO：依次2.0.1引导包、2.0.2升级包；main编译通过后创建各自不可变标签 |
| G3 | 匿名资源验证与真实GitHub Qt升级链 | TODO：本地夹具证据不替代公开资源端到端 |

准备证据：普通GitHub API直连可用，旧本地Git代理不可达，未更改全局代理。已用官方资产API完成一次GitHub CLI下载并核对SHA256及GitHub, Inc.有效Authenticode；默认凭据查询因多账号未选中，明确downdone后认证和admin权限通过。windows-update-release已限制main/v*，四项Environment Secrets已加密配置，公钥变量/托管发行开关已启用。用户追加授权官方URL作为受保护CI Secret、临时文件构建后删除；源码仅示例。无私钥、密码或URL写入日志/源码。

本轮增量：MSVC标签包禁用windeployqt的编译器运行库隐式复制，改从VS2022正式Redist目录校验微软签名、14.3x/14.4x版本、PE x64与复制后散列，BUILD-INFO明确bundled及文件证据。源码目录/版本选择、Release布局与校验文件测试PASS；本机仅有VS18/VC145，未把该不同工具链当作VS2022实包验证，完整MSVC包保持UNVERIFIED。Release增量构建PASS；4项更新CTest PASS（21.73秒，`build/github-enable/update-tests.log/xml`）、14项发行元数据与4项更新发布测试PASS。设备登录等待数分钟仍未返回授权码，已结束仅本任务创建的登录进程；未产生API授权，不重复连接或绕过签名门禁。该早期登录缺口现已解除，自签名与远端配置进展以下续记录为准。

UPD-06远端首轮959e678：源码/隐私守卫与Go服务及真实Qt→Go TLS同步整条CI成功（run42155203595）；Windows完整构建run42155203610因Engine.cpp遗漏cwctype而MSVC C2039失败，未创建tag。补显式algorithm/cctype/cwctype/limits头；本机独立原生MSVC构建另查出manifest资源重复，改保留手写asInvoker/DPI资源、关闭MSVC重复生成；子项目独立UTF8编译固定。本机VS18/MSVC19.52（不冒充CI VS2022）三宿主编译及SDK/真实CNG两项CTest通过0.72秒，导入表OS-only、实际嵌入asInvoker/PerMonitorV2/CommonControls manifest均通过；日志msvc-native-manifest-final.log，原失败保留。修复提交9e5ead1及annotated v2.0.1已推送；main完整CI和同步CI成功。tag候选run42157174276完整构建/CTest/lint通过，但Package被CRT未知文件白名单拒绝，零资产上传。正式发行仍依赖候选打包、签名与独立部署。

UPD-06工具链恢复：合成地址复现了配置后立即删除URL文件会触发CMake自动重配并将官方地址置空；已改为保留至测试/打包后always清理，二进制必须含经规范化的Secret地址且元数据显式true。新增仅主分支调度的固定tag重建，源码SHA与工具SHA分别记录/验收；新候选仍完整测试，旧tag/资产不覆盖。CRT预检前置，可只运行轻量审计以取得真实未知文件名，不为该问题盲目重复20分钟完整构建。新增10项来源/发布策略与8项恢复守卫通过；重建的工具修复提交仅跳过重复普通push CI，受控重建仍执行完整正式门禁。

## 当前功能清单

UPD-06自签名切片：自有P-256公钥`daymark-d3fe49dbd972`已编入生产宿主，独立RSA3072证书仅CurrentUser/My，私钥/PFX/密码以CurrentUser DPAPI和owner/SYSTEM-only ACL封存。四个真实程序副本自签名与固定指纹验签PASS；改PE代码得到`TRUST_E_BAD_DIGEST 0x80096010`并拒绝，错pin/无签名拒绝，两处Root库前后不变，`self-signing-verification-final.log`。公共信任仍false，没有安装根证书。

自有密钥签署221文件本地验证包：生产CLI凭内置公钥初始化PASS；改清单、错签名域、错key、改payload均拒绝且零current指针；完整Qt运行库/隔离环境的`--update-preflight`退出0，`build/github-enable/owned-key-package-check/report.json`。该包明确local-fixture、合成GitHub ID，不能上传为正式包，不冒充公网升级/干净机。Release可信公钥构建PASS，更新4/4 CTest PASS（21.05秒），7项发布策略与14项元数据PASS，Python→PowerShell真实四文件签名门禁PASS；脚本/YAML解析PASS。源码仅公钥/固定指纹，不含证书或私钥；正式完整测试与独立部署门禁继续要求true。

保留的失败：探针最初误用CLI不支持的`--version`，改实际`capabilities`后补验；集合Add返回值混入记录，已抑制并断言每文件只产一条；Python跨进程数组参数被PowerShell拒绝，已改四个明确参数并补真实验签。失败在`self-signing-verification.log`、`python-signature-gate.log`，最终证据另存，未放宽校验。用户随后完成GitHub认证；API/Environment/Secrets配置成功。流水线已拆分签名、独立Windows runner的运行库/窗口/模块来源验证、报告绑定后发布三段，实际远端运行尚待源码提交。

| 功能/原任务 | 现行实现 | 证据与边界 |
|---|---|---|
| 本地任务/分类/标签/重复/提醒/备份 | IMPLEMENTED | 当前完整Windows基线39/39包含仓库、事务、查询、时间、工作区和导入导出；历史设计SYNC-01～04等不再表示缺代码 |
| 主列表/日历/草稿与多选 | IMPLEMENTED | qml_startup、contextual_workspace、continuous_calendar、calendar_interaction、task_list_interaction已通过；未丢身份/草稿/CAS与保存门 |
| 小窗/Mini/球及分类快查（UX-62～77、UX-72） | IMPLEMENTED | 独立分类上下文、默认全部未完成/可选今天、常用/搜索、快速添加/排序/快捷菜单、穿透恢复及统一呈现已实现；旧UX-72“只设计”由UX-77交付覆盖 |
| 账号与同步（SYNC-05～09、AUTH-08/09） | IMPLEMENTED | 官方/自定义URL、注册/登录、8字符密码、本地优先、跨端同步与冲突/资料隔离；真实Qt→Go TLS双工作区长链通过。另一台机器的具体超时原因仍需现场日志 |
| 完成面板分类/标签检索（QUERY-01） | IMPLEMENTED | 目录通知、按ID选择、归档分类与标签/分类交集、direct_area_id归属已修复，服务/工作区及真实QML覆盖 |
| 全局日志与诊断（OBS-01） | IMPLEMENTED | 请求ID、Sxxx-Cxxx、Qt/TLS底层证据与模块事件、本地有界/脱敏日志、详情/复制/日志入口；不上传，不猜Qt未提供的原生码 |
| Windows在线更新（UPD-01～05、UF-01～16） | IMPLEMENTED / WINDOWS-CHAIN-PASS | SDK/原生CLI/Win32薄UI、启动自动prepare、旁路Ready、保存退出、受限启动/提交/RUNNING后清理及恢复已实现；最新Qt应用链42.651秒PASS，正式GitHub启用条件见下表 |
| 更新功能收尾（CLOSE-01） | IMPLEMENTED | 发布说明固定项目链接、手动重试与自动24h退避分离、验证阶段取消/下载失败退避、坏状态/命令启动失败提示、每候选通知、窄窗自适应已补齐；新增真实组件及受影响UI回归已通过 |
| Windows流水线 | CONFIG-IMPLEMENTED | 普通桌面、账号同步和发行候选默认Windows；Python3.13/Qt6.10.2固定，保留旧Unix资产恢复；YAML与发布守卫已验证，未远端运行/推送 |
| 已实施性能优化（PERF-01 P1～7） | IMPLEMENTED | 日期范围/快照索引、模型增量与重排、提醒O(1)到期判断、刷新分工与查询中间结构已落地，当前性能测试通过；整应用/GPU/长期SLA未宣称通过 |

## CLOSE-01 收尾队列

| 顺序 | 当前事项 | 状态 |
|---|---|---|
| F0 | 历史与当前任务去重 | DONE：按后续交付追踪，保留原始失败与未验证范围 |
| F1 | Windows现行功能总集成 | DONE：本轮开始时完整39项CTest一次通过，416.87秒 |
| F2 | 补齐残余功能并验证 | DONE：更新SDK/Qt适配器4项、受影响UI4项、真实原生关于页、Qt/Go TLS和最新Qt应用升级链全部通过 |
| F3 | 工程文档收敛 | DONE：350KB原始记录已归档，正文一致性验证通过；当前页仅保留现行功能与本轮证据 |
| F4 | 本地开发构建与运行入口 | DONE：正常版本2.0.0已恢复并验证，四个程序散列已记录；本地入口更新、旧入口保留备份，旧便携包与远端资产未覆盖 |

## 本轮实现和真实证据

- 基线：Release构建通过；完整39/39 CTest PASS，416.87秒，`build/close01-full-tests.log/xml`。后续增量不冒充已由该基线覆盖，也不重复无关watchdog。
- 更新组件：`update_windows_flow / update_qt_controller / update_windows_crypto / update_sdk` 4/4 PASS，23.25秒，`build/close01-update-tests.log/xml`。覆盖自动取消候选不重下、主动重试、缓存验证取消、坏JSON不显示Ready、初始及新候选通知、固定发布链接、失效CLI与子进程释放；未真的打开浏览器。
- 设置/UI回归：`qml_startup / calendar_interaction / desktop_surfaces / brand_rendering` 4/4 PASS，126.17秒，`build/close01-ui-regression.log/xml`。新增控件与容器支持宽窄/200%，保留原断言和时间预算。
- Windows原生关于页：退出0、4003ms、三窗、QML warnings=false，`build/close01-native-about-final.log`及同名截图目录；浅色/深色200%窄窗与实际焦点已检查。
- 最新Qt升级链：`build/close01-real-qt-update.txt`，3 passed/0 failed/0 skipped，42651ms。实际旧Qt进程→启动自动发现/下载→旁路Ready→保存退出→新Qt受限启动→持久提交/RUNNING→旧exe清理；SQLite完整性与合成任务保留、未知/修改文件保留均通过。使用临时签名和本机发行夹具，不冒充真实GitHub或干净机。两个payload均从最新源码构建；2.0.1只为测试主入口宏，结束已清空宏并恢复正常2.0.0，`build/close01-local-build-metadata.json`记录版本及四个程序散列。
- 服务：本轮完整Go测试只有真实Qt用例因沙箱拒绝测试专用Windows凭据API失败（`build/close01-go-tests.log`）；其他服务用例通过。按同一隔离TLS夹具在正常用户条件仅补该失败项，PASS 55.57秒（`build/close01-qt-go-native.log`），覆盖原生凭据、注册/登录、来宾导入、快照/推拉、离线队列、冲突、1000任务原子导入、大批引用及资料隔离；`go vet ./...` PASS。不以本机TLS通过宣称另一台机器网络已修复。
- 静态：两项QML lint、94 QML样式、四主题颜色、106 schema及诊断注册表、12依赖散列、7恢复+14发布元数据用例通过。工作流解析与Windows矩阵核对通过；没有执行远端工作流。
- 保留的失败：新增设置探针最初在资料写入忙碌时申请打开被原保存门挡住，误抓隐藏窗口旧布局；诊断日志证明窗口内容860×680而QWindow已被探针改为640×500。已分开创建/打开、只打开一次并断言真实可见，再按实际窗口做缩放/焦点验证。早期图像只算绘制证据，不冒充窗口输入验收。另一次最终门禁因探针提前关设置只剩两窗失败，保留日志，改保留实际设置窗口满足原三窗门禁；没有放宽门禁或重跑至碰巧通过。全部失败在`build/close01-native-about*.log`，最终结果独立记录。

## 单列的发布与现场验收

| 项目 | 当前边界/解除条件 |
|---|---|
| 正式GitHub自动更新启用 | 自有签名/信任公钥及受保护Environment/Secrets已配置；仍需独立Windows CI实际证据与两个新Release资源。保持不上传本地夹具、不移动旧tag。见[准备步骤](design/update/github-prerequisites.md) |
| 另一台机器注册超时 | 诊断功能完成；现场根因尚未确认，需要该机使用新版复制诊断或本地日志 |
| Windows独立干净机/物理DPI/多屏/读屏/睡眠与8h | 非本轮功能实现范围，保持未验收；单元/合成事件/当前Windows截图不能替代 |
| Linux、macOS | 按用户2026-11-06指示跳过，本轮不实施或验收 |
| 旧REL-181/1.8.0发布过程 | 属历史发行线，2.0已合入相关源码；不为了消除旧状态降级当前源码或覆盖不可变tag。公开发行状态以对应历史证据为准 |

## 历史追溯与操作

[原始实施日志](PROGRESS-HISTORY.md) · [Windows更新实现](design/update/implementation.md) · [日志与底层诊断](design/runtime-diagnostics.md) · [功能验收契约](design/update/functional-spec.md)。本地开发入口为`build/Launch-Daymark.cmd`，旧入口保留`build/close01-previous-launcher.cmd`；未自动启动或结束用户程序。请先退出旧托盘实例再运行新入口，避免单实例机制激活旧程序。该入口依赖本地开发Kit，不是可转发便携包；旧dist ZIP保持原样；本轮源码已推main并启用更新信任/受保护环境，新的tag/资产状态以上述UPD-06为准。
