# Daymark 技术架构与详细设计（TDD）

> 版本：1.0 · 与 `01_PRODUCT_SPEC.md` 同时适用。  
> 要求：Codex 应先阅读 PRD，再按本文件建立架构。  
> 技术基线：C++20 + Qt 6.8+ + Qt Quick/QML + Qt Sql/SQLite + Qt Test + CMake；不引入网络服务作为必需依赖。

## 1. 架构约束与 ADR

### ADR-001 选择 Qt Quick/QML + C++ 业务核心

- QML 负责视图、视觉状态、动画、用户输入、窗口排布，不直接拼接 SQL。
- C++ 负责 domain、application service、repository、date/recurrence、稳定排序、搜索、平台接口。
- QML 只持有可显示的 ViewModel/Model，对用户操作调用 Service/Command；不得通过多个 UI 副本修改各自状态。
- `QAbstractListModel` 给列表、日历和桌面组件提供可复用模型。界面路由依据同一个任务 ID 查询，不复制实体。

### ADR-002 Local-first 单体桌面应用

- SQLite 是任务、项目、标签、历史事件的唯一事实数据源。
- QSettings 只存小体量界面/本机设置（窗口位置、主题、UI 比例等），不是用户业务数据事实源。
- 首版不实现远程账号、云同步、WebSocket，也不增加本地 HTTP 服务来驱动 QML。
- 若未来需要同步，新增 SyncPort/ChangeLog 版本迁移，不要把 HTTP API 写进当前 TaskService。

### ADR-003 领域时间处理

- 日期：`QDate`，序列化为 `yyyy-MM-dd`，表示用户计划本地日历日期，跨时区移动不自动转换日期。
- 时间：`QDateTime` / `std::chrono`，持久化为 ISO8601 UTC（固定 `Z`）；提醒输入展示本地化。
- 提供 `IClock`/`SystemClock` 注入；不能在领域规则中到处直接调用 `QDate::currentDate()`，便于跨午夜、夏令时测试。
- 业务服务统一裁决 Today/Upcoming/Anytime，QML 不自行判断计划状态。

### ADR-004 单 writer 与事务

- 应用单实例，SQLite 启用 `PRAGMA foreign_keys=ON`, `PRAGMA journal_mode=WAL`, `PRAGMA busy_timeout=5000`。
- Repository 在约束清晰的 DB 线程中串行执行写入；严禁多线程共享同一个 `QSqlDatabase` connection。
- 领域操作以事务提交成功为准，UI 可以立即显示 loading，但失败时必须回滚视觉状态或维持旧状态。
- 不把任务内容写入结构化日志；允许非敏感 SQL 错误码和版本号。

### ADR-005 平台能力隔离

- `IWindowPlatform`：悬浮窗口置顶、鼠标穿透、屏幕位置/多显示器、隐藏程序坞/任务栏表现。
- `INotificationPlatform`：显示提醒、回退到应用内通知。
- `IAutoStartPlatform`：开机启动（P1）。
- `ISingleInstancePlatform`：单实例/二次启动唤醒（推荐 Qt QLockFile + QLocalServer）。
- 不能以 Win32/macOS API 侵入 Domain、Repository、CalendarService。

### ADR-006 设计体验限制

- 以 Things 3 的思路作为功能和体验参考，但所有名称、图标、插图、字体授权、源代码必须合法独立。
- “桌面小组件”首版仅为独立窗口，不能在文档/设置中标记为原生 WidgetKit/Windows Widgets。
- “透明”至少区分视觉 alpha / 整窗鼠标穿透；输入穿透时必须保留非窗口恢复通道。

---

## 2. 逻辑组件和依赖

```text
┌────────────────────────────────────────────────────────────────┐
│ Presentation: Qt Quick / QML                                  │
│ MainWindow   TodayView   TaskDetails   MonthView  WeekView    │
│ FloatingWidgetWindow   Search   Settings                      │
└──────────────────────────┬─────────────────────────────────────┘
                           │ Commands + QAbstractListModel
┌──────────────────────────▼─────────────────────────────────────┐
│ Application Services                                          │
│ TaskService / ProjectService / TagService / CalendarService    │
│ QueryService / RecurrenceService / ReminderService            │
│ BackupService / PreferenceService                             │
└──────────────────────────┬─────────────────────────────────────┘
                           │ domain structs + transactions
┌──────────────────────────▼─────────────────────────────────────┐
│ Persistence                                                     │
│ SqliteDb / MigrationManager / TaskRepository / etc.           │
│ SQLite (.db)    +    JSON export/import                         │
└────────────────────────────────────────────────────────────────┘
   └── ports → WindowsPlatform / MacPlatform / SafeFallback
```

