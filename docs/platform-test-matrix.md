# 平台验证矩阵

2026-10-22，1.0.0-rc.1 Windows本地候选。用户确认暂缺macOS环境；PASS只针对行内所述范围，macOS未执行任何本轮构建。

| 场景 | Windows11 x64 / Qt6.10.2 MinGW | macOS Intel | macOS Apple Silicon |
|---|---|---|---|
| Debug/Release配置与编译 | PASS | UNVERIFIED | UNVERIFIED |
| 批量事务/归档/schema2迁移/旧JSON/周期配置 | PASS；临时SQLite与失败注入 | UNVERIFIED | UNVERIFIED |
| Windows开机启动 | 隔离INI测试PASS；真实Run登记/系统登录触发UNVERIFIED | 未实现原生启动适配 | 未实现原生启动适配 |
| 每视图位置/折叠导航/大字列表/Floating-Mini | PASS；QML及原生合成流程/截图 | UNVERIFIED | UNVERIFIED |
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
| 10k混合数据加载/搜索/月索引 | PASS；155/2/11 ms，i7-13700HX Release数据层 | UNVERIFIED | UNVERIFIED |
| 透明桌面可读性/IME/读屏/高对比 | UNVERIFIED | UNVERIFIED | UNVERIFIED |
| 签名/独立干净机/8h稳定性 | UNVERIFIED | UNVERIFIED | UNVERIFIED |

部署包在本机去除SDK PATH后单独测试，结果追加验证报告；这不是独立干净机验收。不得批量将本表改为PASS。

## V-02W自动恢复与短跑补充（2026-10-23）

| 范围 | Windows11 x64 / Qt6.10.2 | macOS |
|---|---|---|
| 时间/恢复MSG类型、合并、析构取消 | PASS；消息注入，非真实睡眠 | UNVERIFIED |
| 写入时延后维护、保留校验错误与编辑状态 | PASS；隔离模型及原生合成流程 | UNVERIFIED |
| 主窗规范化、隐藏/最小化保持、销毁安全 | PASS；Qt状态与合成离屏坐标 | UNVERIFIED |
| 连续业务循环/完整性检查/资源采样工具 | PASS；Debug 3循环、Release 6循环 | UNVERIFIED |
| Release 10k混合数据层 | PASS；129/1/8ms加载/搜索/月索引 | UNVERIFIED |
| 8h、真实睡眠、物理拔屏/DPI、闲置CPU与内存趋势 | UNVERIFIED；短跑不能替代 | UNVERIFIED |

本轮数据与短跑限制见[验证报告](verification-report.md)，执行方法见[指南](stability-guide.md)。

## CAL-05浏览与定位补充（2026-10-24）

Windows自动：实例排序/只读/失效key/软删测试PASS；Qt合成主/独立窗日期跳转、非法日期保持、实际预览/编辑路径PASS；原生180阶段、浅深/200%截图与420×480逻辑窄窗200%预览PASS。严格SDK隔离、清单/ZIP与运行库来源PASS。上述不替代物理键鼠/IME/读屏/多屏/真实通知/8h或干净机，macOS仍UNVERIFIED。当前包与源码证据只见PROGRESS。

UI-18：Windows原生219阶段自动合成场景/截图PASS，包括快速→完整共享草稿、明确离开保护、内联日期和独立日期菜单当前值、时间选择取消/错误保留、深色200%固定操作及既有滚动回归。最终构建/CTest/严格本地部署数据见PROGRESS；不等同真实鼠标/IME/读屏验收，macOS仍UNVERIFIED。

UI-19验收范围：Windows新增原行展开/完整可见、长备注摘要不丢内容、大字号焦点与手动滚动、新建资料与任务草稿的独立提交、成功完成反馈/减少动态效果、菜单同窗呈现和10k菜单可用性预算。命令耗时与原生两次呈现上界分别记录，不宣称所有物理输入延迟。最终执行结果见PROGRESS；真实键鼠/IME/读屏/多屏/睡眠/8h/干净机和macOS仍UNVERIFIED。

UI-20新增行范围：连续跨日周段/循环日程实际实例与时间预览、精简提醒一次提交、任务和清单合成拖动/Esc/落库、导航独立折叠偏好、校验错误放弃和存储错误保留。Windows构建/数据/合成QML证据见PROGRESS；物理手势/真实键盘读屏/平台系统门禁及macOS仍UNVERIFIED。
