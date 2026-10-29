# 第三方组件与许可说明

## 运行时组件

Daymark 桌面使用 Qt 6.10.2：Core、Gui、Qml、Quick、QuickControls2、Widgets、Sql、Network 及其 QML 运行模块。桌面SQLite由Qt的QSQLITE插件提供，未引入云SDK或遥测。Windows包另含MinGW运行时libgcc、libstdc++、winpthread。

可选服务端为独立Go1.27.0 module，直接依赖modernc.org/sqlite、golang.org/x/crypto、golang.org/x/sys及jsonschema/v6；必要性、版本、许可和替代方案见[ADR-029](ADR/029-server-runtime-dependencies.md)。13个选定直接/间接模块的原许可证/NOTICE见[server/licenses](../server/licenses/modules.json)，清单包含版本、Go sum及文本SHA256；CI验证清单与go.mod选定模块一致。SQLite引擎自身public domain与驱动/传递组件许可分开保留；发布服务二进制须同时分发该目录及Go工具链的适用声明。模块清单不是法律审查或生产部署认证。

图标由本项目 QPainter/QML 绘制，或使用操作系统提供的字体字符；未复制 Things、Apple 字体或 SF Symbols 资源。UX-41 的 daymark 字标由本项目以 QML 矢量路径独立绘制，不捆绑外部字体资源。

## Qt 许可

本项目按 **LGPLv3 动态链接**使用 Qt：不修改 Qt 源码、不静态链接 Qt 库，用户可替换 Qt 共享库。公开分发二进制时须随包提供 LGPLv3 文本、Qt 版权声明及对应 Qt 源码的获取方式。

许可依据与部署条件见 [Qt 许可说明](https://doc.qt.io/qt-6/licensing.html)、[Qt 6.10 部署](https://doc.qt.io/qt-6.10/deployment.html)。Daymark源码使用LGPLv3，全文见[LICENSE](../LICENSE)；其并入的GPLv3条款见[GPL文本](../packaging/licenses/GPL-3.0.txt)。

## 构建与 CI

CI 的 `desktop` 作业只从 Qt 官方开源归档安装 Qt 6.10.2（aqtinstall 默认 `opensource` 通道，社区版 LGPLv3/GPLv3），并在构建日志回显 Kit 路径与 `LICENSES` 目录以便审计；决策与后果见 [ADR-027](ADR/027-open-source-qt-ci.md)。发布二进制必须由该开源 Kit 构建。

构建期 Actions 均为 MIT 许可，仅用于构建与测试，不进入产品运行时：

- `actions/checkout`（MIT）
- `jurplel/install-qt-action`（MIT）
- `aqtinstall`（MIT，由上述 Action 调用）

## 本地验证包

本地预览包用于本机验证，不是已签名或已通过再分发审查的正式发行。包内附 Qt SBOM（`qt-sbom/*.spdx.json`）便于逐项核对；SBOM 存在不代表许可审查已完成。校验和不能代替可信签名。

构建包时使用的 SDK 由开发者自行提供，其版本与许可配置不进入仓库；仓库与发布产物不包含 Qt SDK 或工具链。

## 资产

UI-12 增加 Daymark 原创日历勾选应用标识，SVG/PNG/ICO/ICNS 均由仓库脚本与系统 Drawing API 生成，无下载素材或新增运行时依赖。生成流程记录在仓库中，不视为公开许可已完成。

品牌参考不构成第三方资产许可；Things 仅作交互原则参考，未复制其图标、字体或二进制资源。