架构耦合方向固定：UI → Application → Domain/Repository Interface → Infrastructure；Platform 实现依赖 Interface；测试可替换 IClock、Repository、Platform。

### 2.1 建议目录结构

```text
daymark/
├── AGENTS.md
├── README.md
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE_NOTICE.md
├── docs/
│   ├── 01_PRODUCT_SPEC.md
│   ├── 02_TECHNICAL_DESIGN.md
│   ├── 03_IMPLEMENTATION_PLAN.md
│   ├── ADR/
│   └── platform-test-matrix.md
├── cmake/
├── resources/
│   ├── icons/        # 项目自主绘制/使用合规许可的资源
│   └── translations/
├── src/
│   ├── main.cpp
│   ├── app/           # DI、启动、路由、运行状态
│   ├── domain/        # 纯业务数据结构、规则、IClock
│   ├── services/      # CRUD、查询、日历、提醒、重复、备份
│   ├── persistence/   # QtSql、repo、migrations
│   │   └── migrations/
│   ├── models/        # QAbstractListModel / QML viewmodel
│   ├── platform/
│   │   ├── common/
│   │   ├── win/
│   │   └── mac/
│   └── ui/
│       ├── App.qml
│       ├── MainWindow.qml
│       ├── components/
│       ├── views/
│       ├── dialogs/
│       └── widget/
├── tests/
│   ├── domain/
│   ├── persistence/
│   ├── services/
│   ├── integration/
│   └── fixtures/
├── scripts/
│   ├── build-windows.ps1
│   ├── build-macos.sh
│   └── generate-testdata.py
└── packaging/
    ├── windows/
    └── macos/
```

注意：若实现更简洁但保持层次清晰，可以合并类文件；不能为每个字段过度抽象接口，也不能把 UI 业务和数据库集中到一个 `main.cpp`。

---

## 3. 核心领域模型和 SQL 设计

### 3.1 数据设计规则

- 主键统一使用应用生成的 UUID 字符串（例如 `QUuid::createUuid().toString(WithoutBraces)`），不使用进程内增量 ID。
- DB 时间使用 ISO 8601 UTC 字符串；日期使用 `YYYY-MM-DD` 字符串；bool 使用 INTEGER 0/1。
- 有序实体用 `sort_order INTEGER`，同一分组重排时事务化批量重编号，避免浮点排序值精度衰减。
- 禁止物理级联删除用户任务；软删除以 `deleted_at_utc` 为准。真正永久清理保留到未来独立维护功能。
- 每个用户内容实体的 `row_version INTEGER` 从 1 开始，每次更改递增；为以后同步保留可比较版本。UUID + 时间戳不能代替并发冲突协议。
- 所有 `project_id`/`heading_id` 归属关系须由 Service 验证；跨表规则使用事务+集成测试，必要时增加触发器。
- 约束独立索引避免全表扫描；首版根据真实 1 万条数据的 EXPLAIN QUERY PLAN 验证。

### 3.2 初始 schema（M1 建议迁移 `001_init.sql`）

下面 SQL 旨在可以直接执行。`user_version` 仅由 MigrationManager 管理，迁移脚本内不手工递增；后续演进用 `002_*.sql`，不可重写已交付迁移。

