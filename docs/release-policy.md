# Daymark 发布规则

2026-10-26。用户已确认保留公开v1.0.0，本次使用v1.0.1；本规则适用于后续发布。最新执行状态只维护在[PROGRESS](PROGRESS.md)。

## 版本和标签

1. 使用SemVer：修复/兼容的体验调整递增PATCH，兼容功能递增MINOR，不兼容协议或数据契约才递增MAJOR并说明升级/回退。尚未支持自动预发行标签；本流水线仅接受无前导零的`vMAJOR.MINOR.PATCH`。
2. CMake的project VERSION、DAYMARK_VERSION、对应发行说明标题及tag完全一致。新版本使用`docs/release-notes-v完整版本.md`；已有v1.0.0保留旧`release-notes-v1.0.md`兼容路径，不覆盖旧发行说明。
3. 标签必须为annotated tag，精确指向已提交且已在origin/main中的版本提交。公开标签/Release不改指、不复用；修正进入下个版本，不force push公共分支。推送前先查远端同名标签，出现冲突停止而非覆盖。
4. 仅推明确授权的main和本次单个tag，不使用all/mirror或自动跟随本机历史标签；本地旧local标签不公开。推送使用非强制方式，条件允许时将分支/tag原子推送。

## 源码快照和二进制门槛

用户运行包遵守[ADR-025](ADR/025-minimal-portable-payload.md)：禁止递归复制docs及验收脚本，打包后白名单检查会拒绝开发资料、调试插件和工具元数据；保留必要运行资源、简明说明、第三方追溯与小型测试/完整性元数据。新依赖必须补充规则与无SDK验证，不能直接放宽为整个目录。

- 用户明确请求的本地预览包可用`./scripts/package-windows.ps1 -LocalPreview`，严格范围见[ADR-024](ADR/024-local-preview-package.md)。仍执行完整CTest并保留全部用例结果，仅容许已记录的两个旧UI探针失败，其他失败/跳过/缺失均阻止打包；默认命令及正式发布要求不变。该参数不授权上传运行库，也不标记RELEASE-READY。
- Qt 按 LGPLv3 动态链接使用（[ADR-027](ADR/027-open-source-qt-ci.md)）：公开分发二进制时须随包附带 LGPLv3 文本、Qt 版权声明及源码获取方式，且必须由 CI 使用的开源 Kit 构建；开发者自备 SDK 的版本与许可配置不进入仓库。源码授权选择不因tag自动成立。
- 源码快照允许如实披露已有UI时序失败和未验证平台；必须具备可构建、相关链路证据及发行说明中的FAIL/UNVERIFIED边界，不得称RELEASE-READY。源码发布job只证明元数据/源码守卫和Release创建，不取代桌面完整CI。
- 正式二进制须完整CTest、数据/迁移/备份/故障、UI状态/键盘/缩放、真实平台/辅助技术、干净机/签名及运行库许可等门禁通过；不得加大watchdog预算、删断言或以mock/offscreen代替实测。

## 提交与隐私

- 包含经过审查的功能、测试、设计和发行说明；不混入无关草案、构建缓存、日志、数据库/WAL/备份、凭据、证书、SDK、个人绝对路径。用户已有数据不删除或覆盖。
- 使用项目提交身份和UTC元数据；检查暂存内容和将推送的提交/标签身份。不得未经授权重写已有公开历史来清理隐私。
- 自动守卫检查已跟踪路径、常见密钥和本机路径，错误只报文件名，不输出敏感值。它不能代替人工审查；GitHub自动源码归档只包含对应提交，不上传本机目录压缩包。

## 执行顺序

1. 同步远端并查现有版本/标签；确定增量版本，先写真实发行说明和本轮PROGRESS计划。
2. 审查并暂存明确文件，运行`python scripts/publish-source-release.py --check-current`、发布守卫单测、文档链接/颜色检查及合适的构建/测试；完整门禁结果如实披露。
3. 检查cached diff、隐私及项目作者，提交。创建本次annotated tag，重新检查其commit/版本/说明；确认远端没有同名tag，再非强制推送main和该tag。
4. 分支CI跑发布守卫一次及Windows/macOS/Linux完整构建/测试。tag并行触发两条流水线：`release-source.yml` 校验已跟踪源码、注释标签、checkout SHA及origin/main可达性，创建源码Release；`release-build.yml` 在同一tag上构建三平台、跑完整CTest与QML lint，并把归档附加到该Release。两者都用短期最小权限GITHUB_TOKEN，源码入口不因资产存在而失败，但会拒绝非本流水线命名的资产。
5. 同一tag发布串行，幂等重试只接受目标和说明均一致的现有Release，不静默修订；若失败修复后重跑，不移动公开tag。确认远端分支/tag和Release真实内容，再汇报链接及CI状态。

本地预检（Windows和macOS均使用Python3；macOS通常命令为python3）：

```text
python -m unittest discover -s tests -p test_release_metadata.py -v
python scripts/publish-source-release.py --check-current
```

发布请求仍须来自用户；新增规则本身不授权代理自动推送、上传或发布。GitHub没有可用凭据时不得索取或记录长期token，复用已获授权的Git/Actions通路。


## 已公开标签的发布流程恢复

标签推送成功但Release作业失败时，不移动标签。若修复只涉及发布工具，可在main更新工具，并由用户授权的`.github/release-request.json`明确固定tag、40位commit和source_only；只在该文件变更时启动恢复作业。工具和源码分别checkout，源码从既有tag读取，再校验其注释标签、固定SHA、main可达性及元数据/隐私。这个入口不上传资产、不重写tag或不一致的Release，也不赋予以后自动发布的授权。原始失败记录保留；作业错误以安全注释回报，不能回显token。

发布工具将webhook的完整SHA解析为commit（允许annotated tag对象SHA，但仍校验同一tag及目标commit），显式获取origin/main再判定可达性。所有源码发布入口共用串行队列，避免重复请求竞争创建Release。

Actions checkout可能在一次性runner中把事件标签落成本地轻量引用。发布前显式重新获取远端完整标签对象：`git fetch --no-tags --force origin "refs/tags/$RELEASE_TAG:refs/tags/$RELEASE_TAG"`，这里的force仅替换CI本地引用，不写远端。随后仍强制检查annotated类型、固定commit和main可达性；不得放宽守卫或移动公开标签。标签入口与恢复入口采用同一准备步骤。

创建Release前后还通过GitHub API核对远端tag对象与已校验的本地annotated对象一致。对已存在的tag，不发送仅用于新建标签的`target_commitish`提示；Release来源以tag对象/剥离commit为准，不以该创建提示字段为准。这样不需要给短期GITHUB_TOKEN扩大Workflows写权限。官方字段及权限语义见[GitHub Release API](https://docs.github.com/en/rest/releases/releases#create-a-release)。已有Release仍须名称、正文、tag、非草稿状态和无资产全部匹配，不静默修改。
