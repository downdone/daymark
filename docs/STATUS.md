# 当前状态

2026-11-07：**Daymark 2.0.2已发布，Windows在线更新已启用**；schema10、HTTP JSON v1、Qt6.10.2。

- Windows完整CTest 40/40，桌面和真实Qt→Go TLS同步CI成功。
- 独立Windows实际匿名GitHub 2.0.1→2.0.2升级通过，生产Qt保存重启、持久提交、任务保留及旧程序清理均已验证。
- 使用项目P-256更新签名与固定指纹自签名EXE；不是Windows公共发布者信任，不导入根证书。
- Linux/macOS按用户指示跳过；本机CDN超时、他机网络现场及物理/长时矩阵保持明确边界。

当前事实、资产身份、证据与剩余边界统一见[PROGRESS](PROGRESS.md)；下载见[2.0.2 Release](https://github.com/downdone/daymark/releases/tag/v2.0.2)。旧状态页与逐轮原文已完整保留在[历史](PROGRESS-HISTORY.md)，不再作为当前待办。

TIME-01智能时间输入及新增能力设计标准已本地完成，尚未发布；具体评审与验证边界见[PROGRESS](PROGRESS.md)。

2.1.1性能/对象生命周期、离线时间识别及Windows安装器已实现，本地完整测试与原生验收详见[PROGRESS](PROGRESS.md)。当前正在完成正式流水线，尚未发布2.1.1，不把本地测试等同正式MSVC产物验收。

2.1.0仅源码的Release已因生成映射门禁失败转草稿，原标签保留；2.1.1修复并纳入CTest，正式发布尚待验证。
