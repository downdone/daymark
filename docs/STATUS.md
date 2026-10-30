# 当前状态

本页是**唯一的状态摘要**，每次版本/门禁变化时更新。逐轮详细记录见[进度日志](PROGRESS.md)。

## 版本与数据

2026-10-30用户授权将UX-64～68已验证源码提交到GitHub main；本次仅源码同步，不创建tag或更新公开安装包，发布恢复脚本的既有本地改动单独保留。下列“未推送”描述各轮本地验收时状态，远端CI验证独立于本机Windows证据。

| 项 | 值 |
|---|---|
| 应用版本 | 1.8.0（`CMakeLists.txt` 与 `DAYMARK_VERSION`） |
| 数据库 schema | 10（追加账号/同步元数据，历史1～9未覆盖） |
| 平台目标 | Windows 10/11 x64、macOS 13+ |
| 技术栈 | C++20、Qt 6.10.2（LGPLv3 动态链接）、SQLite（QSQLITE） |
| 可选服务 | Go1.27.0单体 + SQLite3.54.1；HTTP/JSON v1，见[服务指南](../server/README.md) |

## 侧栏底部工具与账号名（UX-68，2026-10-30）

最近删除与账号收到底部单行，与设置同行；未登录/退出/登录失效显示“登录”，已认证显示账号名，长名在固定文字槽省略。折叠侧栏保留三个图标，修正200%字标边距撑宽；侧栏备份快捷按钮移除，设置→通用→备份与数据统一包含周期和管理入口。Qt6.10.2 Release、两项QML lint、最终完整26/26 CTest PASS（310.23秒），Windows默认GPU侧栏专项通过（3窗/无QML警告），样式与文档链接检查PASS，证据见PROGRESS。仅本地构建、未推送；macOS/物理多屏/读屏未验证。

## 桌面菜单实体与层级（UX-67，2026-10-30）

菜单移出动画/禁用内容层，以稳定根承载、保持来源锚点；打开菜单完成当前过渡。桌面Window菜单始终实体且置顶，宿主层级恢复后保持Popup在上方。Qt6.10.2 Release、QML lint、最终26/26 CTest PASS（296.36秒），Windows原生六组菜单场景及三层级裸窗口恢复测试通过，并检查不透明截图像素；实际证据见PROGRESS。旧运行实例保持不变，需退出后运行本地启动器；未推送。macOS/物理多屏/读屏/真实其他应用遮挡未验证。

## 球与小窗动效（UX-66，2026-10-30）

球↔小窗180/150ms角锚点揭示，内容固定大小并延后淡入/提前淡出；快速反向接续当前矩形/透明度、减少动态即时切换，缩球完成才提交原生小尺寸。最后窄Panel容纳Mini边界及合成离开事件修正后，Qt6.10.2本地Release、两项QML lint、最终完整26/26 CTest PASS（268.11秒），默认Windows GPU的停靠+动画组合原生专项通过（3窗/无QML警告）。样式与文档链接检查PASS；物理多屏/读屏/底层桌面点击和macOS仍UNVERIFIED，证据见PROGRESS。本轮仅本地构建，未推送，公开1.8.0包不包含这些改动。

## 悬浮球停靠修正（UX-65，2026-10-30）

球为常驻入口：点击临时展开任务小窗，离开且无交互后约1000ms回球；菜单/设置明确选择小窗才常驻。原球位置在hover成Mini后仍可点击。Qt6.10.2本地Release/lint及完整26/26 CTest PASS（281.90秒），Windows原生停靠专项连续通过，3窗/无QML警告，最终桌面链复验通过；物理多屏/读屏及macOS未验证，详见PROGRESS。本轮未推送，既有v1.8.0公开包不包含本地未提交改动。

## 本地三形态统一（UX-64，2026-10-30）

当前工作区已实现[用户确认方案](design/desktop-widget-modes-proposal.md)：三形态独立启用/直接切换、独立尺寸与固定逻辑锚点、三种拖动、自动收展及键盘/菜单保持。Windows Qt6.10.2 MinGW Release构建/lint及最终完整26/26 CTest PASS（261.10秒）；原生forms/compact专项退出0、无QML警告。原生全链另在历史样式画廊stage89失败，物理输入/多屏/读屏及macOS仍未验证，具体边界见PROGRESS。本轮仅本地开发构建，未推送；下列既有1.8.0包/tag/CI结果不包含本轮未提交变更。

## 构建与测试

| 项 | 1.8.0本轮状态 |
|---|---|
| Windows MinGW Release | c02f36e完整构建及26/26 CTest PASS，269.96秒 |
| QML lint | daymark_qmllint、daymark_style_qmllint PASS |
| 源码发布守卫 | 14/14、版本/隐私、文档/样式/颜色/同步契约 PASS |
| Windows本地包 | 1.8.0常规包PASS：SDK隔离原生437阶段、清单/ZIP/SHA256通过，非预览模式 |
| Windows MSVC / macOS / Linux CI | a7ae585三平台构建/完整测试/lint已通过；候选打包因SPDX格式假定失败，正在恢复 |
| Windows MinGW Debug | 本版未复验 |

