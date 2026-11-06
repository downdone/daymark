# 本地官方服务配置

本目录另有两个可公开的更新配置：`update-public-keys.json`是客户端内置的P-256公钥，`authenticode-policy.json`是经审查的自签名代码证书SHA256指纹和明确的非公共信任策略。它们不包含私钥或证书密码，与下面仅本地保存的服务URL配置分别管理；CMake的`DAYMARK_UPDATE_PUBLIC_KEYS`可指定另一份公钥文件。私钥、PFX、DPAPI封存文件禁止提交。签名与验证流程见[自签名说明](../docs/design/update/self-signing.md)。

本地签名封存目录为`config/signing.local`，由Git忽略，禁止强制提交或随清理build目录删除。目录限定当前用户/SYSTEM访问，私钥、PFX和密码以CurrentUser DPAPI封存；后续签名复用已有密钥，不重新初始化。DPAPI不能直接跨机恢复，受保护CI Secrets与恢复安排见自签名说明。

将official-service.example.txt复制为official-service.local.txt，文件仅保留一行HTTPS源地址，不含账号、口令、路径、查询或片段。local文件由Git忽略，禁止强制提交；公开源码只提供示例，示例不会自动成为官方登录的目的地。

CMake配置时读取本地文件并生成构建目录中的OfficialServiceConfig.h，地址编入程序；运行时不能在官方登录中编辑。缺少本地文件的源码/CI构建仍可编译，官方入口明确提示未配置，自定义URL保持可用。地址会包含在本地编译程序中，此机制控制配置来源，不是对二进制地址的加密。

Windows使用`Copy-Item config/official-service.example.txt config/official-service.local.txt`后编辑；macOS使用`cp config/official-service.example.txt config/official-service.local.txt`。再运行对应build-windows.ps1或build-macos.sh。独立构建工作树可通过CMake的DAYMARK_OFFICIAL_CONFIG文件路径参数读取原本地文件，不通过命令行传实际URL。用户已授权正式Windows CI从受保护Environment的`DAYMARK_OFFICIAL_SERVICE_URL` Secret生成临时配置，保留至构建/测试/打包结束后清除；源码仍仅提供示例，未配置的普通源码构建不获得正式地址。
