# ADR-025：面向用户的最小便携包

2026-10-28，接受。用户要求修正整目录复制docs及其他冗余内容。覆盖ADR-024的包内验收工具位置，保留其完整测试和预览失败白名单门禁。

运行包只包含程序、实际运行依赖、qt.conf、简明README、第三方说明、Qt SBOM、BUILD-INFO/manifest及预览TEST-RESULTS.json。开发文档、设计/进度/ADR、源码、验收脚本、诊断原始日志、SDK工具和私人数据信息不得进入用户包；校验器从仓库scripts/Test-Package.ps1运行，报告留包外build。规则由校验器白名单执行，额外文件即使被重新写入manifest也必须拒绝。

Qt固定6.10.2，DaymarkStyle与Basic回退固定于入口：部署后去除其他可选QuickControls样式及对应独立DLL、qmltooling调试插件、未使用SQL驱动和.qmltypes工具元数据（qmldir去typeinfo声明），保留可执行QML/qmldir/图形资源、模板/Basic/Dialogs依赖。软件OpenGL及D3D编译器是图形兼容回退，不按一次观察的未加载模块删除。SBOM/第三方资料是依赖追溯，不以其存在推断许可已确认。

依据[Qt 6.10 qmldir说明](https://doc.qt.io/qt-6.10/qtqml-modules-qmldir.html)：typeinfo用于外部QML工具的类型描述；它与运行时的plugin/import/dependencies声明分开。本机windeployqt --help提供按插件类型及名称排除的官方入口，实际裁剪结果另做运行验证。

在新建输出目录按明确集合裁剪，路径必须解析到该目录内；不修改SDK或历史包。真实SDK隔离启动、完整功能关联回归和ZIP逐文件检查通过后交付，Windows结果不扩展为macOS或干净机通过。版本仍1.1.0，以真实源码SHA和独立文件名区分本地预览。

额外审计：MinGW Release主程序仍带大量COFF符号表。仅对部署副本执行匹配Kit的strip --strip-unneeded，保留原build产物供调试；PE资源、导入、重定位与x64异常展开段必须保留，最终版本/原生功能仍从裁剪后文件校验。generic下的TUIO协议插件不属于Windows原生鼠标/触摸处理，当前产品未使用，随qmltooling通过官方按类型排除。图形兼容DLL继续保留。
