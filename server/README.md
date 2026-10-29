# Daymark 账号与同步服务

可选的 Go 单体 + SQLite 服务。桌面未登录时只操作本地库；登录后仍先写本地，再通过持久化队列同步。HTTP/JSON v1 使用[唯一 OpenAPI 契约](../docs/api/sync-v1.openapi.json)，13 种业务命令与领域层独立，当前不引入 gRPC。实现及验证状态见[进度](../docs/PROGRESS.md)。

## 构建与测试

需要 Go 1.27.0；SQLite 驱动不需要 CGo。以下命令在 `server` 目录执行。

```sh
go mod verify
go vet ./...
go test ./... -count=1 -timeout=5m
go build -trimpath -o ../build/daymark-server ./cmd/daymark-server
```

Windows 输出文件改为 `../build/daymark-server.exe`。race 检查使用 `go test -race ./... -count=1 -timeout=5m`，需要对应平台的 C 编译器。全部自动测试使用临时目录与合成账号；普通 Go 测试缺少 Qt 程序时明确跳过桌面联调，不代表联调通过。

先构建桌面的 `test_task_workspace`，再将其绝对路径放入 `DAYMARK_QT_SYNC_TEST_EXE`，执行：

```sh
go test ./internal/httpapi -run '^TestQtDesktopEndToEnd$' -v -count=1 -timeout=5m
```

Windows PowerShell 使用 `$env:DAYMARK_QT_SYNC_TEST_EXE=...`，并将 Qt Kit 的 `bin` 与对应编译器运行库加入 PATH；macOS 使用 `export DAYMARK_QT_SYNC_TEST_EXE=...`。当前原生凭据联调证据来自 Windows；macOS Keychain 实机仍需验证。服务端 CI 覆盖 Windows/macOS/Linux；桌面双端联调 CI 使用 Windows。

仓库根目录运行 `python scripts/generate-sync-contract.py --check` 检查服务端嵌入契约与 Qt Schema 副本；不带 `--check` 重新生成。`python scripts/collect-server-licenses.py --check` 检查所选模块版本、校验和及许可证文本，依赖升级时重新生成并审查 [licenses/modules.json](licenses/modules.json)。

## 启动与账号

复制 `config.example.json` 为被忽略的 `config.local.json`，设置自己的数据目录与 HTTPS 公共源。默认仅监听 loopback，由反向代理终止 TLS；也可设置 `tls_cert`/`tls_key` 由服务直接提供 TLS。非 loopback 监听必须配置 TLS；开发模式只允许 loopback。桌面生产连接始终要求 HTTPS，不忽略证书错误。

使用专用系统服务账号，数据目录及配置仅授予该账号和管理员访问。不得将 `var`、密钥、证书、SQLite/WAL、备份或配置提交仓库。相同目录只允许一个进程；管理员命令也需先停服务。默认关闭自助注册；示例配置显式设置 `registration_enabled:true`，部署者可改为false。开启后可从桌面“通用→账号与同步→登录账号→没有账号？注册”创建账号并同步，也可使用下列管理员命令建号。注册账号只是登录名，不表示已验证邮箱。

注册接口 `POST /v1/auth/register` 接收 `protocol_version:1`、`login`、`password`，返回204后独立登录。密码至少15个Unicode字符且最多1024 UTF-8字节；已有账号也返回204，不修改原口令或状态。注册请求在KDF前按socket IP每分钟5次、全进程每分钟20次限制，不信任X-Forwarded-For；同一反向代理后的客户端共享该IP额度。关闭返回404，客户端显示联系管理员。含@的账号只是用户名，不用于邮件找回。丢失注册响应时先用原输入登录确认；不会自动反复建号。旧客户端继续登录同步，无需理解新增入口。

Windows PowerShell 7（口令走 stdin，不写 argv 或历史）：

```powershell
$secret = Read-Host '新账号口令' -MaskInput
try { $secret | & ../build/daymark-server.exe -config config.local.json -create-user 'owner@example.com' }
finally { Remove-Variable secret }
& ../build/daymark-server.exe -config config.local.json
```

macOS/Linux 的 bash：

```bash
read -r -s -p '新账号口令: ' secret
printf '%s\n' "$secret" | ../build/daymark-server -config config.local.json -create-user 'owner@example.com'
unset secret
../build/daymark-server -config config.local.json
```

忘记口令时停服务，按同样 stdin 方式使用 `-reset-password`。重置会撤销旧会话，不恢复已删除账号。正常服务支持 SIGINT/SIGTERM 有界退出；应交由平台服务管理器自动拉起，健康检查为 `/health/live` 与 `/health/ready`。readiness 遇存储损坏/IO 写保护会失败，不能用 liveness 代替。

## 备份、删除与恢复

SQLite 使用 WAL、`synchronous=FULL`、外键和校验迁移；业务、变更批次与幂等回执同事务。磁盘满/IO/损坏进入写保护。进程锁、掉响应重试与子进程 kill 有自动测试，但不等于真实断电/硬件故障已验收。

服务启动恢复中断作业，定期创建一致快照，检查完整性并加密。配置 `backup_target` 与 `backup_target_external:true`，由运维保证该目录实际位于独立故障域；这个布尔值只是运维声明，不能证明异机持久性。仅在保留点的本地/目标副本均验证后，按 `backup_keep` 轮换。未配置目标时不自动裁剪恢复点。可停服务后执行 `-backup-now` 生成并验证一次备份。

**必须独立托管** `backup.key`、`ledger.key` 和最新删除账本（完整目录，包括链和 head）；不要只备份 SQLite。账号删除先持久化独立签名账本，再清理资料；恢复旧备份时先重放删除，再轮换存活资料的 epoch 与会话。丢失最新账本/密钥时应停止恢复，不能凭旧库重新启用账号。

恢复仅写入全新的不存在目录，原目录保持原样；服务必须停止。Windows 用 `.exe`，bash 可用下列多行命令，PowerShell 换行符改为反引号或写在一行：

```bash
../build/daymark-server -config config.local.json \
  -restore /protected/backup/<backup-id>.dmb \
  -restore-to /protected/new-daymark \
  -restore-key-file /escrow/backup.key \
  -restore-ledger-dir /protected/current-deletion-ledger \
  -restore-ledger-key-file /escrow/ledger.key
```

命令验证成功后，由运维检查新目录并显式切换配置。客户端检测 epoch 变化后保留原队列并备份，等待用户执行“备份并重新合并”，不会自动覆盖本地候选。

## 模块边界与限制

`internal/domain` 实现纯命令规划、整树状态组、日期/时区和重复身份；`application` 编排账号、复制、传输与删除；`ports` 定义事务端口；`storage` 独占 SQLite；`httpapi` 负责严格协议与错误映射；`artifact` 负责不可变分块；`maintenance` 负责加密备份、独立删除账本与恢复；`cmd` 组装进程生命周期。

首期单写者/有界队列和只读池；不支持多副本共享 SQLite，不使用网络文件系统承载数据库。单备份明文上限 256 MiB，桌面单下载上限 64 MiB，作业总配额可配置；超限明确拒绝并保留本地数据。提供可关闭的自助注册；没有 E2EE、第三方登录、邮箱验证、邮件找回、移动端或自动部署。实际跨机备份恢复、macOS 桌面原生交互与生产运维演练仍须单独验收。
