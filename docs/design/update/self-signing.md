# 自有密钥签名与固定指纹验证

2026-11-06用户明确选择“先自己签名，自己验证”。这是ADR-033的显式自签名策略，不代表Windows公共发布者信任；当前结果与GitHub进度只维护于[PROGRESS](../../PROGRESS.md)。本阶段仅Windows。

## 两套密钥与存储

- 更新清单/引导描述：Windows CNG生成ECDSA P-256私钥，公开key_id/XY进入`config/update-public-keys.json`，默认编入生产宿主；测试夹具不成为生产信任。没有私钥不能生成有效更新，只有SHA256不能代替签名。
- EXE：独立RSA3072/SHA256代码签名自签名证书，只安装在CurrentUser/My，未加入Root或TrustedPublisher。`config/authenticode-policy.json`固定证书SHA256指纹，不接收候选包自己提供的信任锚。
- `Initialize-UpdateSigning.ps1`拒绝覆盖已有目录；专用目录去掉继承，仅当前用户和SYSTEM可访问。P-256私钥、加密PFX和PFX密码再以CurrentUser DPAPI分别封存；短时明文只在受限操作目录中，finally删除。脚本不输出私钥、PFX或密码。DPAPI绑定用户/机器，直接复制封存文件不等于跨机恢复；正式启用仍须受控备份/Secrets与恢复安排。

## 签名和验签

`Sign-WindowsCandidate.ps1`对隔离候选的四个程序签名：payload/daymark.exe与native下的launcher、CLI、updater。签名前拒绝缺文件/已有签名，使用当前用户个人库中的明确指纹，不通过命令行传私钥密码。无远端时间戳时不宣称过期后仍获可信时间戳保护。

`Authenticode.psm1`先用WinVerifyTrust校验实际文件，只允许成功或精确的`CERT_E_UNTRUSTEDROOT (0x800b0109)`；坏摘要、缺签名、过期或其他平台错误均拒绝。随后核对嵌入证书的SHA256固定指纹、自签名关系，并在仅进程内的.NET CustomRootTrust中验证有效期和CodeSigning EKU，不下载证书、不修改系统根信任。实际修改PE代码段必须得到`TRUST_E_BAD_DIGEST (0x80096010)`。

BUILD-INFO的旧` signed `公共签名字段维持false，新增`authenticode.policy=self-signed-pinned/publicTrust=false/certificateSha256/files`记录四个实际文件的SHA256。正式包只有在外部明确指定相同证书指纹时接受该策略，重新验证实际四个EXE；完整测试和独立Windows部署门禁继续必需。没有指纹不能从自签名自动降级到不验签。GitHub受保护发布读取已审查主分支配置，不从候选的自报指纹建立信任。

## 可重复命令

Windows PowerShell7，工具路径通过参数传入；命令不包含私钥值或服务URL：

```powershell
./scripts/Initialize-UpdateSigning.ps1 -Directory build/private-signing -UpdateCli build/update-dev/bin/daymark-update-cli.exe
./scripts/Sign-WindowsCandidate.ps1 -Directory <隔离候选> -CertificateThumbprint <个人库指纹> -CertificateSha256 <已审查SHA256> -SignTool <WindowsSDK的signtool.exe>
./tests/scripts/SelfSigningTest.ps1 -CandidateDirectory <隔离候选> -UnsignedExecutable <未签名CLI> -CertificateSha256 <已审查SHA256>
./tests/scripts/UpdateSigningTest.ps1 -WorkDirectory <新的证据目录> -SigningDirectory build/private-signing -CandidateDirectory <隔离候选> -RuntimeDirectory <已部署Qt运行目录>
python -m unittest discover -s tests -p test_update_publication.py -v
```

第一条只执行一次，后续使用已生成密钥，不覆盖/换key。`SelfSigningTest`验证四文件、错误pin、未签名、真实PE篡改和根库前后不变；`UpdateSigningTest`用自有P-256签名本地验证包，以生产CLI内置公钥初始化，并验证改清单/错域/错key/改payload拒绝；运行完整Qt运行库的`--update-preflight`只接临时合成数据。该包明确local-fixture、合成Release ID，不作为正式公开包，不冒充公网GitHub升级或干净机。

macOS/Linux本轮按用户指示跳过，不运行这些Windows证书脚本，也不声称已移植。官方机制：[WinVerifyTrust](https://learn.microsoft.com/en-us/windows/win32/api/wintrust/nf-wintrust-winverifytrust)、[CustomTrustStore](https://learn.microsoft.com/en-us/dotnet/api/system.security.cryptography.x509certificates.x509chainpolicy.customtruststore)。
