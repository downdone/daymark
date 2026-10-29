# ADR-029：同步服务实现依赖

日期：2026-10-28。状态：随用户“开始完整实现”授权采用；未授权部署或真实数据上传。2026-10-29用户另授权完善CI并推送源码。

服务代码位于server独立Go module，工具链基线Go1.27.0，使用标准net/http、database/sql、crypto、testing/httptest。新增依赖锁定在go.mod/go.sum：

| 直接依赖 | 版本 | 许可 | 必要性/替代 |
|---|---|---|---|
| modernc.org/sqlite | v1.61.0 | BSD-3-Clause；SQLite自身public domain，传递归属另保留 | 无CGo，随带SQLite3.54.1；替代mattn需C工具链。实际启动拒绝已知WAL-reset未修复版本，并测试真实引擎/PRAGMA。|
| golang.org/x/crypto | v0.58.0 | BSD-3-Clause | Argon2id标准实现，避免自写密码算法。|
| golang.org/x/sys | v0.49.0 | BSD-3-Clause | Windows LockFileEx/Unix flock可恢复的进程互斥；避免仅存在性锁文件造成崩溃后永远拒绝。|
| github.com/santhosh-tekuri/jsonschema/v6 | v6.0.3 | Apache-2.0 | 完整2020-12约束验证并复用OpenAPI组件，避免手写不完整validator。替代是维护105份重复字段校验，不采用。|

协议文件嵌入构建，测试校验与docs/api原文一致；compiler仅将非零年份负向前瞻改为等价RE2表达式，正反fixture验证。重复键/UTF-8/surrogate/整数lexeme检查发生在标准JSON解析替换文本之前。类型Schema与业务规则仍分别负责结构和语义。

Go模块校验保持启用。当前机器默认proxy.golang.org连接不可达，单次命令使用goproxy.cn镜像下载，未写全局go env配置；生产构建不依赖该本机设置。第三方源码不复制进仓库，发布保留许可证/NOTICE及依赖清单；版本升级须重新执行存储/协议/认证/双端门禁。

参考：[SQLite WAL修复说明](https://sqlite.org/wal.html)、[驱动官方包](https://pkg.go.dev/modernc.org/sqlite@v1.61.0)、[JSON Schema实现](https://github.com/santhosh-tekuri/jsonschema)。
