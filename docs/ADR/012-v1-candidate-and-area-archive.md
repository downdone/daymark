# ADR-012：Windows候选交付及领域归档迁移

状态：实施。用户明确当前没有macOS，先交付Windows V1候选版。正式双平台V1仍需原门禁；本决策不批准省略平台验收或公开再分发审查。

PRD FR-006的领域归档此前遗漏。新增002_area_archive.sql，不改001；areas.archived默认0。归档领域隐藏其直接任务及子项目任务的活动视图/日历/提醒，保留全部任务、日期和版本；组织管理中可取消归档。Logbook保留历史。

已交付schema1升级前使用VACUUM INTO创建WAL一致快照，失败拒绝升级；002事务执行，失败回滚，成功后user_version=2。首次空库顺序执行001/002。备份文件名包含实际schema版本。

JSON formatVersion仍为1，schemaVersion输出2；接受schema1/2，旧areas行缺少archived时仅在schema1补默认0，其余字段仍严格验证。旧应用不识别schema2并安全拒绝打开，回退须使用升级前备份，不直接降版本号。

Windows开机启动采用HKCU Run/Daymark，仅用户显式开关写入当前程序路径；测试使用临时INI，真实OS登录触发未自动验证。macOS启动项适配与原生安装/签名留待相应平台交付，明确披露为未完成。
