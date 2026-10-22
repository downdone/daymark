# 平台验证矩阵

2026-10-22，v0.1本地交付。PASS只针对行内所述范围；macOS未执行任何本轮构建。

| 场景 | Windows11 x64 / Qt6.10.2 MinGW | macOS Intel | macOS Apple Silicon |
|---|---|---|---|
| Debug/Release配置与编译 | PASS | UNVERIFIED | UNVERIFIED |
| 真实临时SQLite/WAL/回滚/重开/外键 | PASS | UNVERIFIED | UNVERIFIED |
| CRUD/六视图/搜索/日期与版本冲突 | PASS | UNVERIFIED | UNVERIFIED |
| 项目/领域/Heading/标签/清单、安全删除 | PASS（含事务失败注入） | UNVERIFIED | UNVERIFIED |
| Today排序/今晚与重启恢复 | PASS（服务与异步模型测试） | UNVERIFIED | UNVERIFIED |
| CalendarService边界/合并标记/改期回滚 | PASS | UNVERIFIED | UNVERIFIED |
| 重复规则/幂等/手改保护/完成后N天 | PASS | UNVERIFIED | UNVERIFIED |
| 提醒认领一次/持久记录/完成抑制 | PASS；系统实际送达UNVERIFIED | UNVERIFIED | UNVERIFIED |
| JSON预览/严格校验/追加/替换/快照恢复/每日备份 | PASS；真实SQLite临时文件 | UNVERIFIED | UNVERIFIED |
| worker模型、退出排空、单实例子进程协议 | PASS；不证明人工焦点行为 | UNVERIFIED | UNVERIFIED |
| 各功能页、草稿保护、浅深/200%详情QML启动 | PASS（offscreen和Windows后端合成流程） | UNVERIFIED | UNVERIFIED |
| 原生QWindow模式/位置/可见性/置顶/穿透flag | PASS（Qt状态和临时设置） | UNVERIFIED | UNVERIFIED |
| 真实拖动/缩放/层级/焦点/穿透外部恢复 | UNVERIFIED；代码入口已实现 | UNVERIFIED | UNVERIFIED |
| 托盘关闭保留进程/系统通知权限与送达 | UNVERIFIED；代码已实现 | UNVERIFIED | UNVERIFIED |
| 屏幕热插拔/DPI/系统午夜/睡眠/时区 | UNVERIFIED；边界算法/定时刷新已有 | UNVERIFIED | UNVERIFIED |
| 10k加载/搜索/月索引 | PASS；38/1/11 ms，当前机器数据层 | UNVERIFIED | UNVERIFIED |
| 透明桌面可读性/IME/读屏/高对比 | UNVERIFIED | UNVERIFIED | UNVERIFIED |
| 签名/独立干净机/8h稳定性 | UNVERIFIED | UNVERIFIED | UNVERIFIED |

部署包在本机去除SDK PATH后单独测试，结果追加验证报告；这不是独立干净机验收。不得批量将本表改为PASS。
