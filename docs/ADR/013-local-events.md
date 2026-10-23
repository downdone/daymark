# ADR-013 本地事件与任务议程

2026-10-23，已接受：用户授权完成CAL-01/02。新增FR-019；原FR-011全天周列表继续有效，不新增小时轴或外部账户。

事件独立于任务。全天区间[start_on,end_on)，UI结束日期包含当天、边界加一天；定时区间[start_at_utc,end_at_utc)，保存IANA展示时区。用户输入当地时间，Qt TransitionResolution::Reject拒绝DST不存在/歧义时间，给出修改提示。日历按机器本地日交集显示跨日事件。通知不冒充时间块。

事件与任务多对多，关联角色preparation/followup。完成/改期互不级联；事件软删只隐藏事件及其关系，不删除任务；任务软删隐藏关联，撤销可恢复关系。事件删除提供8秒撤销，修改使用row_version。关系整体替换与事件保存处于同一事务，失败零修改。

schema003追加events/event_tasks，不改001/002。已交付schema1/2先WAL一致快照再顺序迁移；JSON格式版本保持1，schema升3，旧1/2缺少新表时视为空。JSON、快照涵盖事件及关系，CSV仍明确仅导出任务。

QML展示与命令、EventService验证/日期投影、EventRepository独占SQL；TaskWorkspace工作线程载入事件，主线程缓存用于展示。无新依赖。验收涵盖迁移备份失败、跨日/DST/时区、冲突/事务回滚、关系保留、导入损坏零修改、宽窄/200%及草稿恢复。
