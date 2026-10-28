# Daymark Windows 便携版

解压到新目录，退出旧版后运行 daymark.exe，保留随包目录结构。无需安装 Qt。任务数据默认保存在系统应用数据目录，升级时不要删除旧数据；包内不包含个人数据库。设置中可以调整外观、桌面组件和数据备份。

本地预览的测试结果和已知失败见 BUILD-INFO.json 与 TEST-RESULTS.json；本地预览不代表正式二进制验收通过。版本与实际源码提交以 BUILD-INFO.json 为准。

manifest.json 记录逐文件 SHA256，ZIP 随附 .sha256 校验文件；校验和不能代替可信签名。完整使用指南和开发校验器保留在源码仓库，未复制进运行包。

第三方及运行库边界见 THIRD-PARTY-NOTICES.md 和 qt-sbom。当前包仅供本地使用；公开再分发许可、签名、独立干净机和 macOS 尚未核实。
