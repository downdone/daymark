# 当前状态

本页是**唯一的状态摘要**，每次版本/门禁变化时更新。逐轮详细记录见[进度日志](PROGRESS.md)。

## 版本与数据

| 项 | 值 |
|---|---|
| 应用版本 | 1.8.0（`CMakeLists.txt` 与 `DAYMARK_VERSION`） |
| 数据库 schema | 10（追加账号/同步元数据，历史1～9未覆盖） |
| 平台目标 | Windows 10/11 x64、macOS 13+ |
| 技术栈 | C++20、Qt 6.10.2（LGPLv3 动态链接）、SQLite（QSQLITE） |
| 可选服务 | Go1.27.0单体 + SQLite3.54.1；HTTP/JSON v1，见[服务指南](../server/README.md) |

## 构建与测试

| 项 | 1.8.0本轮状态 |
|---|---|
| Windows MinGW Release | 完整构建及26/26 CTest PASS，275.50秒 |
| QML lint | daymark_qmllint、daymark_style_qmllint PASS |
| 源码发布守卫 | 14/14、版本/隐私、文档/样式/颜色/同步契约 PASS |
| Windows本地包 | 05c3ddb再次26/26 PASS；原生200%菜单定位失败已修复，原生437阶段/相关2项通过，最终包待复验 |
| Windows MSVC / macOS / Linux CI | 本次推送及tag结果待核验，不能沿用旧run |
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
