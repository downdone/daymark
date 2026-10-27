# Daymark图标

Daymark原创日历勾选标识，与源码一并维护；项目尚未指定开源许可，不在此另授第三方许可。不含Things、Apple或其他产品资产。

UX-48沿用日历勾身份，以全局强调蓝校准母版和全部平台尺寸，并减轻日历分隔笔画。UI内部使用`src/ui/style/IconPaths.qml`原创24格矢量路径，和应用文件图标分开维护，无字体符号或专有图标包依赖。

`daymark.svg`为矢量母版；PNG/ICO/ICNS与母版几何一致，由仓库脚本生成。

Windows/PowerShell 7重建：`./scripts/generate-app-icons.ps1`，使用系统Drawing API，无额外依赖。macOS直接构建已提交资源，无须运行此脚本；ICNS实际Finder/Dock验收保持UNVERIFIED。

ICO包含16/20/24/32/48/64/128/256像素PNG帧；PNG覆盖应用多分辨率图标和预览；ICNS包含16至1024像素表示。Windows资源路径由CMake生成，不提交本机绝对路径。
