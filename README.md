# Daymark

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/brand/current/png/logo/logo-dark-640.png">
  <img alt="daymark" src="assets/brand/current/png/logo/logo-primary-640.png" width="320" height="72">
</picture>

[品牌规范与完整VI](resource/VI/README.md) · [品牌软件接入](docs/design/brand-identity-integration.md)

Windows/macOS 离线任务管理与日历应用：C++20 + **Qt 6.10.2 / Qt Quick** + SQLite本地优先存储。

**简约而不简单**：记下事情，明确下一步，安心完成；组织方式可选，数据与恢复可靠。[产品哲学](docs/product-philosophy.md)规定功能与界面的取舍。

## 功能概览

- **统一任务**：所有内容都是任务，支持起止时间、循环规则、周期提醒、真实父子任务；分类取代项目/领域，子任务取代检查清单。
- **五个自动视图**：今天、计划、全部、特别关注、完成，切换列表/日历两种查看方式。
- **日历**：连续月历、日/周时间轴、拖选排期、年视图与独立桌面日历。
- **桌面组件**：浮动小窗、Mini 与桌面日历，不抢焦点、可置顶或置于桌面层。
- **数据安全**：软删除与撤销、一致性快照备份、JSON/CSV 导入导出、升级迁移。
- **离线优先**：默认仅本地保存，无遥测，核心操作不依赖网络。
- **可选同步**：官方账号或自定义HTTPS服务，支持离线队列、冲突处理与独立账号资料；[服务端指南](server/README.md)。官方构建地址按[本地配置说明](config/README.md)提供，公开源码仅示例。
- **Windows交付**：完整安装向导、托管便携包和签名在线更新；标题栏融入界面，并可在外观设置切回系统原生。

当前版本 **2.2.4**，数据库 schema 10。[Windows 下载](https://github.com/downdone/daymark/releases/tag/v2.2.4)，实际验证见[进度](docs/PROGRESS.md)。

## 构建

需要 Qt **6.10.2（EXACT）**、CMake ≥ 3.24、Ninja、C++20 编译器：

| 平台 | 编译器 | 其它 |
|---|---|---|
| Windows | MinGW GCC 13.1 或 MSVC 2022 | PowerShell 7（测试/打包脚本） |
| macOS | Apple Clang（Xcode） | 最低 macOS 13 |
| Linux | GCC 13 | X11/OpenGL 运行库；按需手动构建 |

当前产品CI优先Windows，使用开源Qt 6.10.2/MSVC 2022执行完整CTest和QML lint；macOS/Linux本轮按用户指示跳过。

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

打 `v*` 标签会触发 [release-build.yml](.github/workflows/release-build.yml)，生成完整测试通过的Windows候选；随后由受保护的 [windows-update.yml](.github/workflows/windows-update.yml) 签名、独立部署验收并发布安装包、便携包与更新清单。

完整环境说明见[环境与构建](docs/environment.md)；测试分层见[测试方法](docs/testing-method.md)。

## 项目状态

Windows 与 macOS 是工程目标，当前优先交付Windows。正式包使用项目固定指纹自签名及P-256更新签名，尚非Windows公共发行者信任。独立Windows部署、安装与公网升级和物理输入、多屏/DPI、读屏等实机边界分别记录，不能以版本号代替验收。

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
- 发行说明：[1.8.1](docs/release-notes-v1.8.1.md)。

Things 仅为交互原则参考，未复制其图标、字体或二进制资产。
