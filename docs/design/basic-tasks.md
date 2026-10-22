# BASIC-01：本地基础任务闭环

用户本轮要求实现基本功能。先交付可实际使用的收集、计划、执行和恢复，不把窗口POC未验收项当作已完成。优先级调整来自本轮用户指令，原里程碑需求继续保留。

## 交付范围

- Inbox、Today、Upcoming、Anytime、Someday、Logbook六个统一查询视图；标题和备注搜索。
- 默认新任务进入Inbox，可在详情改标题、备注、规划状态、开始日、截止日及持续今日关注。
- 完成、取消、重开、软删除及撤销最近一次删除；重启数据保留。
- 组件显示同源Today任务并完成，Mini显示今日数量；保留已有窗口控制。
- 初始完整schema采用TDD 001，SQLite WAL/FK、单实例、数据库线程和手动一致备份。

项目/领域编辑、标签、清单、排序、日历、重复、提醒、托盘和穿透不在本次基础切片，原需求不删除。

## 用户操作与失败处理

主窗口左侧六个视图，中央搜索/快速输入/任务列表，右侧选中项详情。880DIP宽仍保留主要操作，详情使用对话框减少挤压。Enter连续创建；成功才清空输入，保存期间禁用重复写入。Ctrl/Cmd+N聚焦输入，Ctrl/Cmd+F聚焦搜索。

详情本切片采用显式“保存”，不依赖400ms隐式提交；未保存离开详情、切视图、换任务或退出时提供保存/放弃/取消，保存失败留在编辑。日期输入ISO yyyy-MM-dd，留空为清除，非法日期和start>deadline就地报错且不写库。未来开始日期与“加入今天”冲突时由服务改为今天并提示；Someday日期暂停。

主窗口错误条显示安全错误和重试入口；磁盘/DB失败保留输入。删除为软删除，显示撤销入口。手动备份显示实际创建位置。启动数据库失败不重建数据库、不进入可写状态，可重新加载或退出。

## 层次与并发

- domain：Task、Result、FieldPatch、IClock。
- persistence：SqliteDatabase/MigrationManager/TaskRepository，SQL只在此层，首次空库事务迁移；未知/过新库拒绝打开写业务。现有已识别库不覆盖schema。
- services：TaskService验证和状态转换，QueryService统一筛选与稳定排序，注入IClock。
- models：TaskWorkspace协调异步命令，TaskListModel按ID增量刷新，主列表与Today组件共享已提交缓存。
- worker对象在专用QThread创建/使用/销毁数据库连接，GUI通过queued调用传值，不跨线程共享QSqlDatabase。一个pending写命令；成功回传已提交Task，GUI再更新两个模型；失败不改缓存。退出等待已接受命令完成。
- 单实例使用QLockFile及用户级QLocalServer；在开库前取得锁，第二实例只唤起主窗。自动测试和smoke均使用QTemporaryDir，支持开发时--data-dir指定隔离目录。

## 时间与数据

QDate表示本地日历日，UTC时间戳固定Z。QueryService使用本地D；午夜单次计时和低频日期变更兜底刷新，不做高频数据库扫描。SQL查询包含项目有效状态，即使项目编辑UI尚未交付也不放宽筛选语义。

标题trim后非空，最多512个Unicode标量值。TaskPatch区分不改/清空/赋值，row_version冲突返回Conflict。删除及恢复均带版本；完成/取消/重开独立处理终态时间。日历和提醒字段保留schema但不添加无用编辑控件。

备份使用VACUUM INTO创建不覆盖既有文件的WAL一致快照。首次空库无须备份；未知非空version0库拒绝迁移，避免误认用户库。只有首个schema在本轮交付，后续升级必须新migration、迁移前快照及回滚验证。

## 验证

TC-001～013（含项目状态筛选）、TC-020/026/030/038/040/043的相关自动化子集；临时库初始化/重入/过新拒绝、20项连续录入、Unicode边界、三态patch、并发冲突、删除恢复、重开、快照恢复一致。异步模型验证“失败不清输入/不改模型”的契约；QML启动使用临时库并等待worker ready，不能只验证窗口创建。
