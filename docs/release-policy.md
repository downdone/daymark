# Daymark 发布规则

当前发布规则。最新执行状态只维护在[PROGRESS](PROGRESS.md)，日常 Bugfix、New feature、Test 和 Release 的路径见[工程方法](engineering.md)。

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

本轮Windows托管更新的授权范围：用户明确选择项目自签名，采用[固定证书指纹策略](design/update/self-signing.md)与独立P-256更新签名，不降低完整CTest、实际PE验签、独立Windows部署及真实GitHub升级门禁。发布说明必须披露非Windows公共发布者信任及未覆盖的物理/最低系统矩阵；独立托管Windows验收不能标为这些实机验收通过。Linux/macOS按用户指示跳过。当前实际结果只记录于[PROGRESS](PROGRESS.md)。

## 提交与隐私

- 包含经过审查的功能、测试、设计和发行说明；不混入无关草案、构建缓存、日志、数据库/WAL/备份、凭据、证书、SDK、个人绝对路径。用户已有数据不删除或覆盖。
- 使用项目提交身份和UTC元数据；检查暂存内容和将推送的提交/标签身份。不得未经授权重写已有公开历史来清理隐私。
- 自动守卫检查已跟踪路径、常见密钥和本机路径，错误只报文件名，不输出敏感值。它不能代替人工审查；GitHub自动源码归档只包含对应提交，不上传本机目录压缩包。

## 执行顺序

1. 选版本并写发行说明。本地跑受影响测试、元数据和隐私检查。审查明确暂存的文件及作者元数据，提交后创建注释标签。确认远端无同名标签，用 `git push --atomic origin HEAD:refs/heads/main refs/tags/<tag>` 推送。普通提交、PR 和手动 CI 保留现行验证；`main` 推送只有查到当前版本的远端注释标签确实指向同一 SHA，才把完整桌面门禁交给标签候选。查不到时走常规 CI。
2. 标签并行创建源码快照和构建 Windows 候选。正式 Qt 6.10.2/MSVC2022 候选执行一次完整 CTest 与 QML lint。打包时验证实际拷入的 CRT。托管发布直接复用候选目录，最终便携 ZIP 只在签名阶段生成；单独的 CRT 盘点入口供诊断。账号与同步 CI 保留独立职责。
3. 候选成功后运行一次 `windows-update.yml`，输入 `tag`、`candidate_run` 和上一公开托管版本 `previous_version`（首次托管发行才留空）。受保护环境完成签名、独立部署、安装验收和上传，随后自动并行验证匿名公开安装与升级。公开升级直接运行现有真实 Qt 公网观察器，复用实际公开产物。查看整条工作流与资产摘要后更新 PROGRESS。失败重跑对应 job 或手动复验；同一标签和已公开资产保持不可变。

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

## 标签候选构建恢复

已发布tag的构建工具失败时，保持tag不变。用户授权的`.github/build-request.json`固定现有tag及40位commit，只有main上该文件的变更触发恢复。工作流从main读取请求与修复后的编排，恢复远端annotated对象、检查SHA相等及origin/main可达性，再让全部构建checkout固定源码SHA，执行该源码的版本/隐私守卫、完整CTest和QML lint。资产仍只能附加到该tag的既有Release，已有同名资产不覆盖；源码Release正文不修改。不把main上的新代码冒充已发布tag，也不把首次失败隐去。

跨平台run步骤若使用`$RELEASE_TAG`，必须显式`shell: bash`；PowerShell应使用`$env:RELEASE_TAG`。恢复构建的归档命名/上传统一使用解析后的tag，不使用触发它的main分支名。

Qt6.10.2官方SBOM默认tag/value（.spdx），JSON为可选生成结果。标签包保留四模块原始文档字节和实际扩展名，并校验SPDX结构；缺模块、损坏或目标重名仍阻止归档。恢复时打包工具从当前工作流提交单独checkout，源码/QML/LICENSE/二进制及BUILD-INFO源码SHA仍来自固定tag。依据[Qt生成器](https://github.com/qt/qtbase/blob/v6.10.2/cmake/QtPublicSbomGenerationHelpers.cmake)和[可选JSON生成操作](https://github.com/qt/qtbase/blob/v6.10.2/cmake/QtPublicSbomOpsHelpers.cmake)。
