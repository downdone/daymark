# 第三方组件与本地包说明

Daymark v0.1运行时使用Qt 6.10.2：Core、Gui、Qml、Quick、QuickControls2、Widgets、Sql、Network及其QML运行模块。SQLite由Qt的QSQLITE插件提供，未引入云SDK、遥测或额外业务依赖。MinGW运行时为libgcc、libstdc++、winpthread，随本地Windows包复制所需DLL。

图标由本项目QPainter/QML绘制或使用系统字体中的基础字符，字体由操作系统提供；未复制Things、Apple字体或SF Symbols资源。

本轮按照用户“先交付本地版本”生成本机验证包。构建机Qt安装含商业许可协议，工具存在许可服务版本诊断；未修改许可配置或绕过检查。**公开再分发许可尚未核实，本地包不代表已获公开分发授权。** 源码没有附带Qt SDK。包内附本机Qt SBOM便于后续逐项核对，SBOM存在不代表许可审查已完成。

Qt支持多种许可选项，具体模块及再分发条件必须按实际选择的许可确认，参考[Qt许可说明](https://doc.qt.io/qt-6/licensing.html)、[Qt 6.10部署](https://doc.qt.io/qt-6.10/deployment.html)。本项目尚未替用户指定源码开放许可。

预备CI中的actions/checkout和install-qt-action为MIT许可，仅用于构建；具体提交与采用理由见ADR-011。CI本轮未推送运行。