```sql
CREATE TABLE IF NOT EXISTS areas (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL CHECK(length(trim(name)) > 0),
    color_key TEXT NOT NULL DEFAULT 'neutral',
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS projects (
    id TEXT PRIMARY KEY,
    area_id TEXT REFERENCES areas(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    description TEXT NOT NULL DEFAULT '',
    planning_state TEXT NOT NULL DEFAULT 'ACTIVE'
        CHECK(planning_state IN ('ACTIVE','SOMEDAY')),
    status TEXT NOT NULL DEFAULT 'OPEN'
        CHECK(status IN ('OPEN','COMPLETED','CANCELLED')),
    start_on TEXT,
    deadline_on TEXT,
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    completed_at_utc TEXT,
    deleted_at_utc TEXT,
    CHECK(start_on IS NULL OR deadline_on IS NULL OR start_on <= deadline_on)
);

CREATE TABLE IF NOT EXISTS headings (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS recurrence_templates (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    note_text TEXT NOT NULL DEFAULT '',
    task_defaults_json TEXT NOT NULL DEFAULT '{}',
    mode TEXT NOT NULL CHECK(mode IN ('FIXED','AFTER_COMPLETION')),
    frequency TEXT NOT NULL CHECK(frequency IN ('DAILY','WEEKLY','MONTHLY','YEARLY','AFTER_DAYS')),
    interval_n INTEGER NOT NULL DEFAULT 1 CHECK(interval_n >= 1),
    anchor_on TEXT NOT NULL,
    weekdays_mask INTEGER NOT NULL DEFAULT 0 CHECK(weekdays_mask BETWEEN 0 AND 127),
    month_day INTEGER CHECK(month_day BETWEEN 1 AND 31),
    year_month INTEGER CHECK(year_month BETWEEN 1 AND 12),
    end_on TEXT,
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1)),
    last_materialized_on TEXT,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT,
    CHECK(end_on IS NULL OR anchor_on <= end_on)
);

CREATE TABLE IF NOT EXISTS tasks (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    note_text TEXT NOT NULL DEFAULT '',
    project_id TEXT REFERENCES projects(id),
    direct_area_id TEXT REFERENCES areas(id),
    heading_id TEXT REFERENCES headings(id),
    planning_state TEXT NOT NULL DEFAULT 'INBOX'
        CHECK(planning_state IN ('INBOX','ACTIVE','SOMEDAY')),
    status TEXT NOT NULL DEFAULT 'OPEN'
        CHECK(status IN ('OPEN','COMPLETED','CANCELLED')),
    today_focus INTEGER NOT NULL DEFAULT 0 CHECK(today_focus IN (0,1)),
    evening INTEGER NOT NULL DEFAULT 0 CHECK(evening IN (0,1)),
    start_on TEXT,
    deadline_on TEXT,
    remind_at_utc TEXT,
    reminder_fired_at_utc TEXT,
    sort_order INTEGER NOT NULL DEFAULT 0,
    recurrence_template_id TEXT REFERENCES recurrence_templates(id),
    occurrence_on TEXT,
    instance_overridden INTEGER NOT NULL DEFAULT 0
        CHECK(instance_overridden IN (0,1)),
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    completed_at_utc TEXT,
    cancelled_at_utc TEXT,
    deleted_at_utc TEXT,
    CHECK(NOT (project_id IS NOT NULL AND direct_area_id IS NOT NULL)),
    CHECK(heading_id IS NULL OR project_id IS NOT NULL),
    CHECK(start_on IS NULL OR deadline_on IS NULL OR start_on <= deadline_on),
    CHECK((recurrence_template_id IS NULL AND occurrence_on IS NULL)
        OR (recurrence_template_id IS NOT NULL AND occurrence_on IS NOT NULL)),
    UNIQUE(recurrence_template_id, occurrence_on)
);

CREATE TABLE IF NOT EXISTS checklist_items (
    id TEXT PRIMARY KEY,
    task_id TEXT NOT NULL REFERENCES tasks(id),
    title TEXT NOT NULL CHECK(length(trim(title)) > 0),
    is_done INTEGER NOT NULL DEFAULT 0 CHECK(is_done IN (0,1)),
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS tags (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    normalized_name TEXT NOT NULL UNIQUE,
    color_key TEXT NOT NULL DEFAULT 'neutral',
    sort_order INTEGER NOT NULL DEFAULT 0,
    row_version INTEGER NOT NULL DEFAULT 1,
    created_at_utc TEXT NOT NULL,
    updated_at_utc TEXT NOT NULL,
    deleted_at_utc TEXT
);

CREATE TABLE IF NOT EXISTS task_tags (
    task_id TEXT NOT NULL REFERENCES tasks(id),
    tag_id TEXT NOT NULL REFERENCES tags(id),
    PRIMARY KEY(task_id, tag_id)
);

CREATE TABLE IF NOT EXISTS task_view_order (
    view_code TEXT NOT NULL,
    task_id TEXT NOT NULL REFERENCES tasks(id),
    sort_order INTEGER NOT NULL,
    PRIMARY KEY(view_code, task_id)
);

CREATE INDEX IF NOT EXISTS idx_tasks_planning ON tasks(planning_state, status, start_on);
CREATE INDEX IF NOT EXISTS idx_tasks_deadline ON tasks(deadline_on, status);
CREATE INDEX IF NOT EXISTS idx_tasks_project ON tasks(project_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_area ON tasks(direct_area_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_heading ON tasks(heading_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_tasks_updated ON tasks(updated_at_utc);
CREATE INDEX IF NOT EXISTS idx_tasks_reminder ON tasks(remind_at_utc, reminder_fired_at_utc);
CREATE INDEX IF NOT EXISTS idx_tasktags_tag ON task_tags(tag_id);
CREATE INDEX IF NOT EXISTS idx_checklist_task ON checklist_items(task_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_project_area ON projects(area_id, sort_order);
CREATE INDEX IF NOT EXISTS idx_heading_project ON headings(project_id, sort_order);
```

