# Daymark

Windows/macOS离线任务管理与日历应用，C++20 + **Qt 6.10.2 / Qt Quick** + SQLite。

v0.1本地版已实现离线任务、项目/领域/标签/清单、Today/今晚与排序、月/周日历、基础重复和提醒、托盘与桌面组件、主题设置，以及JSON导入导出/快照恢复/每日备份。

本轮按用户最新要求先交付本地Windows版本，未推送GitHub。macOS、真实OS穿透/多屏/层级/焦点和8h稳定性保持UNVERIFIED，UI目标仍需完整体验验收。操作说明见[使用指南](docs/user-guide.md)、[发行说明](docs/release-notes-v0.1.md)。

## 开始开发

1. 阅读 [AGENTS.md](AGENTS.md)、[文档导航](docs/README.md)、[当前进度](docs/PROGRESS.md)。
2. 按 [环境与构建](docs/environment.md) 设置DAYMARK_QT及Windows的DAYMARK_MINGW。
3. Windows运行 `./scripts/build-windows.ps1`、`./scripts/test-windows.ps1`、`./scripts/run-windows.ps1`。
4. macOS运行 `bash scripts/build-macos.sh`；当前未验证该平台。
5. Windows Release构建测试后，`./scripts/package-windows.ps1`生成带运行库的本地验证包并在不含SDK的PATH下验证。公开再分发许可见[第三方说明](docs/third-party-notices.md)。

## 项目资料

- [产品规格](docs/01_PRODUCT_SPEC.md)、[技术基线](docs/02_TECHNICAL_DESIGN.md)、[原始计划](docs/03_IMPLEMENTATION_PLAN.md)。
- [AI开发方法](docs/engineering.md)、[工程原则](docs/principles.md)。
- [详细设计](docs/design/architecture.md)、[模块契约](docs/design/contracts.md)、[界面设计](docs/design/ui.md)。
- UI目标：[Things/Apple调研](docs/design/ui-research.md)、[UI/UX设计基准](docs/design/ui-baseline.md)、[UX验收](docs/design/ui-acceptance.md)。
- [里程碑任务](docs/milestones.md)、[验证报告](docs/verification-report.md)、[平台矩阵](docs/platform-test-matrix.md)。

顺序：M0窗口POC → M1可靠基础 → M2数据闭环 → M3主窗 → M4日历 → M5桌面MVP → M6重复提醒 → M7备份/稳定性/双平台发布。工程基线不等于M0/M1整体完成。

V1无账号、云端或AI功能依赖，核心操作离线可用。Things仅为交互原则参考，不复制资产；Daymark是暂定代号，公开发布前核实名称与许可。
