# 当前架构与实施设计

日期：2026-10-22。需求基线为 PRD 1.0；Qt 版本由 ADR-007 固定为 6.10.2。

2026-10-28追加的可选账号同步方向见[ADR-026](../ADR/026-go-sqlite-sync-service.md)；其Go+SQLite服务端、客户端同步模块及可靠性目标见[工程架构](server-architecture.md)，协议以[v1草案](../api/sync-v1.md)为准。以下原桌面架构保留，不将新增设计视为当前实现，状态仍只见PROGRESS。

## 1. 已实现与计划边界

已实现：CMake、QApplication、AppRuntime、Daymark QML模块、主窗口及独立组件；W-01的窗口控制，以及BASIC-01的初始schema、单实例、worker线程、任务CRUD/软删恢复、六个统一视图、搜索、同源今日组件和手动一致快照。窗口设置持久化、托盘/穿透和完整项目组织尚未实现。

当前基础切片详见 [BASIC-01](basic-tasks.md)，明确的实施选择见 [ADR-009](../ADR/009-basic-task-slice.md)。以下目标架构只有上述已落地部分属于完成范围。

本文件其余内容是后续实施契约，不能当作已实现声明；实现状态见 PROGRESS。

## 2. 模块与依赖方向

```text
main → app/AppRuntime（对象组装、生命周期）
             ├─ ui/QML ← models/ViewModel ← services
             ├─ services → domain + persistence/Repository
             └─ platform（窗口、通知、单实例、自启）
```

| 目录 | 职责与约束 | 首次落地 |
|---|---|---|
| src/app | 启动、服务对象所有权、退出协调；不承载查询业务 | 当前 |
| src/ui | QML 布局/交互/视觉；只消费模型和调用命令 | 当前 |
| src/domain | 领域值、IClock、验证、FieldPatch、Result；不依赖 QML/SQL | M1/M2 |
| src/services | 状态命令、查询语义、日历、重复、提醒、备份 | M2 起 |
| src/persistence | 数据库连接、迁移、Repository、SQL 和事务执行 | M1 起 |
| src/models | QAbstractListModel、请求状态与可翻译错误适配 | M2/M3 |
| src/platform/common | Qt 公共窗口能力与恢复计算 | M0 |
| src/platform/win、mac | 确有需要的原生薄适配，仅对应平台编译 | M0/M6 |
| tests | domain/persistence/services/integration 分层验证；临时数据 | 当前 integration |

按切片创建目录和类，不靠空目录或空接口假装架构完成。当前启动入口日后可替换具体服务，不引入服务定位器或通用 DI 框架。

## 3. 启动和退出

当前：QApplication → 参数/数据目录 → QLockFile/QLocalServer → QML双窗口 → worker中开库/迁移/加载 → 服务就绪后启用任务命令。启动DB失败显示安全错误和重试入口，不覆盖原库；smoke用QTemporaryDir且等待DB/完整合成任务流程。关闭主窗口直接退出，退出屏障等待worker已接受写入完成；尚未启用托盘驻留。

M1 目标顺序：解析数据目录 → 单实例锁及本地唤醒 → 串行 DB worker → 打开 SQLite/检查版本 → 必要时一致性备份 → 迁移 → 创建服务/模型 → 加载 UI → 托盘 → 启动提醒和跨天刷新。第二进程只发送激活请求，不打开写连接。唤醒服务尚未就绪时有限重试，不能忽略启动竞态。

启动迁移失败：不加载可写业务 UI；展示错误码及恢复步骤，不尝试自动删除/重建用户库。当前预览没有业务 DB，所以尚无这一分支实现。

目标退出顺序：停止接收新命令 → 处理/保留详情未提交文本 → 等待已接收写队列完成 → 保存 UI 设置 → 关闭 DB worker 内连接 → 释放锁。退出失败显示具体阻塞原因，不静默丢弃编辑。

## 4. 线程与命令提交

UI、QML、QAbstractListModel 只在 GUI 线程；DB worker 拥有唯一写连接，连接必须在所属线程创建、使用、销毁。初期所有 Repository 查询也在该 worker 执行，避免连接跨线程；后续读连接优化需实测支持。

用户命令携带 requestId、taskId、期望 rowVersion 和字段 patch。GUI 适配层进入 pending，worker 验证并在事务中执行，成功返回已提交结果和 ChangeSet。GUI 线程接收结果后更新模型并发完成信号。失败保留原模型与输入草稿，允许重试；禁止只在界面“先完成”而忽略落库失败。

