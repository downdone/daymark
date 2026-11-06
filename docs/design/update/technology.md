# 技术选型确认

工程路线已落实，依赖固定版本/commit/散列见third_party/update/SOURCES.json；实际验收统一见PROGRESS。C++20/Qt6.10.2为既有固定基线；Windows优先，不为跨平台预建通用安装框架。

## 1. 选型表

| 领域 | 采用 | 理由/边界 |
|---|---|---|
| 发行托管 | GitHub Releases REST/资产下载 | 无需服务端开发；只读匿名公开资源，不把PAT嵌入客户端 |
| API版本 | `X-GitHub-Api-Version: 2026-03-10` | 当前官方文档支持；本次固定目标，集成时验证响应，不跟随latest文档自动改协议 |
| 主应用 | C++20 + Qt6.10.2 Network/QML | 只做状态、协调和UI；不承担重型解包 |
| 更新核心 | C++20、标准库类型和显式结果 | Qt-free静态库，供原生exe与Qt适配器复用规则 |
| SDK/原生宿主 | Qt-free C++20 SDK静态链接；根启动器、Win32 GUI宿主和console CLI宿主，静态CRT | `daymark.exe`固定入口、`daymark-updater.exe`薄UI、`daymark-update-cli.exe`可独立执行；不依赖Qt或VC动态CRT |
| 三端轻量UI | Windows Win32、macOS AppKit/Objective-C++、Linux可选GTK3.24 | 共享SDK/Presenter，GUI只改控件与主线程投递；无GUI时CLI可用，安装平台端口仍分阶段验证 |
| 网络 | 更新全程原生WinHTTP；账号同步沿QNetworkAccessManager | 原生进程独立运行；共享请求/重定向/限额测试，不要求两套传输实现共享Qt类型 |
| 代理 | Windows当前用户手动代理/PAC，经WinHTTP显式解析应用 | 不自动继承Qt代理；失败可诊断，不绕过指定代理盲目直连 |
| 签名/散列/随机数 | Windows CNG：ECDSA P-256/SHA256/系统随机 | 公钥内置；P1363固定64字节；不引入OpenSSL运行库 |
| ZIP | vendored miniz3.1.2，MIT，只编译所需读取/解压部分 | 不执行PowerShell/7-Zip，不使用Qt私有QZipReader；锁commit/源码散列并审计 |
| JSON | vendored nlohmann/json3.12.0，MIT；有界DOM解析回调 | 解决原生无Qt的解析需求，不手写JSON解析器；回调跟踪每层key以拒绝重复字段 |
| IPC | 本机Windows Named Pipe，显式当前用户DACL与远程拒绝 | 校验peer PID/创建时间/映像；nonce由继承通道传递，不放通用命令行 |
| 文件互斥 | 原生文件句柄 + LockFileEx shared/exclusive | 按安装物理根分离prepare、launch gate、runtime租约；进程退出释放 |
| 状态写入 | 原生临时文件+FlushFileBuffers+同卷安全替换/校验备份 | 原生端不调用QSaveFile；Qt端业务设置仍沿用现有写法 |
| 数据预检 | 新版本Qt SQL独立只读启动路径 | 禁止复用自动迁移/备份/WAL初始化的正常open |
| 安装格式 | 整包portable-managed-v1、独立版本目录 | 下载成本可接受，避免混DLL；差分/MSI/MSIX不在首期 |
| 测试 | Qt Test用于核心/适配；原生子进程夹具用于锁/文件/进程 | Qt测试依赖不进入产品updater；真实Windows、无SDK/干净机另设门禁 |

Qt-free层不包含QString/QUuid/QDateTime/QObject、现有任务Result或Qt JSON。跨边界采用UTF-8 DTO和标准结果，Qt适配器再转为既有`daymark::Result<T>`及属性；Windows路径仅平台端转换为UTF-16。共享代码不重复实现SemVer/签名输入/清单字段/状态规则。

