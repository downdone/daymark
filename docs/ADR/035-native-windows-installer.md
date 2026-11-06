# ADR-035：Windows 安装向导复用更新 SDK 的可信目录

状态：接受（2026-11-06，用户授权按新增能力标准开发完整安装包）。设计自审见 SETUP-01，单代理，没有宣称独立评审。

采用 NSIS 3.13 Unicode Modern UI2 作为原生安装/卸载向导，C++20 SetupManager 负责签名、索引、租约、同卷目录事务和保留数据。无需常驻系统服务、管理员权限、额外 Qt 安装器或服务端 API。运行期更新继续使用 GitHub Release、既有 ECDSA P-256 与 payload 切换；安装器负责根宿主的首次安装/修复。不得放宽 MSVC143/MinGW131 兼容门禁。

构建依赖固定官方 [3.13 下载入口](https://prdownloads.sourceforge.net/nsis/nsis-3.13.zip?download)；ZIP SHA256 `ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8`，makensis.exe SHA256 `4ec390d0f96f9728507fbc2f4e91311dd90bfc6c8ddfe2159357b3651ae151c0`。版本/许可依据 [NSIS 官方下载](https://nsis.sourceforge.io/Download)与[许可](https://nsis.sourceforge.io/Docs/AppendixI.html)。工具不提交仓库、不全局安装。程序使用 **zlib 压缩模块**（zlib/libpng 许可），不启用 LZMA 模块；完整原始版权文本位于 packaging/licenses/NSIS-COPYING.txt，随签名 payload 分发。可获取完整上游源码，未修改 NSIS。Inno/WiX 作为未来替代，但不在当前引入第二套安装协议。

正式发布继续要求现有独立 hosted deployment、完整测试、来源和签名门禁；本地提供明确 local candidate 模式，要求完整本机 CTest 及真实部署/安装验收、同一公钥/自签名证书，只生成 local ZIP/Setup，不产生可发布更新 manifest。自签名不等于 Windows 公共发行者信任，不导入 Root/TrustedPublisher，不关闭 TLS。

Windows 目录改名不能持有内部租约句柄：安装器采用同父持久安装锁，启动器/应用/更新器共享持有，安装器独占；确认旧 runtime/job/prepare/launch 均空闲后，释放内部句柄，在外锁保护下提交。日志保留稳定 key、注册表分配的 S000-C8xx 和 Win32 数值，不打印私有路径或账号。

风险：未知/修改的文件保留，可能留下备份目录；应用未成功 RUNNING 前不清理旧程序。机器断电的目录间隙通过受限 journal 恢复。未能取得独立干净 Windows 和多显示器实测时继续标为 UNVERIFIED；本机通过不冒称这些验收通过。
