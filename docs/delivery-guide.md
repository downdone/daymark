# Windows本地候选交付与离线校验

候选ZIP是免安装目录包。解压到新目录，保留完整文件树；从托盘退出旧版后再运行daymark.exe，避免单实例激活旧程序。运行核心业务不需要Qt SDK、PowerShell、账号或联网。默认业务库在系统应用数据目录；不要把真实数据库复制进发行目录。

## 用户校验（PowerShell 7）

下载时对ZIP计算SHA256，与随包侧车及交付渠道公布的值比较；校验和只能检测差异，不能代替可信签名。解压后从候选目录执行：

```powershell
Get-FileHash './Daymark-版本-windows-x64-local-时间.zip' -Algorithm SHA256
# 切换到已解压的候选目录；无需安装Qt或设置DAYMARK_QT。
pwsh -NoProfile -File ./verification/Test-Package.ps1
```

校验器先只读检查整个目录的清单、文件长度/SHA、版本与敏感文件，再以系统PATH运行--version及完整原生smoke。所有业务操作使用QTemporaryDir合成库，不访问现有任务库、不建立托盘或修改启动项；会出现测试窗口。运行期间检查加载的包内DLL是否实际来自包内，至少观察Qt6Core、qwindows和qsqlite。完成后再次检查目录，任何错误非零退出。

Qt DLL文件版本另行核对为6.10.2。系统可能选择Windows/System32中的D3DCompiler_47.dll代替包内副本；仅允许这一精确系统路径，记录在windowsSystemModules。Qt、MinGW、QML插件仍必须来自候选目录，任意SDK或其他目录来源均失败。

成功信息给出包外临时报告目录，内含report.json和本机诊断日志；可用-ReportParent指定已有的包外目录。不要将报告复制回候选目录，否则下次校验会因额外文件失败。日志可能含本机路径，不直接上传。PATH和Qt/QML搜索环境变量在成功或失败后恢复；校验器只结束自己启动的测试子进程。

report.json的passed只表示这台机器的SDK隔离原生检查通过；cleanMachineVerified和redistributionLicenseVerified仍为false。脚本不会把本机结果转成独立干净机、签名、许可、8h、睡眠、多屏或macOS通过。

发现损坏时重新解压可信来源的完整ZIP到新目录，不通过手改manifest绕过检查。真实用户数据独立保存，校验器不会修复、删除或替换它。

## 开发者打包（Windows）

先设置[Qt/MinGW环境](environment.md)，提交源码并保持工作区干净：

```powershell
./scripts/package-windows.ps1
# 指定版本只用于核对，必须与CMake DAYMARK_VERSION一致，不能重标旧二进制。
./scripts/package-windows.ps1 -Version 1.0.0-rc.2
```

脚本重新构建并测试当前Release，部署运行库、说明、SBOM和独立验收器，生成源码提交及版本元数据/manifest，再运行无SDK验收；通过后生成新的ZIP及SHA侧车，并逐项比较ZIP内容与已验证目录。失败保留独立候选目录用于诊断，不覆盖历史包，不自动推送或上传。合成篡改回归已接入Windows CTest的package_validation，也可单独执行tests/scripts/PackageValidationTest.ps1；合成文件不用于证明Qt可以运行。

升级/回退与数据库备份继续遵循[RC2说明](release-notes-v1-rc2.md)。公开分发前核实[第三方许可](third-party-notices.md)，当前版本不包含安装器、签名或自动升级。

## macOS差异

Windows验收器依赖Windows后端与进程模块枚举，不能在macOS上作为通过证据。macOS先用bash scripts/build-macos.sh构建，再另行完成macdeployqt、签名/公证、Intel/ARM及独立系统验收；本轮未执行。
