# ADR-022：测试责任分层及1.0源码发布

2026-10-26，接受。用户要求测试整体审视、减少不必要重复且不遗漏链路，并明确Qt再分发许可尚未确认、1.0先发布源码。

测试层按[测试方法](../testing-method.md)确定唯一风险责任；保留全部CTest入口和关键失败/迁移/回滚断言，合并同一偏好的重复启动，worker等待改为同一状态通知，旧探针的20ms/软件渲染试验因时序未收敛而撤回，保留60ms及已知超时并规划独立链路拆分。fast/ui仅是开发入口，默认/主CI仍full，45秒late watchdog保持。原生呈现断言不以offscreen代替。

源码版本1.0.0、v1.0.0标签和发行说明一致；不把源码版本发布等同于二进制再分发许可、平台签名或所有人工门禁通过。未更改既有源码许可选择，不打包SDK/Qt DLL/数据库/日志。

本机只有Git SSH推送权限，没有GitHub CLI/API凭据；采用仓库内源码发布workflow，以GitHub作业短期GITHUB_TOKEN及仅该发布job的contents:write创建Release，无新账号/PAT或应用运行依赖。发布脚本使用Python标准库，校验tag/版本/notes/checkout并拒绝覆盖不一致发布；上传资产为空，GitHub提供对应源码归档。主CI只对分支/PR/手动运行，tag不再对同一源码重复跑两套平台测试；源码发布有独立元数据门，平台CI状态不能伪称通过。

GitHub API依据：[Release REST API](https://docs.github.com/en/rest/releases/releases#create-a-release)、[作业令牌与最小权限](https://docs.github.com/en/actions/tutorials/authenticate-with-github_token)。复用已有固定checkout提交（MIT），未引入新第三方Action或产品库。可替代方案：用户在网页手动创建Release，或已安装且已授权的gh；这里不索取长期凭据。仅推用户明确授权的main和v1.0.0，不强制重写公共历史。
