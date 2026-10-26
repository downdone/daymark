# Daymark

Windows/macOS离线任务管理与日历应用，C++20 + **Qt 6.10.2 / Qt Quick** + SQLite。

**简约而不简单**：记下事情，明确下一步，安心完成；组织方式可选，数据与恢复可靠。[产品哲学](docs/product-philosophy.md)与[全产品体验方案](docs/design/product-experience-review.md)规定功能和界面的取舍。

当前源码按[统一任务系统](docs/design/unified-task-system.md)结合日历与提醒事项：所有内容统一为任务，支持起止时间、循环、周期提醒、真实父子任务与两级分类；分类取代项目/领域，子任务取代检查清单。侧栏删除规划，顶部切换列表/日历，共用详情、完成与编辑记录。导航采用今天、计划、全部、特别关注、完成五个自动视图；父子任务完成、删除、分类移动联动，最近删除按同次操作恢复。分类与标签在右键就近管理。数据库schema9；本轮实现与验证仅以[PROGRESS](docs/PROGRESS.md)为准，RC2包描述仅为历史发行范围。

当前源码版本为 **1.0.2**；版本提交与远端标签/发布分别记录，不覆盖已有版本。本轮不附Qt运行库或用户数据。[1.0.2发行说明](docs/release-notes-v1.0.2.md)列出变化和边界，[测试方法](docs/testing-method.md)说明分层、去重及链路覆盖；实际PASS/FAIL/UNVERIFIED只见[进度](docs/PROGRESS.md)。

Windows/macOS是工程目标，当前源码的跨平台CI及物理输入、通知/穿透、多屏/睡眠/8h、独立部署和签名仍需分别验收，不能以1.0版本号或历史测试代替。Qt运行库公开再分发许可未确认，因此Windows便携包暂不公开上传；[RC2记录](docs/release-notes-v1-rc2.md)及[v0.1记录](docs/release-notes-v0.1.md)保留历史。

UI-07统一了主窗、详情、管理弹窗、日历与桌面组件的字体、间距、图标、控件和动作层级。规范见[视觉统一](docs/design/visual-unification.md)，本地预览包与实际验证见[进度](docs/PROGRESS.md)。

## 开始开发

1. 阅读 [AGENTS.md](AGENTS.md)、[文档导航](docs/README.md)、[当前进度](docs/PROGRESS.md)。
2. 按 [环境与构建](docs/environment.md) 设置DAYMARK_QT及Windows的DAYMARK_MINGW。
3. Windows运行 `./scripts/build-windows.ps1`、`./scripts/test-windows.ps1`、`./scripts/run-windows.ps1`。
4. macOS运行 `bash scripts/build-macos.sh`；已验证范围和本机工具环境见[环境与构建](docs/environment.md)。
5. Windows Release构建测试后，`./scripts/package-windows.ps1`生成带运行库的本地验证包并在不含SDK的PATH下验证。公开再分发许可见[第三方说明](docs/third-party-notices.md)。

## 项目资料

- [产品哲学与概念关系](docs/product-philosophy.md)、[产品规格](docs/01_PRODUCT_SPEC.md)、[技术基线](docs/02_TECHNICAL_DESIGN.md)、[原始计划](docs/03_IMPLEMENTATION_PLAN.md)。
- [AI开发方法](docs/engineering.md)、[工程原则](docs/principles.md)。
- [详细设计](docs/design/architecture.md)、[模块契约](docs/design/contracts.md)、[界面设计](docs/design/ui.md)。
- UI目标：[Things/Apple调研](docs/design/ui-research.md)、[UI/UX设计基准](docs/design/ui-baseline.md)、[UX验收](docs/design/ui-acceptance.md)。
- [里程碑任务](docs/milestones.md)、[验证报告](docs/verification-report.md)、[平台矩阵](docs/platform-test-matrix.md)。

顺序：M0窗口POC → M1可靠基础 → M2数据闭环 → M3主窗 → M4日历 → M5桌面MVP → M6重复提醒 → M7备份/稳定性/双平台发布。工程基线不等于M0/M1整体完成。

V1无账号、云端或AI功能依赖，核心操作离线可用。Things仅为交互原则参考，不复制资产；Daymark是暂定代号，公开发布前核实名称与许可。

版本、不可变标签、源码/二进制门禁及隐私检查遵守[发布规则](docs/release-policy.md)。
