# BASIC-01：本地任务基础功能

本轮实现范围和行为见 [设计](../design/basic-tasks.md)，当前状态以 [PROGRESS](../PROGRESS.md) 为准；使用方式见 [用户说明](../user-guide.md)。

## 可观察闭环

输入任务→SQLite保存→视图显示→编辑日期/状态→完成进入历史→恢复→软删撤销→重启保留。Today组件消费同一QueryService及TaskService结果，不持有第二份业务状态。

## 自动验收证据

- task_service：20条录入、UTF-8/Unicode边界、三态patch、非法日期零修改、row_version冲突、取消/完成/重开、软删恢复、项目JOIN筛选、只读写入失败、WAL快照往返、过新和未知schema不覆盖。
- task_workspace：实际worker线程和两个增量模型，QAbstractItemModelTester校验、失败不改模型、完成同步、删除撤销、备份、退出排空已接受写入。
- single_instance：真实子进程激活既有进程，不成为第二writer，不创建任务库。
- qml_startup：临时库完整QML流程、失败保留输入、未保存离开保护；原生后端另跑并检查截图。
- sqlite_runtime/widget_window：此前运行时与窗口状态回归。

实际命令、失败及修复、验证边界见 [报告](../verification-report.md)。测试代码不使用用户业务数据目录。

## 余项

完整M1领域实体/迁移升级、D-03组织、D-04标签清单、D-05导出、Tonight/排序/主题、完整组件与托盘穿透、日历提醒仍未完成。本轮不能据此标M2/M3/M5整体通过。后续先完成W-02/W-03恢复通路，并按用户优先级推进组织与详情能力。
