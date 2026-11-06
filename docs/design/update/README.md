# 在线更新开发设计包

Windows SDK、CLI、Win32观察界面、Qt接线与发行工具的设计入口。实际实现、正式GitHub发布与真实公网升级的当前证据统一见[进度](../../PROGRESS.md)。用户授权完整实现与Windows全流程，后续明确GitHub流水线本期只做Windows。正式GitHub发布、签名配置与独立部署门禁分别验证，不由本地测试代替。

产品主流程固定为：**启动后检查 → 自动启动独立更新器下载/校验/安装到新版目录 → 提醒重启 → 用户重启或下次启动切换 → 新版确认正常后删除旧程序**。GitHub Releases 已承担版本/资源托管查询，不另外开发服务端，也不注册 Windows 常驻服务。

| 顺序 | 文档 | 开发前应得到的结论 |
|---|---|---|
| 0 | [方案评审](review.md) | 原方案缺口、修正与剩余发布前置条件 |
| 1 | [功能定义](functional-spec.md) | UF-01～16行为、完整状态与验收 |
| 2 | [技术选型](technology.md) | Qt/原生边界、依赖版本、协议/资源限制 |
| 3 | [架构](architecture.md) | 文件归属、进程/锁、启动写门、提交/清理恢复 |
| 4 | [模块接口](modules.md) | 类型、接口、IPC消息、持久化数据与模块级验收 |
| 5 | [验证与实施顺序](verification.md) | 单元/集成/真实Windows故障矩阵和U1～U6交付门槛 |
| 6 | [轻量UI与CLI](native-ui.md) | SDK复用，Win32/AppKit/可选GTK薄UI及CLI独立运行边界 |
| 7 | [GitHub前置准备](github-prerequisites.md) | 公钥先行、签名Environment/Secrets、最小发布权限、资产就绪与首次启用 |
| 8 | [开发与发布操作](implementation.md) | 实际源码/命令、SDK结果边界、生产与测试隔离、发布门禁 |
| 9 | [自签名与验证](self-signing.md) | 自有更新密钥、固定证书指纹、独立信任验证与非公共信任边界 |

与[总体方案](../windows-online-update.md)、[ADR-033](../../ADR/033-github-windows-update.md)共同使用。**本包中更具体的类型、协议、提交/清理与选型约束取代总体方案的早期接口草案**；产品行为与用户最新指令一致。当前实现/验证事实只记于[PROGRESS](../../PROGRESS.md)。

首期边界：Windows x64、稳定频道、当前用户可写本地NTFS、同数据schema/读写ABI、整包更新。首次引导版本手动安装；根启动器/更新器的自身升级与数据迁移首期手动处理。具体支持的Windows最低build/VC++版本只能在干净机验证后冻结为正式清单值，不能把示例当支持证据。
