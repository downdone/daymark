# GitHub 发布前置准备与启用步骤

2026-11-05。此文是准备契约，不表示仓库Environment/Secrets或Release资产已经配置。客户端只读公开资源，GitHub已经提供查询、托管、下载能力，无需新服务器程序。依据[Release API](https://docs.github.com/en/rest/releases/releases)、[资产API](https://docs.github.com/en/rest/releases/assets)与[Environment保护](https://docs.github.com/en/actions/how-tos/deploy/configure-and-manage-deployments/manage-environments)。

## 1. 仓库与权限

- `downdone/daymark`需保持更新资源公开可匿名读取；客户端不捆绑PAT，也不要求用户登录GitHub。若改为私有分发，应另设计认证，不将发布凭据交给用户。
- 发行版使用独立递增稳定SemVer、annotated `vX.Y.Z`与固定源SHA。源代码Release存在不等于Windows更新包已就绪。
- 普通PR构建只需`contents: read`，不能接触签名私钥。上传最终资源的受控发布job才授予短期`GITHUB_TOKEN`的`contents: write`；不申请仓库管理员或长期PAT权限。
- Environment建议命名`windows-update-release`，限制可信分支/tag；可用时设required reviewers与禁止自批。具体保护能力依仓库类型/计划而异，不假定配置文件能代替GitHub设置。

## 2. 信任公钥先于第一版客户端

第一次支持更新的引导包必须内置发布公钥。私钥只由项目负责人控制的离线签名环境或受保护Environment使用；不能把私钥JSON/PEM放进源码、ZIP、日志、Actions普通artifact或仓库变量。

建议准备项：

| 名称 | 类型/位置 | 用途 |
|---|---|---|
| 更新公钥配置 | 源码中的公开key_id/XY或受控编译输入 | 客户端验证更新签名；公钥可以公开 |
| `DAYMARK_UPDATE_PUBLIC_KEYS_JSON` | 仓库公开variable，keys数组JSON | release-build编译根宿主信任；不能放private_blob |
| `DAYMARK_MANAGED_WINDOWS_RELEASE` | 仓库variable，默认不启用 | 设true时旧flat包自动上传暂停，改由受保护Windows更新发布；避免ZIP同名冲突 |
| `DAYMARK_UPDATE_SIGNING_KEY` | Environment secret或离线签名输入 | P-256私钥，只有已审查发布作业可读取 |
| `DAYMARK_UPDATE_KEY_ID` | 受控配置，与客户端一致 | 选择已内置公钥，不构成密钥本身 |
| 正式官方服务地址配置 | 独立受控编译输入 | 沿现有账号构建配置；不在更新清单夹带本地文件 |

没有公钥/私钥配置的开发包应明确显示“当前构建未配置更新信任”，不得退成仅SHA校验自动安装。测试签名只在本地临时夹具中使用，不进入正式信任集合。私钥要有离线备份和轮换责任人；撤销/泄漏时需新的可信引导通路，不能宣称失陷私钥自己签一份撤销就解决信任。

## 3. 每个Windows可更新Release的资源

```text
Daymark-X.Y.Z-windows-x64.zip
Daymark-X.Y.Z-windows-x64.zip.sha256
SHA256SUMS.txt
update-windows-x64.json
update-windows-x64.json.sig
```

先构建/验证托管布局、索引与内嵌种子签名，再上传ZIP和校验文件；读取GitHub实际releaseId/assetId/字节数/digest后生成外部清单并签名，最后上传清单和签名。客户端仅在两个清单文件均完整且验签、资源身份和兼容条件都满足时发现可更新资格。不能签一份含占位ID/源SHA/运行库版本或未验证门禁的清单。

已公开tag和同名资产不覆盖；资源不一致发布失败，修正使用下个版本。发布工具的现有资产白名单、恢复流程和测试必须认识这两个新增文件；保留既有CI恢复与来源校验改动，再按用户追加要求增量接入Windows，不覆盖他人工作。

用户随后明确只优先Windows：默认release-build矩阵现为windows-2022/MSVC2022，保留历史已验证Unix资产的恢复校验代码，不新增Unix构建工作。普通标签流水线产出`windows-update-candidate`，另有手动受保护`windows-update.yml`负责正式发布。已有普通包与托管包ZIP同名，因此启用前必须设置`DAYMARK_MANAGED_WINDOWS_RELEASE=true`，让普通附件上传job退出；不能在同一tag覆盖旧包来切换布局。

初始准备阶段只实现工具与工作流，不代表远端已配置；现行配置和发行结果只见PROGRESS。普通候选必须声明未签名/未独立部署验证，受保护流水线按明确的公共签名或self-signed-pinned策略真实校验、独立验证后才发布，不允许只修改布尔值绕过。候选run必须对应相同tag源码SHA、成功push构建和固定workflow；恢复工具分支的不同SHA产物不自动提升为正式候选。

2026-11-06用户已授权实际启用与发布，现行队列见PROGRESS的UPD-06。签名接入分为两件事：P-256更新清单密钥可由项目受控生成；Windows Authenticode需要公开可信的代码签名证书/服务，网站TLS证书不能代替。现有证书须明确签发机构、令牌/HSM/云服务或既有可导出PFX形式；私钥、密码不经聊天或源码传递。新申请须先核对签发机构的身份和硬件保护要求；自签名不能冒充公开Windows信任，清单签名也不能承诺消除SmartScreen。若选择暂不使用Authenticode，须明确变更发布策略、保留真实未签名状态，不能把signed字段改成true。

MSVC候选的`runtime_min: bundled`按app-local部署实现：`windeployqt --no-compiler-runtime`，再由`MsvcRuntime.psm1`从VS2022的`VC/Redist/MSVC/<version>/x64/Microsoft.VC143.CRT`选取最新版；拒绝开发bin、System32、重解析目录和未知文件，校验每个微软Authenticode签名、14.3x/14.4x版本和PE x64架构，复制后核对SHA256。BUILD-INFO记录文件名/版本/散列而不记录本地路径。原生宿主仍静态CRT；不在客户端自动安装vc_redist。分发权依Visual Studio许可及其REDIST列表，不从开发机任意DLL推定。依据[Microsoft可分发文件说明](https://learn.microsoft.com/en-us/cpp/windows/redistributing-visual-cpp-files?view=msvc-170)。独立Windows验证仍必须实际运行，文件复制成功不算干净机通过。

## 4. 第一版启用顺序

用户2026-11-06随后明确选择自签名，采用[自签名契约](self-signing.md)：自有P-256更新信任和独立RSA自签名EXE，源码只含公钥/证书指纹，私有材料DPAPI封存。受保护发布从已审查配置选择`self-signed-pinned`并校验四个真实文件；BUILD-INFO的公共` signed `仍false，`authenticode.publicTrust=false`明确。该显式策略不向系统导入根证书伪造Valid，独立Windows部署/完整测试门禁保留；GitHub仍须安全配置更新私钥与代码签名材料，不能把仅有公钥当可签名候选。

1. 确定正式发布密钥/公钥与保管方式，先把公钥编入第一版引导层和客户端。
2. 用独立测试密钥在隔离Windows完成N→N+1下载/旁路安装/保存退出/受限启动/提交/RUNNING/删旧版及故障恢复。
3. 完成正式完整CTest/lint、无SDK、目标Windows/VC运行库、签名/干净机与许可证门禁，构建第一版正式引导ZIP。没有更新器的旧版仍需用户手动安装这一次。
4. 在GitHub实际配置Environment/Secrets/受保护发布权限，将发布工具接入已有流程；这一步是仓库配置，不是部署后端。不能在本地测试结果里写成远端配置完成。
5. 用户明确授权发布后创建独立版本/tag并上传资源，匿名GET复核完整性；再以第二个递增版本验证真实GitHub升级。

本地夹具可验证完整Windows机制；真实GitHub端到端需要至少一份新发布资产和对应受信客户端。未获发布授权/正式签名配置时保留这一外部边界，不为了证明更新而自动上传测试资产、移动tag或泄露私钥。

## 5. 维护与失效处理

限流/网络不通显示可操作状态而非“已最新”。签名私钥不长期驻留普通runner，使用后清除临时密钥文件；日志仅记录公钥ID、源SHA、资产名/散列和门禁结果。签名轮换先让旧客户端获得经旧信任授权的新公钥，再切换发布者；引导协议过旧或密钥失陷走明确手动信任恢复。

## 固定源码的受控重建

2026-11-06发行工具恢复契约：`release-build.yml`允许在main上workflow_dispatch指定既有annotated稳定tag，源码继续checkout该tag的精确SHA，工具链/工作流来自已审查main。两个SHA分别写入payload/BUILD-PROVENANCE.json，连同runId、event、触发ref、仓库、tag及官方入口配置证据。发布者从GitHub API独立确认成功run、固定workflow/仓库、主分支工具SHA可达性及其包含源码提交，再校对artifact中的全部身份；不能把main工具SHA冒充源码SHA。旧的任意跨run恢复资产仍不能自动提升；新入口是显式完整重建，全部CTest/lint/签名/独立部署继续必需。预检-only不会生成候选artifact。该方式修复配置生命周期/打包工具，不移动公开tag，不覆盖已上传资产。

官方配置输入被CMAKE_CONFIGURE_DEPENDS追踪，必须保留至所有可能触发CMake重配的构建、lint和打包结束，再always清理。收集候选及受保护签名阶段都验证实际EXE含有规范化官方地址，不在日志输出地址；正式包缺失官方配置会被拒绝。
