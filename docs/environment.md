# 环境与可复现构建

## 本轮识别

2026-10-22：Windows11专业版x64，系统版本10.0.26300；Qt6.10.2 mingw_64；匹配MinGW GCC13.1.0；CMake3.29.2；Ninja1.12.0。Kit包含Quick/Controls/Sql/Test。macOS工具链未在本机验证。

本机路径不进入共享预设；通过当前Shell变量或忽略的CMakeUserPresets.json保存。禁止把Strawberry附带g++与Qt Kit混用。

## Windows（PowerShell）

```powershell
# 替换为本机Qt和配套编译器目录，不能仅指向Qt根目录。
$env:DAYMARK_QT = '<Qt根目录>/6.10.2/mingw_64'
$env:DAYMARK_MINGW = '<Qt根目录>/Tools/mingw1310_64'
./scripts/build-windows.ps1
./scripts/test-windows.ps1
./scripts/run-windows.ps1
./scripts/build-windows.ps1 -Preset windows-release
./scripts/test-windows.ps1 -Preset windows-release
./scripts/run-windows.ps1 -Preset windows-release
./scripts/test-window-native.ps1 -Preset windows-release
./scripts/check-docs.ps1
```

CMake>=3.24、Ninja须在PATH。预设在子进程环境前置匹配编译器和Qt，脚本检查原生命令退出码。运行脚本启动有窗口的GUI，Hidden仅避免额外控制台。

直接命令为 `cmake --preset windows-debug`、`cmake --build --preset windows-debug`、`ctest --preset windows-debug`。产物位于build/windows-debug/bin；运行脚本使用当前Kit DLL，不代表安装包可分发。

## macOS（bash）

2026-10-24本机构建环境：macOS15.8.1 / Apple M3 Pro（arm64）、Xcode15.4 / Apple Clang15.0.0、CMake3.31.10、Ninja1.13.0、Qt6.10.2 macOS Kit（含qtbase/qtdeclarative/qtsvg/qttools/qttranslations）。Debug/Release编译及各16项CTest、QML lint和Cocoa原生合成流程已有实际证据，最新范围见[进度](PROGRESS.md)。Intel、物理交互/读屏/IME、独立部署和签名仍分别待验；合成截图不替代这些门禁。

```bash
export DAYMARK_QT='<Qt安装目录>/6.10.2/macos'
bash scripts/build-macos.sh macos-debug
bash scripts/build-macos.sh macos-release
./build/macos-debug/bin/daymark.app/Contents/MacOS/daymark
```

要求Xcode/Apple Clang、CMake>=3.24、Ninja及对应架构Qt；Intel/Apple Silicon分别验证，最低macOS13。发布仍需macdeployqt及签名/公证核查。

本轮本机工具均安装于忽略的build/toolchain，并生成仅本机使用的环境文件；在仓库根目录可直接复建：

```bash
source build/toolchain/env.sh
bash scripts/build-macos.sh macos-debug
cmake --build --preset macos-debug --target daymark_qmllint daymark_style_qmllint
bash scripts/build-macos.sh macos-release
# 隔离合成数据，不能当作物理键鼠或真实OS通知验收。
QT_QPA_PLATFORM=cocoa ./build/macos-release/bin/daymark.app/Contents/MacOS/daymark --smoke-test --screenshot build/ui-smoke.png
```

该环境文件仅在已准备工具的本机存在，不提交也不代替其他机器配置DAYMARK_QT。测试日志位于build/macos-debug/Testing/Temporary。生成的daymark.app仍引用本机Kit，不是可独立分发的macOS安装包；正常运行会创建用户业务库，自动验证使用脚本内的隔离smoke。

## 测试与诊断

Windows构建测试/交付脚本使用PowerShell 7（pwsh）；CTest额外执行package_validation合成篡改回归。独立候选包校验无需Qt SDK，见[交付指南](delivery-guide.md)。

- sqlite_runtime使用QTemporaryDir，验证QSQLITE/WAL、回滚、中文往返、重开；不是Repository/迁移测试。
- qml_startup使用offscreen及QTemporaryDir真实文件库，加载模块、等待worker就绪，并通过实际QML输入/详情信号执行新增→非法日期拒绝保留草稿→编辑→完成/恢复→软删撤销→未保存离开保护；检查两窗口且无QML警告，不写真实用户库。
- CTest使用noTestsAction=error；零测试不能冒充通过。日志在对应build目录Testing/Temporary，原始日志含本机路径，不提交。
- 配置提示缺少WrapVulkanHeaders，但默认路径本轮构建通过；Vulkan后端未验证。
- moc输出许可客户端期望服务版本3.6.1、安装服务不匹配。本轮编译及测试仍成功；未修改许可配置或启用绕过变量。发布前核实Kit许可来源及服务状态。
- QML加载失败：正常启动显示错误对话框；自动smoke返回失败，避免模态弹窗挂住测试。

## W-01窗口验证

新增widget_window Qt Test，默认offscreen；测试隐藏/显示状态、模式往返尺寸位置、取消置顶、失败回滚和销毁安全。原生脚本显式使用windows后端运行同一测试与启动smoke，输出JUnit XML到对应build目录；测试无业务DB访问，不包含实际鼠标拖动、焦点或桌面窗口遮挡验证。

QML静态检查：`cmake --build --preset windows-debug --target daymark_qmllint`。它检查QML类型和属性，不验证视觉布局。

## 基础任务验证

CTest共6项：sqlite_runtime、task_service、task_workspace、single_instance、widget_window、qml_startup。任务/数据库测试均用QTemporaryDir；单实例测试启动真实第二进程，临时目录包含空格。

正常启动会创建用户业务库；只有smoke和自动测试强制隔离。--data-dir允许开发者指定独立目录，不要指向真实用户测试数据。--smoke-test --screenshot <本地PNG路径>在合成流程中保存主界面，以及追加.editor.png的窄窗错误详情截图；只用于开发验证，不含真实用户任务。

Qt SQL连接仅在worker创建、使用和销毁。初始迁移001已作为本轮schema交付；后续schema变化必须新增迁移，不覆盖001。手动快照已验证，自动每日备份、导入恢复与未来版本迁移仍属后续工作。