## 2. JSON与资源上限

严格UTF-8、无BOM、无注释/尾逗号/NaN/Infinity、最大深度16。清单64KiB、签名封套4KiB、单IPC消息64KiB、普通状态文件128KiB、文件索引8MiB。解析回调校验重复key/深度与整数类型，输入字节预先有界；解析失败不输出半可信对象。

大数字GitHub ID使用十进制字符串；size/count用有界无符号整数；不接受浮点、科学计数表示身份/计数或负数。SemVer三段uint32，无前导零；超出实现能力返回UnsupportedVersion，不截断。format未知或必需能力缺失拒绝；允许不影响语义的未知可选字段，遇到`required_capabilities`未知项则ManualRequired。

每个HTTP JSON页上限2MiB，最多3页/90秒/12次元数据请求；每页嵌套资产若有分页/截断须按请求预算补齐，否则DiscoveryIncomplete，不能把缺资产当确定最新。首期签名清单同架构每Release只接受一个。

## 3. 发布/信任选型

签名域严格分离：`Daymark.UpdateManifest.v1`与`Daymark.BootstrapPayload.v1`，各加NUL和原始UTF-8字节再SHA256。内置公钥map按key_id选择；签名封套本身有界严格解析，只用于选择已内置key/固定算法。签名失败不读清单的安装指令。

根启动器/更新器协议首期固定1，不自更新。当前官方标签沿MSVC2022路线，首个正式引导包目标runtime_family为msvc143；自动安装的Qt payload要求自包含bundled运行库，缺失走手动安装，根引导层静态CRT。MinGW本地包不在线跨轨；依旧先手动安装统一引导包。签名私钥与真实服务URL配置只走受控发布环境，不进入源码/文档/ZIP；清单签名不替代Authenticode。

新依赖只在更新子系统目标使用，保留上游许可/来源、完整commit、散列、裁剪说明与SBOM。3.1.2/3.12.0是选型版本，不等于安全审核已通过；实施前查实际供应链/CVE和编译器兼容，必要升级经ADR记录后进行。依赖不运行构建时下载，不把“最新版”作为FetchContent浮动目标。

用户补充SDK/CLI/三端轻量UI要求后，确认首期采用SDK+Win32原生薄UI+CLI，主应用继续Qt适配；不同时实现另一套独立Qt更新器窗口。macOS AppKit使用系统框架；Linux GTK为可选GUI依赖，SDK/CLI不链接GTK，具体分发依赖许可/SBOM在Linux实施前冻结。不能承诺Linux无外部GUI库、也不能把三端UI可做推导为安装后端已完成，详见[UI契约](native-ui.md)。

## 4. 官方依据

- [GitHub API版本](https://docs.github.com/en/rest/about-the-rest-api/api-versions)、[Release资产下载](https://docs.github.com/en/rest/releases/assets#get-a-release-asset)：API版本可固定，公开资产可匿名下载并返回直接流或重定向；正式路径必须实测。
- [WinHTTP](https://learn.microsoft.com/en-us/windows/win32/winhttp/about-winhttp)：原生HTTP/TLS；[自动代理注意项](https://learn.microsoft.com/en-us/windows/win32/winhttp/autoproxy-issues-in-winhttp)说明代理路由须处理，不能假定继承浏览器状态。
- [LockFileEx](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex)：共享/独占字节范围锁；[管道安全](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights)：必须显式ACL，不能依赖默认描述符。
- [nlohmann/json3.12.0](https://github.com/nlohmann/json/releases/tag/v3.12.0)、[SAX接口](https://json.nlohmann.me/api/json_sax/)、[MIT许可](https://json.nlohmann.me/home/license/)；[miniz3.1.2及许可](https://github.com/richgel999/miniz/releases/tag/3.1.2)。

2026-11-05查阅。官方说明支持API/依赖能力，不构成本项目已经通过平台、代理或安全测试的证据。
