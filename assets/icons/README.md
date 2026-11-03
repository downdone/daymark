# Daymark 平台图标

唯一源为[批准A母版](../brand/current/README.md)，炭黑D形记录轮廓与四色短笔；旧日历勾停止使用。`daymark.svg`是载体矢量，PNG覆盖16/20/24/32/48/64/128/256/512/1024；48物理px起四色，小号单色。ICO含8种尺寸PNG表示；ICNS含11个原生表示。托盘黑/白各16/20/24/32，始终单色，macOS作为alpha模板使用。

Windows：`./scripts/generate-app-icons.ps1`；macOS：`python assets/brand/current/source/build_identity.py --runtime`。作者依赖见母版说明；普通CMake构建不需要Python。系统缓存刷新、Finder/Dock和物理混合DPI需分别实测，文件结构正确不能替代。功能操作图标仍由src/ui/style/IconPaths.qml维护。
