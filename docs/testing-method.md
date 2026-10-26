# 测试方法与链路责任（1.0）

2026-10-26。目标是用最少的重复准备和等待证明不同风险，不以测试数量、相似截图或单项PASS替代用户操作链。当前真实结果见[PROGRESS](PROGRESS.md)，规格以PRD和ADR为准。

## 分层与唯一责任

| 层/CTest标签 | 保留入口 | 负责的风险 | 上层不再重复什么 |
|---|---|---|---|
| storage | storage_integrity、sqlite_runtime | SQLite插件真实事务、外键、故障不改库 | 不再为每个视图各建一套存储CRUD |
| core | task/catalog/calendar/unified/event/recurrence/reminders/data_transfer | Today/截止语义、树事务/CAS/恢复组、旧schema迁移/备份、DST/循环、提醒去重、导入零修改 | UI只挑一条代表入口验证接线，不穷举全部业务边界 |
| integration | task_workspace、ux34_workspace | worker提交前后模型一致、草稿一事务、ID/版本投影、范围/独立窗与持久偏好 | 无需在每个UI主题重做DB回滚穷举 |
| integration | startup_settings、system_recovery、single_instance、widget_window | 启动项失败、系统事件合并/销毁、单写者、平台命令失败不提交状态 | Mock只证明控制器状态，不冒充真实穿透/通知 |
| ui | qml_startup | 历史完整用户链、草稿离开/焦点/滚动/拖动、设置/数据及旧资料访问 | 保留原436阶段断言，暂不重写依赖链 |
| ui | contextual_workspace | 同ID详情/父子返回、范围日周月年、离开失败、时间拖动预览/取消/落库 | 不在此穷举所有循环和DST排列 |
| ui | continuous_calendar | 有限周回收、跨月底、完整新增草稿/取消/失败零残留 | 不重复所有服务层日期算法 |
| ui | task_list_interaction | Shift/Ctrl或Command范围、单/多选菜单、禁用高亮、文本全选、密度及重复属性入口 | 不镜像批量事务实现；事务故障由task_service负责 |
| watchdog | qml_watchdog_late | 同一完整流程到436阶段后被应用自己的45秒守卫拒绝 | 这次重复执行有独立故障目的，保留；不改成早期启动成功或外部kill |
| performance | performance | 10k任务及关系的加载/搜索/月投影预算 | 选择最小投影另测适配器成本；不把两者相加当UI首帧 |
| package | package_validation | 清单、路径、schema9、篡改/重复项/ZIP、运行库来源边界 | 合成DLL只测试校验逻辑；真实部署仍由Test-Package执行 |

核心测试按风险保留当前全部可执行入口。上表所有CTest入口在完整模式中各执行一次，watchdog因故障注入独立运行一次；不把fast结果称为发布全通过。现有旧循环事件测试负责迁移兼容，不能因为新任务日程替代旧入口就删除旧数据路径。

## 本轮消除的冗余

1. `TaskWorkspaceTest`与`Ux34WorkspaceTest`中等worker空闲/就绪的50ms轮询改为`WorkspaceWait`等待`stateChanged`，维持原5秒失败期限及原predicate，数据/错误/模型断言不删。没有完成通知会明确失败，不以进程启动或固定sleep替代完成。
2. reduceMotion/highContrast/compact三个仅重复建库重启的测试合成一次外观偏好持久化场景；各自默认、错误类型、改值及重启结果保留。批量业务场景不再顺便重复compact持久化；backupDays合法/非法与批量冲突重启仍保留。
3. 历史UI探针暂保留60ms轮询、45秒守卫和所有阶段/frame断言。20ms与软件渲染试验暴露旧搜索去抖、tooltip及虚拟delegate的帧时序依赖，未作为默认或发布通过条件；试验代码已撤回。下一片需按有独立fixture的链路拆分及条件等待，不能为绿色结果批量删阶段、加长预算或降低呈现断言。
4. CTest标签提供开发者按改动跑fast或ui；默认及CI仍跑full。UI测试共享resource lock，外部开启CTest并行也不让几组窗口抢焦点。新功能按风险选择增量测试，不默认复制所有主题/尺寸组合。

