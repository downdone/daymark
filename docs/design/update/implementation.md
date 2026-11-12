# Windows 更新器实现与操作

实现入口：`src/update/sdk` 纯 C++20 规则，`platform/windows` 的 CNG / WinHTTP / 原子写盘 / ZIP / 锁 / 事务引擎，`cli` 独立控制台，`frontends/win32` 根启动器与原生观察界面。Qt 仅通过 `qt/UpdateController` 进程命令/文件状态和 `StartupUpdateGate` 受控句柄接线，不把原生静态 CRT 对象跨 Qt 动态 CRT 传递。

## SDK 与开发入口

真实公共类型见 `Update.h`；`Outcome.h` 提供 `Outcome<T> / UpdateError / Done / invoke` 标准 C++ 结果边界。当前 `win::Engine` 是同步的安装级协调器，宿主将其放在独立进程执行；内部用 `Failure` 与 RAII 统一释放文件、进程与锁，宿主转换为稳定 JSON 状态/退出码。设计中的异步 `UpdateSession` / Presenter 类是逻辑契约，未额外堆同名转发类；事件 `Notify` 为不可变 JSON 值，Qt 用目录通知，GUI窗口存续时250ms观察持久状态。

原生平台实现限定在更新子系统自身的platform/windows，独立于Qt窗口平台端口；CLI和Win32宿主不将原生操作泄漏到QML或任务服务。

Windows 沿既有 build-windows.ps1 / test-windows.ps1；CMake增加原生目标 `daymark-bootstrap`（输出 daymark-launcher.exe）、`daymark-updater`、`daymark-update-cli`。原生目标静态 CRT，不依赖 Qt DLL。主程序版本保持仓库2.0.0，测试的2.0.1仅 `BUILD_TESTING=ON` 的隔离构建选项 `DAYMARK_UPDATE_FIXTURE_APP_VERSION`，不是正式版本/tag。

```powershell
# 根目录已经是带可信种子签名的托管安装。
.\daymark.exe                         # 固定入口：恢复事务/应用Ready/启动当前版
.\daymark-update-cli.exe status --json
.\daymark-update-cli.exe prepare --json
.\daymark-update-cli.exe cancel --json
.\daymark-update-cli.exe cancel-ready --json
.\daymark-update-cli.exe auto --enabled false --json
.\daymark-update-cli.exe apply --json # 活动Qt应用先走保存退出门
.\daymark-update-cli.exe recover --json
.\daymark-updater.exe                  # 用户主动查看薄UI
```

`--root`接受用户选择的托管目录，仍检查本地固定磁盘NTFS、非UNC/非重解析点、签名和租约。路径/进程参数不是远端命令。退出码：0完成或已受理/暂缓；2参数；10网络/资源未就绪；11限流；20验证；21不支持/不兼容；30实例或退出；31文件/空间；40启动健康；41恢复/内部；50取消。JSONL事件最后 `result` 表明结果，不用进程已启动冒充安装成功。

作者工具要求Python3.13，Windows工作流固定解释器版本。

macOS/Linux本期不增加构建/部署工作；SDK核心可独立移植，实际下载、安装、CLI与GUI端口均保持未支持。Windows自动安装只接受完整自包含运行库 `runtime_min: bundled`；Qt真实预检验证加载能力，不自动安装VC运行库。首次引导、引导层自身更新、schema/读写ABI变化均手动安装。

## 提交与故障恢复

2026-11-06功能收尾：用户取消候选只抑制24小时自动重下，明确手动操作可重试（仍遵守服务端限流，自动检查至少间隔24小时）；缓存验证/解压时取消与下载取消使用同一策略，下载网络失败亦持久化一小时自动退避。Qt适配器有界读取状态、损坏时不声称Ready，启动失败可见且临时QProcess释放；按候选事务通知且初始Ready留给连接完成后的订阅者。关于页固定项目发布说明入口也可用于非托管安装，原生SDK仍独占签名/安装资格裁决。

下载流式写 part + ETag/散列/字节收据；206严格验证区间/总量，200清空重下，416只认可完整散列匹配。元数据ETag/304缓存有界；遵守Retry-After/RateLimit-Reset，自动检查有启动冷却，网络失败不显示已最新。ZIP先预检完整索引与相对路径，再独立staging解包，只有签名索引相同的遗留候选可以复用；不覆盖未知目录。

预检是隐藏窗口下的QML/插件/合成SQLite资源检查，**不等待不可见窗口的DWM帧**。实际候选受限启动仍要求真实首帧、只读schema/完整性检查；提交前不正常打开用户DB、不备份迁移、不启动同步/提醒。current持久提交后才允许生产初始化，正常RUNNING与磁盘收据成立后才删旧版。COMMIT回复丢失由匹配事务/代数的current确认；worker提交后中断由新应用正常RUNNING收据补记恢复。清理逐个只删除原签名索引中散列未改的文件，未知/修改文件保留，占用延期；清理开始不可回退旧版。原生元数据读取允许删除共享；原子替换仅对短暂共享/非只读访问冲突最多250ms，不延长安装健康预算，持续占用返回原生错误码并保留旧完整文件。Qt读取在通知UI前关闭文件句柄。整个提交前启动区受恢复保护，恢复旧程序前释放launch锁。

独立CLI/GUI不替代保存门，用户取消保存不安排更新重启。主程序普通退出会取消自己的准备任务；apply独立完成切换。开机启动的显式启用指向固定根启动器，资料与原有用户目录不搬入versions。

## 签名和正式资产

正式密钥不由默认构建自动生成。负责人离线运行 `daymark-update-cli.exe keygen <安全目录>`，独立备份 private.json；public-keys.json通过 `DAYMARK_UPDATE_PUBLIC_KEYS` 编入三个根宿主，源码可公开公钥。无公钥构建显示未配置，不能仅散列安装。测试key/loopback配置仅在测试目标启用，生产目标不读update-fixture.json、不允许注入URL或信任绕过。

`package-update.py` 输入已验证的完整payload/三个根宿主，生成索引、内嵌种子签名、托管ZIP/散列及外部draft。生产要求BUILD-INFO精确版本/源SHA和fullTestGatePassed/cleanMachine/signed；不会自动把未签名标签产物升级成可自动安装资格。`finalize-update.py` 在ZIP上传后验证真实GitHubrelease/asset ID、repo/tag下载URL、大小/digest，再生成正式清单和签名；禁止覆盖已有资产。

受保护的 `.github/workflows/windows-update.yml` 消费成功同源tag构建candidate，要求Environment批准、真实Authenticode有效、全部门禁，再调用 `publish-windows-update.py` 上传ZIP后最后发布清单与签名。默认标签构建只产生未签名candidate，正式门禁因此会拒绝；需先完成代码签名与干净机验收接入，不伪造这些标志。远端运行和公开升级需另授权并有正式资产验证。
