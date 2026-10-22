# ADR-007：Qt 6.10.2 与原生工具链

日期：2026-10-22。状态：已接受，用户明确指定。

## 背景和决策

原规格以 Qt 6.8+ 为最低基线。当前实施固定 Qt 6.10.2，CMake 使用 EXACT 校验；不是放宽产品行为。Windows 使用实际安装的 MinGW 64 位 Qt 与 GCC 13.1 配套，不混用 PATH 中其他 GCC 或 MSVC。macOS 使用同版本 macOS Kit 与 Apple Clang，待真实环境验证。

DAYMARK_QT 指向 Kit 目录，DAYMARK_MINGW 指向 Windows 编译器目录；机器绝对路径只由本地环境变量或忽略的 CMakeUserPresets.json 保存。CMake >=3.24、Ninja、C++20。Debug/Release 独立构建目录。

## 方案对比与后果

不沿用宽松的 6.8+ 自动选版，避免不同会话使用不同 Qt；不复制参考项目依赖和 SDK。默认不引入第三方依赖，应用动态链接 Qt。升级 Qt 必须修改 ADR/版本约束并复跑矩阵，Qt 许可和安装包分发条件在发布前单独核查。
