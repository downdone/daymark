# 第三方组件

本地包使用 Qt 6.10.2 的 Core、Gui、Widgets、Sql、Network、Qml、Quick、QuickControls2、QuickDialogs2、QuickLayouts、QuickShapes 等实际依赖；SQLite 由 QSQLITE 插件提供。MinGW 运行库包含 libgcc、libstdc++ 和 winpthread。软件图形及 D3D 编译器用于兼容回退。

Qt 按 LGPLv3 动态链接使用：未修改 Qt 源码、未静态链接 Qt 库，可用同版本的 Qt 共享库替换。公开发布时须随包附带 LGPLv3 文本、Qt 版权声明及对应 Qt 源码的获取方式。Qt 模块及其第三方组件的版本、版权及许可声明见随包 qt-sbom/*.spdx.json。

此包用于本地验证，不授予额外许可，也不是已签名的正式发行。SBOM 便于逐项核对，但不等于再分发审查已完成。

Daymark 使用原创图形和系统字体回退，不捆绑 Apple/Things 专有图标或字体。项目源码及完整第三方记录在源码仓库维护。

Qt 官方许可与部署说明：https://doc.qt.io/qt-6/licensing.html 、https://doc.qt.io/qt-6.10/deployment.html 。

## CI标签候选的Qt追溯

Qt is Copyright (C) The Qt Company Ltd. and other contributors. 逐模块版权和第三方声明见qt-sbom/*.spdx.json。CI标签候选随附LGPL-3.0.txt、GPL-3.0.txt；Daymark及Qt共享库动态链接，允许替换兼容Qt运行库，并允许为调试这类修改而反向工程。

Qt6.10.2未修改源码可从官方完整归档取得：
https://download.qt.io/archive/qt/6.10/6.10.2/single/qt-everywhere-src-6.10.2.tar.xz
（同目录另有ZIP和校验文件）。构建与重新链接Daymark使用对应Git标签源码及docs/environment.md中的命令；不设置运行库替换锁定。第三方SBOM中的许可证条件仍分别适用。

以上CI候选由开源Qt Kit生成，未签名且未通过独立干净机验收。前文“本地包”的MinGW与用途描述仅适用于本地验证包，CI Windows候选使用MSVC、另需VC++运行库。