说明：

1. `INBOX` 任务若被明确排期，Service 必须自动转 `ACTIVE`；schema 允许历史修复但业务命令需约束。
2. `deleted_at_utc` 与 `UNIQUE(normalized_name)` 的关系：**被删除的标签名不会立即允许同名复用**，首版编辑界面可恢复标签或明确报冲突；后续若需要软删除后复用，可迁移到 partial unique index。
3. SQLite 文本字段未做日期格式的 DB 级全面验证，Service 必须解析 ISO 日期、拒绝非法日期；导入前需逐条校验。
4. 递归规则 `task_defaults_json` 的 schema 需固定为带 `format_version` 的对象，至少包含归属、标题、备注、标签和相对日期策略，不允许把任意未知 JSON 字段直接拼到 SQL。
5. `task_view_order` 首版只实际使用 `view_code='today'`，其他视图按既定规则查询；在项目内使用 `tasks.sort_order`。
6. 新建/移动 Project/Heading 时必须保证任务的 Project 与 Heading 匹配；跨表约束在服务中验证。
7. 项目被完成或取消后，TaskQueryService 通过 JOIN 隐藏其任务，不改变任务内容；恢复项目即恢复任务可见性。

### 3.3 业务对象（建议 C++）

```cpp
struct Task {
    QString id;
    QString title;
    QString noteText;
    std::optional<QString> projectId;
    std::optional<QString> directAreaId;
    std::optional<QString> headingId;
    PlanningState planningState;
    TaskStatus status;
    bool todayFocus = false;
    bool evening = false;
    std::optional<QDate> startOn;
    std::optional<QDate> deadlineOn;
    std::optional<QDateTime> remindAtUtc;
    std::optional<QDateTime> completedAtUtc;
    qint64 sortOrder = 0;
    quint64 rowVersion = 1;
};

struct TaskPatch {
    // 三态 patch：未指定 / 清空 / 赋值，区别于 std::optional<T>
    // Codex 自行实现 FieldPatch<T>，需附单元测试。
};

struct QueryContext {
    QDate localToday;
    bool includeCompleted = false;
    std::optional<QString> areaId;
    std::optional<QString> projectId;
    std::optional<QString> tagId;
};
```

### 3.4 服务接口契约（建议）

这些是行为契约，类名/参数可根据 Qt 信号槽和异步线程实施优化，但**不允许语义缩水**。

```cpp
// 所有命令必须返回 Result<T>（成功值 / 验证错误 / 持久化错误），
// 异步调用时使用 requestId 信号响应；UI 层不得私自吞异常。

class TaskService {
public:
    Result<Task> createTask(const CreateTaskCommand&);
    Result<Task> updateTask(const QString& id, const TaskPatch&);
    Result<void> completeTask(const QString& id);
    Result<void> cancelTask(const QString& id);
    Result<void> reopenTask(const QString& id);
    Result<void> softDeleteTask(const QString& id);
    Result<void> moveTaskToProject(const QString& id, const QString& projectId,
                                   std::optional<QString> headingId);
    Result<void> moveTaskToArea(const QString& id, const QString& areaId);
    Result<void> reorderTasks(const QList<QString>& ids, const QString& context);
};

class QueryService {
public:
    QList<Task> inbox(const QueryContext&);
    QList<Task> today(const QueryContext&);
    QList<Task> upcoming(const QueryContext&, QDate from, QDate to);
    QList<Task> anytime(const QueryContext&);
    QList<Task> someday(const QueryContext&);
    QList<Task> logbook(const QueryContext&, int limit, int offset);
    QList<Task> search(const QString& query, bool includeCompleted);
};

struct CalendarEntry {
    QString taskId;
    QDate displayOn;
    enum class Kind { Start, Deadline, StartAndDeadline } kind;
    QString title;
    bool isOverdue;
};

class CalendarService {
public:
    QList<CalendarEntry> entries(QDate from, QDate to, bool includeCompleted);
    Result<void> moveCalendarEntry(const QString& taskId,
                                   CalendarEntry::Kind kind, QDate target);
};

class WidgetService {
public:
    void show();
    void hide();
    void setMode(WidgetMode mode);
    void setClickThrough(bool enabled);
    void setAlwaysOnTop(bool enabled);
    void setOpacity(double opacity); // [0.6, 1.0]
    void normalizeGeometryAgainstAvailableScreens();
};
```