不能删除：WAL快照/升级失败、导入晚失败零修改、树CAS和中途写失败回滚、DST间隙/重叠、提醒持久去重、关闭排空已接受写入、真实单实例进程、穿透恢复通路、200%键盘可达、脏草稿失败保持。

具名C++测试盘点从110个变为108个：仅`reducedMotionPreferenceIsExplicitAndPersistent`、`highContrastPreferencePersistsAndRejectsInvalidValue`、`defaultCompactDensityPreservesSavedComfortableChoice`合并为`appearancePreferencesPersistAndRejectInvalidValues`；数据行和现有业务测试未删。其余23个CTest入口不变。

## 端到端链路矩阵

| 用户链 | 规则/事务证据 | 接线及交互证据 |
|---|---|---|
| 新建→安排→查看→重启 | task_service、unified_tasks、task_workspace | qml_startup、continuous_calendar |
| 编辑→切任务/窗口/视图→失败保留 | task_service、task_workspace | qml_startup、contextual_workspace |
| 父子创建→整树完成/恢复/软删→重启 | task_service、unified_tasks | qml_startup、contextual_workspace |
| 多选→范围/菜单→批量预览→冲突全回滚 | task_service、task_workspace | task_list_interaction |
| 日历新建/跨天/月→拖动/端点→Esc/提交 | calendar_service、ux34_workspace | contextual_workspace、continuous_calendar |
| 重复/提醒→实例→编辑/完成→去重 | recurrence_service、reminders、unified_tasks | qml_startup、task_list_interaction |
| 备份/导入/升级→失败零修改→恢复 | data_transfer、task_service、sqlite_runtime、storage_integrity | qml_startup数据/错误入口；真实恢复仍需独立操作 |
| 小窗/mini/独立日历→置顶/穿透→恢复 | widget_window、system_recovery、task_workspace | qml_startup/原生探针＋平台人工矩阵 |
| 源码发布→版本/Tag/Release一致 | 版本检查、干净暂存及隐私扫描 | Git远端与GitHub Release读取确认；二进制许可/签名独立 |

未知或未覆盖的链路须记UNVERIFIED，不将此表当作全产品已验收。OS通知送达、物理IME/读屏、壁纸透明对比、睡眠/多屏/8h、独立干净机、macOS本轮源码仍有独立平台门禁。

## 运行方式

Windows（先设置DAYMARK_QT/DAYMARK_MINGW）：

```powershell
./scripts/test-windows.ps1 -Preset windows-release -Suite fast
./scripts/test-windows.ps1 -Preset windows-release -Suite ui
./scripts/test-windows.ps1 -Preset windows-release # full，发布默认
```

macOS：

```bash
bash scripts/build-macos.sh macos-release # 构建＋full
ctest --preset macos-release -L 'core|storage|integration'
ctest --preset macos-release -L '^ui$'
ctest --preset macos-release # 发布完整
```

真实Windows包：PowerShell 7运行`scripts/Test-Package.ps1`；真实macOS签名/公证与平台矩阵另验。此轮仅发源码，未满足再分发许可的Qt运行库不上传。

## 以后增加测试的准则

先选择能最便宜发现缺陷的层；跨层重复仅保留“规则正确但入口接错”的代表性链。每次删除或合并须写出旧断言去向与失败边界，不能只比用例数。等待依赖状态、模型身份、成功/失败信号或真实呈现；采样指标与业务正确性分开，别把偶发机器负载放进普通CRUD硬阈值。历史巨型探针后续按拥有完整fixture的业务链拆分，分段前保留依赖清单；本轮不冒险删阶段来制造绿色结果。
