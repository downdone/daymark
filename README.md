# Daymark

Windows/macOS 离线任务管理与日历应用：C++20 + **Qt 6.10.2 / Qt Quick** + SQLite本地优先存储。

**简约而不简单**：记下事情，明确下一步，安心完成；组织方式可选，数据与恢复可靠。[产品哲学](docs/product-philosophy.md)规定功能与界面的取舍。

## 功能概览

- **统一任务**：所有内容都是任务，支持起止时间、循环规则、周期提醒、真实父子任务；分类取代项目/领域，子任务取代检查清单。
- **五个自动视图**：今天、计划、全部、特别关注、完成，切换列表/日历两种查看方式。
- **日历**：连续月历、日/周时间轴、拖选排期、年视图与独立桌面日历。
- **桌面组件**：浮动小窗、Mini 与桌面日历，不抢焦点、可置顶或置于桌面层。
- **数据安全**：软删除与撤销、一致性快照备份、JSON/CSV 导入导出、升级迁移。
- **离线优先**：默认仅本地保存，无遥测，核心操作不依赖网络。
- **可选同步**：登录后同步到自管Go + SQLite服务，支持离线队列、冲突处理与独立账号资料；[服务端指南](server/README.md)。

当前版本 **1.8.0**，数据库 schema 10。实现与验证状态只以[进度](docs/PROGRESS.md)为准。

## 构建

需要 Qt **6.10.2（EXACT）**、CMake ≥ 3.24、Ninja、C++20 编译器：

| 平台 | 编译器 | 其它 |
|---|---|---|
| Windows | MinGW GCC 13.1 或 MSVC 2022 | PowerShell 7（测试/打包脚本） |
| macOS | Apple Clang（Xcode） | 最低 macOS 13 |
| Linux | GCC 13 | X11/OpenGL 运行库；CI 用 `ubuntu-24.04` |

三个平台都在 CI 中构建、跑完整 CTest 与 QML lint。

```bash
# Windows（设置 DAYMARK_QT 指向 6.10.2/mingw_64，DAYMARK_MINGW 指向 Tools/mingw1310_64）
./scripts/build-windows.ps1
./scripts/test-windows.ps1

# macOS（设置 DAYMARK_QT 指向 6.10.2/macos）
bash scripts/build-macos.sh macos-release

# Linux（设置 DAYMARK_QT 指向 6.10.2/gcc_64）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

打 `v*` 标签会触发 [release-build.yml](.github/workflows/release-build.yml)：三平台构建后把归档附加到源码 Release。

完整环境说明见[环境与构建](docs/environment.md)；测试分层见[测试方法](docs/testing-method.md)。

## 项目状态

Windows 与 macOS 是工程目标。CI 在两端执行构建、完整 CTest 与 QML lint；平台专属门禁（物理输入、通知、穿透、多屏/睡眠、干净机部署、签名）仍需分别验收，不能以版本号代替。

## 许可

本项目源码以 **GNU Lesser General Public License v3.0** 发布，全文见 [LICENSE](LICENSE)。

运行时使用 Qt 6.10.2，按 LGPLv3 动态链接，不修改 Qt 源码、不静态链接。公开分发的二进制须由开源 Qt（LGPLv3）构建，并随包提供 LGPLv3 文本、Qt 版权声明与源码获取方式。详见[第三方说明](docs/third-party-notices.md)与 [ADR-027](docs/ADR/027-open-source-qt-ci.md)。

## 参与

- [文档导航](docs/README.md)：项目文档总入口。
- [开发方法](docs/engineering.md) 与 [工程原则](docs/principles.md)：提交、验证与分层纪律。
- [AGENTS.md](AGENTS.md)：代码代理的工作约束。

## 项目资料

- [产品规格](docs/01_PRODUCT_SPEC.md)、[技术基线](docs/02_TECHNICAL_DESIGN.md)、[原始计划](docs/03_IMPLEMENTATION_PLAN.md)。
- [详细设计](docs/design/architecture.md)、[模块契约](docs/design/contracts.md)。
- UI：[调研](docs/design/ui-research.md)、[设计基准](docs/design/ui-baseline.md)、[验收](docs/design/ui-acceptance.md)。
- [里程碑](docs/milestones.md)、[平台矩阵](docs/platform-test-matrix.md)、[发布规则](docs/release-policy.md)。
- 发行说明：[1.8.0](docs/release-notes-v1.8.0.md)。

Things 仅为交互原则参考，未复制其图标、字体或二进制资产。