`Result<T>` 应具有标准错误码：`ValidationError`, `NotFound`, `Conflict`, `PersistenceError`, `PermissionError`, `UnsupportedPlatform`, `Cancelled`；错误包含供 UI 翻译的 code+safe parameters，而不是数据库原始敏感内容。

### 3.5 QML 数据交互与刷新

- 将 `TaskListModel` 设计为 `QAbstractListModel`，roles 最少为：`taskId`, `title`, `isDone`, `projectName`, `startOn`, `deadlineOn`, `isOverdue`, `todayFocus`, `evening`, `tagNames`, `sortOrder`。
- `MonthCalendarModel` 使用 CalendarEntry 模型，`WeekCalendarModel` 复用同一查询 Service。
- `WidgetTodayModel` 使用 Today 视图的 QueryService，同一 TaskService 完成任务。
- Repository 写成功后发布 `taskChanged(taskId, affectedFields)` 事件；ViewModel 仅重查受影响的日期/视图或应用精确增量，不采用 100ms 全库轮询。
- 关闭详情/切换视图时等待重要编辑完成或显式保留未提交文本。
- QML 持久化仅通过 Service，禁止直接 QSqlQuery、原生数据库句柄进入 QML。

---

## 4. 领域状态机和流程

### 4.1 新建

```text
输入标题
  ├─ 默认 → INBOX/OPEN
  └─ 明确项目/领域/日期 → ACTIVE/OPEN
      └─ 明确选择 Someday → SOMEDAY/OPEN
→ 验证 → DB transaction → modelChanged → 列表/日历/悬浮同步
```

### 4.2 完成/取消/重开

```text
OPEN ── complete ──> COMPLETED ── reopen ──> OPEN
  └──── cancel ────> CANCELLED ── reopen ──> OPEN
```

- 完成时设置 `completed_at_utc`、清除 `cancelled_at_utc`；取消反向处理；重开两者清空。
- 完成时提醒未触发也不得再推送；如果重开且 remind_at 仍在未来，可以重置 reminder fired；若过去提醒不自动重播。
- 重复实例完成后执行 RecurrenceService 的后续生成，须在一致的事务边界中进行或使用幂等重试。

### 4.3 列表筛选 SQL/Service

筛选逻辑参考 PRD §1.3，特别是 JOIN 项目状态：

```text
effectiveActive(task) =
  task.status == OPEN
  && task.deletedAt == null
  && task.planningState == ACTIVE
  && (task.projectId == null ||
      (project.deletedAt == null
       && project.status == OPEN
       && project.planningState == ACTIVE))
```

Someday 视图包括 task 本身的 SOMEDAY 或被 Someday 项目有效纳入的开放任务。项目已完成或取消的任务不显示活动视图；Project 删除则事务迁出任务。

---

## 5. 日期/日历技术设计

### 5.1 MonthGrid/WeekGrid

- 可使用 Qt Quick Controls MonthGrid 作为日期网格基础，任务内容用自定义 delegate 呈现；WeekGrid 独立自绘 7 列。
- **注意** Qt Quick MonthGrid 中 `month` 0-based，而 `QDate::month()` 1-based，CalendarService 应输出日期字符串或明确转换。
- 日历查询统一返回 `CalendarEntry`，其中 `Kind::Start` 和 `Kind::Deadline` 仅标记含义，拖动目标决定修改哪个字段。
- 同一任务 start/deadline 同一天时合并成 `StartAndDeadline`，**拖拽该合并条目时要求二次选择“修改计划开始/修改截止”，不可默认同时修改两个字段**。
- 日历小单元格 UI 不绑定整个 Task 对象数组，可由 C++ 先聚合当天 `displayOn -> entries`，限制一屏渲染。

### 5.2 计划时间块（仅预留）

后续 `calendar_blocks` 表包含 `id, task_id, start_at_utc, end_at_utc, local_timezone, notes`。V1 不建该表、不要添加冗余不可操作字段到 Task；仅在架构中说明预留，周视图首版是按日展示。

### 5.3 拖拽的一致性

