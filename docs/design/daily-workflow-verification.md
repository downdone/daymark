# 日常工作流验收记录

开发基线 5ef3c7b，分支 code_pet/daily-workflow。设计及编号见[总表](daily-workflow-delivery.md)。Windows 11、Qt 6.10.3、MinGW。所有自动资料使用临时目录。日志和原生截图存放于外层工作区 tmp/workflow-*.log、tmp/workflow-proof；不含真实用户资料。

| 编号 | 实现与验证入口 | 当前证据 |
|---|---|---|
| A1 | 现有离线 SQLite 创建、time_recognition／task_workspace | 已通过相关测试；空收集箱改为中性文字 |
| A2 | CalendarCreatePopup 日期冲突与手动覆盖优先 | calendar_interaction 新增冲突拦截及显式选择通过 |
| A3 | 已提交 ID 通知、主窗／小窗隐藏任务反馈和查看 | 新增 hiddenCreationReportsCommittedIdentity 通过；最终主窗原生列表、小窗工作流增量均通过 |
| B1 | 既有分类命令和 taskId、QueryService 投影 | unified_tasks、task_workspace、task_list_interaction 通过 |
| B2 | Main 空态优先级，搜索条件优先于历史空态 | 启动及列表回归通过；保留读取错误区 |
| B3 | 既有范围与 ListViewportState | calendar_interaction、task_workspace 通过 |
| C1 | 既有分类共享顺序与窄窗查找 | task_workspace、widget_workflow 通过 |
| C2 | 独立子任务输入、分钟刷新、组字保护 | widget_workflow 自动及 Windows 原生通过 |
| C3 | 既有形态控制、菜单交互锁、命中区 | widget_window、desktop_surfaces 通过；物理混合 DPI 仍未验证 |
| C4 | 共享 TaskDraftState／TaskConflictDialog，退出协调 | 主窗取消／选值／保存、小窗保留独立备注、退出继续使用和保存子任务后关闭的最终回归通过 |
| D1 | 既有 TaskCycleService、完成记录、提醒处置 | task_workspace、reminders、unified_tasks 通过；真实 Qt↔Go 周期资料联测通过 |
| D2 | 既有相对和绝对提醒 patch | time_input_ui、reminders 通过 |
| D3 | 既有轮次历史与旧轮拒绝 | task_workspace 通过 |
| E1 | 既有持久队列、资料隔离、重新登录 | Qt↔Go 真 TLS 双端联测通过，见下述后端边界 |
| E2 | 既有服务幂等和冲突；本轮编辑器字段合并 | 同一任务周期联测及主／小窗合并回归通过 |
| E3 | 既有版本能力门、备份与恢复契约 | 真 TLS 联测覆盖暂停／复查／重放；数据及更新测试通过；具体发行包有损迁移验收由发行时执行 |
| E4 | 既有更新控制器、安装与更新事务 | update_qt_controller、update_windows_flow、setup_windows_flow 通过；公网目标发布后另做发行验收 |

## 本轮新增行为

退出由主窗口协调。主、小窗快添和子任务草稿纳入确认；继续使用为默认，保存后逐条推进，放弃明确丢弃输入。写入失败中止退出并复位退出意图，已完成的前序保存保留。关闭主窗回托盘仍走原偏好。

冲突比较以打开时快照为基线。标题、备注、标签、关注和置顶独立比较；日期／周期／提醒及分类／父任务分别作为关联字段组。单边修改自动合并，两边不同值要求选择；取消保留草稿，应用不写库，保存仍经版本校验。合并期间再次更新会拒绝本次应用。周期草稿补保存 monthDay/yearMonth，取消和合并均保留月末／年周期参数。

## TLS 联测边界

Windows Schannel 在当前机器访问 Go httptest 自签名 TLS 服务时，能力请求在 connecting 阶段超时，服务端未收到 HTTP。代理绕过及 TLS 1.2 对照未解决，未更改产品超时、证书校验或生产协议。使用测试专用 DAYMARK_TEST_TLS_BACKEND=openssl 和本机 OpenSSL 3.5.7 后，TestQtDesktopEndToEnd 全链通过，57.71 秒。测试覆盖真实凭据、TLS、导入、增量推拉、持久队列、冲突、千条导入、版本门和资料隔离。该后端选项仅对本地合成联测生效，外部账号测试不接受覆盖。Schannel 问题保持可复现记录，不能用 OpenSSL PASS 声称它已修复。

## 尚需取得的证据

完整 Windows CTest 55/55 PASS（530.09 秒）。最后的 QML 增量重新构建和 lint 通过，Windows 原生列表回归零退出码、小窗保存退出增量回归 1/1 PASS（15.45 秒）。冲突弹窗居中与自然高度的最终原生截图复验零退出码，已目视检查截图，无裁切或过量短文本空白。物理混合 DPI 多屏、首次使用者无讲解观察、公网安装验收依赖相应设备／发行目标，单独记录，不计入本地开发通过。

## 正式发行证据（2026-11-28）

4.1.3 正式候选 CTest 55/55、QML lint 和生命周期检查通过；签名、独立部署、公开安装、匿名 GitHub 4.0.9→4.1.3 实际更新全部通过，保存退出重启后清理旧程序并保留合成资料。具体工作流链接和前序失败修复记录统一见 [PROGRESS](../PROGRESS.md)。物理多屏混合 DPI、首次使用者观察和 macOS 仍未验证。