跨领域操作（删除项目并迁出任务、完成重复实例并生成下一项）由一个应用命令定义事务边界，Repository 不在内部擅自提交半个操作。普通写失败不丢旧值；命令重入通过 pending 限制和 rowVersion 冲突识别。

## 5. 数据与迁移

业务表采用 TDD §3.2 的 areas/projects/headings/recurrence_templates/tasks/checklist_items/tags/task_tags/task_view_order；M1 交付首个 migration 后即视为历史，不再覆盖。schema_version 由 MigrationManager 管理 PRAGMA user_version。

应用库启用 FK、WAL、busy_timeout。迁移计划：读取现有版本并拒绝新于程序的 schema → 对非空既有用户库创建一致快照 → 在事务中按顺序执行迁移 → 验证 FK/版本 → commit。备份失败停止迁移；失败回滚，不修改历史脚本。VACUUM INTO 在事务外的实际可用性由测试确认。

数据路径来自 QStandardPaths；测试通过构造参数注入 QTemporaryDir；当前 smoke 不打开任何业务库。设置存在 QSettings，业务数据不得写入源码、程序目录或 QSettings。

## 6. 视图与刷新

QueryService 以 QueryContext.localToday 和项目有效状态统一解释 PRD §1.3；主列表、日历、组件共享服务和任务 ID。今天与 Anytime 可重叠；日历项只是引用，不是任务副本。

ChangeSet 包含实体 ID、字段、原/新日期及归属。先刷新受影响视图快照，再按 ID 使用插入/删除/移动/更新信号发布；默认不高频全表轮询。10k 基准之前不承诺性能。午夜由下一本地午夜计时触发；唤醒/系统时间或时区变化时由平台事件与可控的兜底检查刷新，IClock 可替换。

## 7. 悬浮窗恢复

### W-01 当前接口

AppRuntime持有QtWindowPlatform、WidgetViewModel、QQmlApplicationEngine，按此顺序声明以保证窗口先于模型销毁。QML模块注册不可由QML创建的WidgetViewModel类型，启动时通过required property注入；QML加载后由C++找到独立组件窗并绑定模型，再显示。

IWindowPlatform的prepare/setAlwaysOnTop/resize/setVisible/startMove返回WindowResult（None、WindowUnavailable、OperationRejected、MoveUnsupported）。这是窗口端口错误值，尚未引入M1领域Result。模型将错误转为可翻译提示，并且只在尺寸切换成功后提交mini状态。visible/alwaysOnTop读取QWindow并监听信号，处理外部关闭与销毁。

QtWindowPlatform.prepare解除QML嵌套Window的transient parent，避免取消置顶后仍因owner关系压住主窗；设置Tool和Frameless。置顶变化保存/恢复几何及原可见性，隐藏窗口不会被误显示。窗口操作不调用requestActivate；是否真正不抢焦点需平台实测。拖动使用 [Qt 6.10 QWindow.startSystemMove](https://doc.qt.io/qt-6.10/qwindow.html#startSystemMove)；offscreen/minimal明确返回不支持，禁止作为原生拖动PASS。

Qt成功接受flag不等于OS窗口层级已验证；两者在验证记录中分开。以下恢复/穿透/持久化仍为后续设计。

WidgetViewModel 管理 show/hide、mode、alwaysOnTop、clickThrough、locked；IWindowPlatform 负责实际窗口操作与能力报告。正常模式/迷你模式与穿透开关分别存状态，避免关闭穿透时丢掉原布局。

仅在托盘恢复动作已注册且主窗口设置入口存在时允许启用穿透；托盘不可用时禁用穿透并给原因。修改 flags 可能重建窗口句柄，适配层恢复可见性、几何和焦点策略；失败返回错误，UI 不显示已生效。

位置记录 screenName、DIP 矩形、DPR、归一化位置与像素诊断值；恢复优先使用 DIP 和当前 availableGeometry，确保至少 48 DIP 表头可达；屏幕丢失回主屏，提供重置。背景 alpha 只作用于背景 Rectangle，文字独立保持可读。

## 8. 当前待决问题

- macOS Qt.Tool/Spaces/全屏与不抢焦点的实际行为，需 M0 原生测试后决定是否增补 AppKit。
- 正式发行使用的 Qt 许可来源、NOTICE、签名和打包条件，M7 前核实；当前安装的许可客户端有诊断输出。
- 标题计数已按ADR-009落实为Unicode标量值并验证补充平面字符；若后续要求按字素簇计数需新ADR。
- JSON 字段名基线有 format_version/export_version 两种表述，M2-07 以带版本的格式契约和 round-trip 样本明确，M7 导入前冻结。
