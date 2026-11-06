# ADR-033：GitHub 资源与 Windows 便携在线更新

状态：**2026-11-05已采用并进入Windows实现/验证；正式签名发布与其他平台未验证**。总体见[设计](../design/windows-online-update.md)，功能/选型/架构/SDK接口/三端轻量UI详见[设计包](../design/update/README.md)。

## 背景

用户要求设计在线更新，利用 GitHub 安装资源，优先 Windows。现有资源契约是完整便携 ZIP，源码 Release 与二进制资源分时产生；本地 MinGW/正式标签 MSVC 构建不能按名称盲目互换。直接覆盖运行中的 EXE/Qt DLL，以及把普通 SHA256 校验当发布者认证，都不足以满足可靠交付。

## 建议决策

1. GitHub公开Releases已有版本查询/资源托管/下载能力，无需另外开发服务端。发布工具生成签名更新清单并作为Release附件，不复用账号认证、不收集任务或设备身份。按用户修正默认启动检查，发现新版自动启动独立更新器后台下载/校验/旁路安装；完成后提醒用户重启，下一次正常启动也应用已准备版本。
2. Windows x64 当前用户可写的本地 NTFS 目录先行；采用稳定原生启动器、版本目录、原子指向/事务日志与版本化 worker，不逐文件覆盖正在使用的程序。已有客户端手动安装一次引导版本；首期启动器自身升级需要手动引导包。
3. 清单签名采用 ECDSA P-256/SHA256，Windows 使用系统 CNG；私钥独立受控、公钥内置。清单签名与 Authenticode、二进制发布门禁分开，不宣称签名清单能消除 SmartScreen。
4. 优先 Qt 6.10.2 公共 API。Qt 无公开、合适的 ZIP 解压接口，建议 vendoring miniz **3.1.2**，只编译必要 ZIP/deflate 读取代码。上游 [Release](https://github.com/richgel999/miniz/releases/tag/3.1.2)、[MIT 许可](https://github.com/richgel999/miniz/blob/3.1.2/LICENSE)；实施时固定完整源码 commit、分发散列、许可与 SBOM，审核变更/漏洞并做畸形包测试，本提案不等于依赖已审核通过。
5. 首期仅允许同 schema、同数据读写兼容 ID 的自动安装与提交前二进制回退。受限启动到提交前冻结全部写源；数据格式迁移走明确手动升级，不自动覆盖用户数据库。
6. 用户明确底层SDK/CLI并讨论原生轻量UI：采用Qt-free C++20 SDK静态库、Win32薄GUI与独立console CLI；主程序Qt适配状态，元数据和下载均由原生WinHTTP处理。后续macOS AppKit、Linux可选GTK3.24共用SDK/Presenter，缺GUI可CLI；平台安装端口分阶段独立验证，不承诺Linux零GUI运行库依赖。不同时维护独立Qt更新器GUI。根引导层自身升级首期手动处理。
7. 原生严格JSON采用nlohmann/json3.12.0（MIT）有界解析回调，实施时锁完整源码commit/散列/许可证/SBOM并审计；不是Qt JSON默认解析，也不手写通用解析器。见[官方版本](https://github.com/nlohmann/json/releases/tag/v3.12.0)与[许可](https://json.nlohmann.me/home/license/)。核心标准类型/Outcome，Qt仅边界转换；原生持久化不依赖QSaveFile。
8. 现有发布标签/同名资源不可变规则继续有效；增加资源需要同步发布工具白名单/测试。清单+签名两者有效才就绪，上传竞态不会变成半包安装。正式更新资源不能降低现有Qt许可、完整测试、签名和干净机门槛。持久化提交后不可自动退旧，RUNNING成功才具备清理资格；清理开始前落盘不可回退标记。

## 替代方案

| 方案 | 不采用的理由 |
|---|---|
| 下载 ZIP 后 PowerShell/批处理覆盖 DLL | 运行锁、参数/提取安全、进程退出、半成功恢复难以统一测试，依赖外部运行环境 |
| Qt 私有 QZipReader | 私有 ABI/API 不稳定，不符合固定公共工具链与可维护性要求 |
| libarchive / 7-Zip 全量运行时 | 支持格式远多于当前需要，增加分发/攻击面；miniz 更贴合已锁定的 ZIP 范围 |
| 首期接入 MSI/MSIX/第三方安装更新框架 | 当前发行是便携包，安装身份/权限/签名分发体系尚未建立，转换成本超出本次目标 |
| 仅 HTTPS + 同站 SHA256 | 可检查损坏，无法在托管发布权限受损时独立验证发布者 |
| 自建更新 API/CDN / 账号服务转发 | 目前无必要，增加运维/隐私/故障依赖；GitHub 网络失败可安全回到手动下载 |
| 同时做差分包与跨平台自动安装 | 扩大恢复/签名/平台矩阵，首期完整包约数十 MB，可接受 |

## 后果与批准条件

后台准备至新版本健康确认/提交期间临时保留新旧两版，成功后删除旧程序；删除失败延后、不强杀或影响新版。需要额外完整新版空间、native bootstrap/updater、ZIP读取与受限启动写门。首个引导版本须递增公开版本、统一运行时、实测Windows/无SDK/干净机和真实GitHub资源/崩溃恢复；macOS安装端口未实现。

用户随后授权完整实现并验证Windows全流程，依赖已锁定源码/许可/散列；本期只实施Windows流水线。实际证据见PROGRESS，新的外部发布仍按明确授权与既有门禁执行。

2026-11-06用户选择“先自己签名，自己验证”：本阶段增加显式`self-signed-pinned`发行策略。P-256更新签名维持必需，EXE使用独立RSA自签名代码证书；验证同时要求独立SHA256证书指纹、WinVerifyTrust文件完整性及仅进程内CustomRootTrust的有效期/代码签名EKU检查。不导入Root/TrustedPublisher、不取消TLS验证；`signed`旧公共签名字段保持false，单列`authenticode.policy/publicTrust=false`及文件散列。该授权解决代码证书来源，不豁免完整测试、真实GitHub和独立Windows部署验证；不得将自签名描述为Windows公开可信发布者。
