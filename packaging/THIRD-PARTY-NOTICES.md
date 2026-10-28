# 第三方组件

本地包使用 Qt 6.10.2 的 Core、Gui、Widgets、Sql、Network、Qml、Quick、QuickControls2、QuickDialogs2、QuickLayouts、QuickShapes 等实际依赖；SQLite 由 QSQLITE 插件提供。MinGW 运行库包含 libgcc、libstdc++ 和 winpthread。软件图形及 D3D 编译器用于兼容回退。

Qt 模块及其第三方组件的版本、版权及许可声明见随包 qt-sbom/*.spdx.json。SBOM 不代表再分发审查已完成；本机 Qt 的许可选择及公开再分发条件仍待核实。此包不授予额外许可，也不是已签名的正式发行。

Daymark 使用原创图形和系统字体回退，不捆绑 Apple/Things 专有图标或字体。项目源码及完整第三方记录在源码仓库维护。

Qt 官方许可与部署说明：https://doc.qt.io/qt-6/licensing.html 、https://doc.qt.io/qt-6.10/deployment.html 。