- QML Drag/DropArea 获取 `{taskId, kind, date}`；只调 CalendarService.moveCalendarEntry。
- 应用层验证 `deadline >= start`、状态 OPEN、有效归属和权限，成功后更改 DB。
- 失败应动画返回原日，不产生一份“孤立日历项”，提示具体原因。
- 拖拽跨天后立即更新 Today/Upcoming/Anytime 结果，尤其拖到今天。

---

## 6. 悬浮窗口与系统平台适配

### 6.1 Qt 窗口原型

- QML `Window { color: "transparent"; flags: Qt.FramelessWindowHint | Qt.Tool | Qt.WindowStaysOnTopHint }`；Qt 6 版本不同以目标构建通过为准。
- 背景由半透明 Rectangle 绘制，内容（任务标题）尽量不随低 alpha 而不可读；**整窗透明度和背景透明度是不同实现**。
- 采用独立 `FloatingWidgetWindow.qml`，可创建/隐藏而不退出进程；可拖动区域使用 DragHandler 或平台允许的窗口移动机制。
- 置顶/鼠标穿透通过 `IWindowPlatform` 实现，Qt flags 可行时先用 Qt，验证不足时按平台扩展。
- QML 可调用 `WidgetViewModel`，禁止在该 Window 内持有任务数据副本。

### 6.2 Windows 实现要求

- 透明窗与置顶以 Qt 能力为基线；必要时 Win32 调整 extended styles / `SetWindowPos`。
- 窗口工具条不应占用 Alt+Tab/任务栏，但不同配置以实测为准；改变 flags 可能导致窗口句柄重建，需恢复坐标/可见性。
- 全窗穿透可先使用 `Qt::WindowTransparentForInput`，若平台效果不符合预期，使用 Win32 样式按文档实现并测试；禁用穿透时恢复输入样式。
- Multi-monitor 通过 QGuiApplication::screens()，定位按 availableGeometry 钳制。
- Windows 锁屏/UAC 安全桌面不承诺置顶覆盖。

### 6.3 macOS 实现要求

- 首选 Qt Window flags；对 Spaces/全屏关联行为，在 `.mm` Obj-C++ 适配层使用公开 AppKit `NSWindow` 能力，确保 App Store 外普通桌面应用可行。
- 置顶组件不应遮蔽系统关键 UI；`NSWindow.CollectionBehavior` 需分别在正常桌面/全屏应用/多 Spaces 实测。
- `Qt.Tool` / AppKit panel 激活策略需检查点击及焦点，不得导致任务详情的输入失败。
- 鼠标穿透退出路径不依赖悬浮窗本身。
- 原生 WidgetKit 不在 V1，不能为了模拟桌面嵌入引入私有 API。

### 6.4 Window geometry 规范

QSettings 记录：`widgetMode`, `clickThrough`, `alwaysOnTop`, `locked`, `opacity`, `windowRectDip`, `screenName`, `screenDpr`。恢复时：

1. 查找匹配 screenName，找不到则选择当前主屏幕。
2. 将窗口大小钳制到可用区域，至少留 48 DIP 可见标题拖动区。
3. DPI 变化时按 DIP，而不是简单重用物理像素坐标。
4. 检查窗口是否完全屏外并自动恢复；多屏切换后用户可“重置悬浮窗位置”。

### 6.5 托盘与单实例

- 推荐 `QApplication` + `QQmlApplicationEngine` + `QSystemTrayIcon`；`setQuitOnLastWindowClosed(false)`，托盘行为在 Qt Widget 模块完成。
- 单实例使用 QLockFile 阻止双 writer、QLocalServer/QLocalSocket 发送 `activate` 消息使原进程显示主窗口；应处理崩溃残留锁（QLockFile stale lock）和服务端未初始化竞态。
- 用户点击“退出 Daymark”时保存设置、等待数据库任务队列清空后释放句柄退出。

---

## 7. 提醒/重复/备份设计

### 7.1 提醒

- ReminderService 依据 UTC 时间安排下一次到期；通过单次计时与唤醒/午夜重算，不允许每秒全表扫描。
- `reminder_fired_at_utc` 在决定发送/记录时提交；若系统通知接口失败，应写入应用内通知列表，前端显示未投递状态。
- 不假设 QSystemTrayIcon::showMessage 一定被系统显示（OS/用户设置可屏蔽）。
- 单实例是全端一次性提醒的前提；多设备同步后要重新设计跨设备去重（V2）。

### 7.2 重复