之前三平台全绿[run40855720727](https://github.com/downdone/daymark/actions/runs/40855720727)属于8c9d41f；随后c9d6b71的Linux在desktop_surfaces stage117失败。本版保留真实点击及接线断言，先等设置分类/字号布局并reveal至视口后只点击一次；本机完整回归通过。新tag将触发三平台完整构建/CTest/QML lint及候选归档，尚不能预称通过。

账号同步先前Go test/vet/race、Windows真实Qt双工作区↔Go TLS完整故障矩阵及Windows↔WSL普通HTTPS服务已PASS，具体范围见PROGRESS及[同步验证](sync-verification.md)。本次未改变服务生产代码/协议，不以本机联调代替生产部署或macOS原生验收。

## 自助注册（SYNC-06，2026-10-29）

提供POST /v1/auth/register及独立账号窗口中的“注册并同步”；旧服务器配置默认关闭，示例registration_enabled=true。用户名/密码注册不表示邮箱验证。Go全包test/vet、注册相关race、真实Qt注册后双端同步已通过；Windows Release曾25/25；后续完整探针在既有UX38 stage76失败。注册独立切片连续3轮、浅色/深色200%渲染检查通过，最终针对性回归及证据见PROGRESS。前述6cfc6d7远端基线不包含本轮注册，注册提交的CI另行核验；macOS原生和生产部署未验证。

## 独立账号窗口（SYNC-07/UX-61，2026-10-29）

主窗侧栏及折叠态提供独立账号入口，设置移除账号内容；[设计、实现前评审与实现后复核](design/account-window.md)已记录。Windows Release构建/lint、18段账号状态及接线、真实Qt/Go TLS联调PASS。完整回归曾24/25，修复样式预览裁剪定位后相关套件复验通过；最终desktop_surfaces/account_access 2/2 PASS，不将其称为单轮26/26。原生/平台边界见下方及PROGRESS，新提交CI需单独核验。

## Mini悬停球与紧凑日历（UX-62，2026-10-29）

设置→桌面组件选择Mini后可开启闲置收球；悬停恢复条形，菜单/拖动/键盘保持展开，尺寸持久化与平台失败回滚有临时设置单测。独立日历默认560×540、最小360×380，正文/辅助/标题12/11/15及月格、时间轴、年视图统一收紧；已有手动窗口尺寸保留。设计与复核见[UX-62](design/compact-desktop-surfaces.md)。完整26项、Windows合成输入与实际渲染、SDK隔离原生包验证PASS；物理平台边界仍见下方。

本地新包Daymark-1.1.0-windows-x64-local-20261029-144244.zip（39,466,619字节），源码44661df，未签名；路径、校验和、原始失败与最终证据见PROGRESS，不代表正式远端二进制发布。

## Mini 展开动效与漂移修正（UX-63，2026-10-29）

按用户确认采用160ms OutCubic揭示，固定左上锚点和目标宽高；减少动态直接切换。C++统一正常宽度及Mini高度、先更新模式/约束再提交几何，QML不写Window高度；不再记忆过渡帧尺寸。共享菜单忽略同值原生尺寸回执，真实移动/缩放仍关闭。30轮污染帧往返、屏边、快速反转和实际交付exe的100/200%中间帧已有PASS，详细范围见[设计](design/compact-desktop-surfaces.md)与PROGRESS。

修复包Daymark-1.1.0-windows-x64-local-20261029-153905.zip（39,479,871字节），源码180345f。完整26项及SDK隔离/清单/ZIP/SHA256全部PASS；未签名本地试用，未发布远端二进制。

## 本机 WSL 同步服务（SYNC-WSL，2026-10-29）

Ubuntu22.04/WSL2用户级Go+SQLite服务已运行，Windows本机地址`https://127.0.0.1:9443`、开放注册；私钥/数据在WSL专用ext4目录。经用户确认信任当前用户CA，Windows默认TLS、双Qt客户端注册/登录/双向同步/离线/冲突及服务重启保留数据PASS；原故障注入全矩阵另行PASS。来源与界面/引擎边界见PROGRESS，[普通服务验证说明](../server/README.md)与忽略build/wsl-local-sync运行记录。未开启公网访问或Windows开机启动；本机CA于2026-11-28到期。

## 已知问题

- 平台专属门禁仍未验收：macOS 物理输入、读屏、通知、穿透、多屏/睡眠、干净机部署与签名。CI 通过不等于这些门禁通过。
- 同步专项尚需原生平台验收：macOS Keychain、账号表单/冲突的完整原生键盘/200%/读屏、真实异机备份与断电恢复；未对私人资料或生产账号做测试。

## 报告规则

- 「通过」只代表所列命令在该环境真实执行成功；不代表其他平台。
- 未执行的平台与门禁标 UNVERIFIED，不用版本号或合成截图代替。
- 逐轮证据（命令、日志名、SHA、耗时）保留在[进度日志](PROGRESS.md)与[验证记录](verification-report.md)。