- 固定计划 recurrence 使用 `anchor_on` + 频率/间隔 + weekdays_mask/month_day/year_month。
- `month_day` 和 `year_month` 仅在相应频率下填写，服务严格校验规则组合。
- 新建规则时，`task_defaults_json` 记录新实例的标题、归属、标签、notes、默认提醒的**本地时间**策略，不直接复制已过期绝对时间。
- 固定模式实例 `occurrence_on` = 周期日期、`start_on` = occurrence_on；deadline 相对偏移仅作为 V2 可配置，V1 默认无 deadline 或使用模板定义的有效同日 deadline 策略。
- 反复执行 materialize 采用 `INSERT OR IGNORE` + UNIQUE，不能误覆盖已完成实例。
- 本地日期频率使用 QDate::addDays/addMonths/addYears 时注意 Qt 对非法月日可能调到月末，**MONTHLY 31 / YEARLY 2月29 必须用“构造目标日期并检查是否有效”的方案，不得简单 addMonths 自动折算**。
- 模板更新在事务内处理将来未完成且未手动编辑实例；模板删除仅停止新生成。
- AFTER_COMPLETION 模式不和 FIXED 模式同时配置；一次完成产生一个新的 occurrence，按本地 calendar day 计算下一日，禁用多次提交生成重复任务。

### 7.3 备份/导出

- SQLite WAL 数据库备份优先通过 `VACUUM INTO` 或 SQLite Online Backup API 创建一致快照，需在实际 Qt QSQLITE 环境中验证该语句可用与路径正确；不得直接复制正在使用中的 `.db` 主文件。
- 日常自动备份按本地日期每天最多一次并轮换最近 7 份，避免每次启动对大库重复备份；MigrationManager 在正式升级 schema 前先生成备份；若升级失败回滚/停止启动并给恢复说明。
- 导入 JSON 支持 `format_version` / `exported_at_utc` / entities；先 parse + validation + reference resolution，再事务插入。
- 导出 JSON 建议固定：`schema_version`, `export_version`, `entities`, `metadata`。仅导出业务必要字段，系统绝对路径不包含在导出中。
- 不允许备份失败后静默继续执行危险数据迁移。

---

## 8. 工程框架和构建

### 8.1 构建约束

- CMake >= 3.24（苹果平台满足最低配置，实装需确认本机工具链）；C++20；`CMAKE_AUTOMOC` 等由 `qt_standard_project_setup(REQUIRES 6.8)` 处理。
- `find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Widgets Test)`（Test 可只在测试构建时要求）。
- 使用 `qt_add_qml_module` 注册 QML 模块，不使用 Qt 5 旧式 qmake。
- 一次构建只选择 native 平台实现，非对应平台文件不参与编译。
- Windows 推荐 MSVC 2022 64 位；macOS Xcode/Apple Clang；用 CMakePresets 描述 Debug/Release，避免写死本机 Qt 安装路径。
- 禁止把 `CMAKE_PREFIX_PATH` 写成开发者个人绝对路径提交进仓库，通过环境/预设指向 Qt。
- 脚本负责部署 Qt runtime；Windows `windeployqt`；macOS `macdeployqt`，实际发布需验证签名/notarization（没有证书时能产出未签名测试包，但不能声称通过系统公证）。

### 8.2 第三方依赖策略

- 默认仅使用 Qt 官方模块、SQLite Qt driver、C++ 标准库，必要时系统 API。
- 未论证的网络框架、ORM、主题库、图标 CDN、开源 Things 克隆代码不允许自动引入。
- 若确有必要引入第三方库，先写 ADR：功能缺口、许可、二进制体积、维护和替代方案，再新增锁定版本。
- 构建与运行时不需要互联网；任何 icon/font 请作为许可明确的本地资源。

### 8.3 可观察性与错误处理

- 日志级别 DEBUG/INFO/WARN/ERROR，默认 INFO；脱敏，不记任务内容、备份内容、用户设备唯一码。
- 用户可导出诊断日志，明确展示文件位置。
- Repository 错误分类：约束失败/磁盘满/只读/锁冲突/数据库损坏；UI 给可恢复建议。
- 崩溃恢复：优先完整数据库；启动前自动检测 `PRAGMA quick_check` 的频率可配置，至少提供手动“检查数据完整性”功能。

---

## 9. 测试策略与质量门禁

### 9.1 单元测试（Qt Test）

- Domain：planning_state / status 转移、Today/Anytime/Upcoming 的边界、start/deadline 校验、午夜跨天、Someday 覆盖、Project 状态遮蔽。
- Calendar：月末、跨年、不同一周起点、同日 start+deadline 双标记、拖拽仅改变目标字段。
- Recurrence：每日、周指定日、每月 31、闰日、DST 当地日期、重复生成幂等、AFTER_COMPLETION 只生成一项。
- TaskPatch：未设置/清空/赋值三态测试；不能把空值与“不修改”混淆。
- 任务排序：移动/插入/删除/重开后稳定，不产生重复 position。

### 9.2 Repository 集成测试

- 测试 DB 必须临时隔离、自动清理，永远不能指向用户实际 AppDataLocation。
- 新建项目/领域、Heading 归属合法性、跨项目任务移动、删除 Project 后任务保留。
- SQLite FK 启用、索引与事务、版本迁移、失败原子回滚。
- JSON 导入碰撞、损坏、缺字段、非法日期、外键错误；验证失败时原 DB 零修改。
- WAL 热备份恢复一致、迁移备份失败时停止。

### 9.3 UI 集成/跨平台手工测试

| 场景 | Windows | macOS |
|---|---|---|
| 透明窗口可见且文字清晰 | 必测 | 必测 |
| 置顶/取消置顶 | 必测 | 必测 |
| 鼠标穿透/托盘恢复 | 必测 | 必测 |
| 小组件拖动/调整尺寸/锁定 | 必测 | 必测 |
| 单屏/双屏/拔插屏/DPI 改变 | 必测 | 必测 |
| 最小化/关闭到托盘/彻底退出 | 必测 | 必测 |
| 高亮焦点/快捷键/输入法 | 必测 | 必测 |
| 深浅主题/系统主题切换 | 必测 | 必测 |
| 睡眠唤醒/跨本地午夜 | 必测 | 必测 |
| 全屏应用/Spaces 限制 | 记录 OS 行为 | 必测并记录限制 |
| 包安装到干净系统后运行 | 必测 | 必测 |

禁止宣称未跑过的平台测试“已通过”。若开发当前仅有 Windows 环境，macOS 标记 `UNVERIFIED`，不得标记完成跨平台交付。

### 9.4 性能压力基准

数据生成脚本输出稳定、可重复测试：

- 10,000 任务、200 项目、20 Areas、50 Tags、平均每任务 3 个 Checklist、30% 完成态。
- 冷启动时间（从进程开始到主界面可交互）、任务 CRUD 交互反馈、Today 查询、30天 CalendarEntry 查询、搜索耗时。
- 主窗口长时间打开+桌面组件打开：CPU/内存/GUI 帧稳定性。
- 记录 CPU、OS、Qt 版本、Release/Debug、样本规模；不要只提供单次主观体验。

---

## 10. 工程疑难问题的默认处理顺序

1. 数据完整性优于动画视觉；任何可能丢数据的设计必须先修复。
2. 原生平台差异先用公开 Qt API；验证不足再做薄的 Win32/AppKit 适配层。
3. 紧贴 V1 边界，优先完成“从收集到执行”的业务闭环。
4. 冲突的需求请提交 ADR：当前行为、受影响场景、替代方案、可复现测试、建议结论；不得擅自删功能。
5. 不要凭假设宣称某操作系统支持原生小组件、全局快捷键或后台提醒。

## 11. 官方文档入口

- QML Window: <https://doc.qt.io/qt-6.8/qml-qtquick-window.html>
- Qt Namespace / Window flags: <https://doc.qt.io/qt-6.8/qt.html>
- QML MonthGrid: <https://doc.qt.io/qt-6.8/qml-qtquick-controls-monthgrid.html>
- QML module CMake: <https://doc.qt.io/qt-6/qt6-port-to-qt-add-qml-module.html>
- Qt Windows deployment: <https://doc.qt.io/qt-6.8/windows-deployment.html>
- Qt macOS deployment: <https://doc.qt.io/qt-6/macos-deployment.html>
- QSystemTrayIcon: <https://doc.qt.io/qt-6/qsystemtrayicon.html>

## 追加设计：本地事件（ADR-013，2026-10-23）

用户授权FR-019。本地EventService/EventRepository与TaskService/Repository并列，不将任务日期改造成时间块。schema003、全天排他结束日、定时UTC与IANA时区、DST拒绝策略、关系事务/软删/撤销、JSON旧版本兼容以[ADR-013](ADR/013-local-events.md)为准。QML只展示服务投影并提交命令，事件不参与Today/Upcoming业务筛选。周视图继续无小时轴。