# PERF-01 性能与内存优化记录

2026-11-04，Windows x64 / Qt 6.10.2 / MinGW 13.1。策略、接口与失效契约见[设计](design/performance-and-memory.md)。本轮使用合成临时数据库；未读写用户任务、改变数据模式、同步协议、CI或构建脚本。1.8.1发布候选在独立工作树，本轮不创建发布标签。

## 方法与实现

先记录输入规模、构建参数和原实现，再逐项比较正确性与成本。各表使用预热后的中位数（通常5～7次），不从少量样本推导P95。初次全量加载等单次数据另行标注。进程私有提交、工作集、GPU纹理和文件大小不是同一指标。

| 切片 | 实现 | 保留的行为与验证 |
|---|---|---|
| 日历 | 只为当前7/42天建立起止端点索引 | 独立逐日对照，跨千年区间、无效日期、完成显示、周起始 |
| 快照 | id→行号替代第二份完整Task字典；提醒呈现复用只读查询 | 行号/id双校验，异步操作前复制单任务，跨重载不保存指针 |
| 提醒 | 快照更新时缓存下一有效UTC提醒；每秒判断O(1) | 仍比较真实UTC时钟，保留回拨、前跳、重复、取消、归档及远端删除语义 |
| 列表模型 | 首次批量插入；同集合重排用layout和持久索引映射；无变化不通知 | QAbstractItemModelTester、持久索引、选中身份、增删、日期变化；不使用reset |
| 刷新 | 主列表导航/筛选与数据快照刷新分开 | 写入、同步、重载、日期仍全量失效；独立今天/截止列表不受主筛选影响 |
| 数据读取 | 未到期提醒先解析时间再构造Task；无活动重复规则不读任务全图 | 原事务/CAS保留；TEMP视图故障注入证明活动规则必须真实读取并报告错误 |
| 查询投影 | 排序、父子表、根及遍历栈存行号，最终输出才拥有Task | 稳定比较器、折叠、搜索、跨分类根、垃圾组与输入快照不变 |

QML/窗口、客户端同步和Go服务已逐层审查，具体取舍见设计表。没有为覆盖层数而盲改缓存、纹理、索引或安全参数。混合大量插删的模型复杂度、维护后的无写全量刷新、同步大图JSON及服务端分配仍需独立夹具和证据。

## 同配置前后对比

发现旧开发目录虽然名为Release，但CMAKE_CXX_FLAGS_RELEASE为空。以下**全部是在该相同、未开启C++编译优化的开发配置下**比较，适合判断算法/数据结构收益，不能作为发布SLA；也不能与下一表相除计算加速倍数。

| 场景 | 修改前 | 修改后 | 证据文件（build/perf01内） |
|---|---:|---:|---|
| 4万任务，稀疏周日历 | 120.56 ms | 8.16 ms | reference-calendar.txt / confirmed-performance.txt |
| 同夹具月日历 | 108.91 ms | 7.98 ms | 同上 |
| 1000行不变替换 | 1.514 ms | 0.0046 ms | reference-model.txt / optimized-model-1000.txt |
| 1000行完全逆序 | 785.30 ms | 1.279 ms | 同上；count通知9→0 |
| 4万行树投影 | 1620.43 ms | 686.86 ms | tree-baseline.txt / tree-optimized.txt |
| 同树投影测试进程峰值私有提交 | 367.10 MiB | 169.63 MiB | 同上；包含输入/输出/运行库，不是整应用内存 |
| 1万未来提醒数据库轮询 | 59.41 ms | 45.40 ms | reminder-baseline.txt / reminder-optimized.txt |
| 1万任务主视图切换 | 448.79 ms | 204.62 ms | navigation-baseline.txt / navigation-workspace.txt；比较刷新分工前后 |

各行逐切片测量，时间点不同，不相加推导整应用收益。数据库提醒轮询仍扫描候选；O(1)指工作区每秒“是否到期”的判断，不能混淆。

## 真实Release结果

随后显式配置`-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG`，完整重建相关目标及应用（204个构建步骤），两项QML lint通过。以下为最终Release数据，日志release-configure.log、release-build.log、release-performance.txt、release-workspace.txt。

| 场景 | 结果 | 范围 |
|---|---:|---|
| 4万稀疏任务周 / 月日历 | 2.65 / 3.00 ms | 周2.30～2.68、月2.60～4.08 ms |
| 4万行树投影 | 171.26 ms | 测试进程峰值私有提交171.52 MiB |
| 1万行不变替换 / 逆序 | 0.027 / 5.83 ms | 无冗余count通知 |
| 1万任务主视图切换 | 54.83 ms | C++投影，不等于物理屏幕反馈时间 |
| 1万未来提醒数据库轮询 | 39.67 ms | 包含SQL候选读取 |
| 1万根任务+3万子任务混合数据 | 加载517、搜索7、月投影17 ms | 单次原有预算测试；20分类、200项目、50标签、30%完成 |

最终另起两个独立Release测试进程，4万任务的索引额外私有提交由68.52 MiB降至1.81 MiB（71843840→1896448字节，约减少97.4%）；工作集增量由68.29降至1.94 MiB。日志release-memory-full.txt / release-memory-row.txt。它只比较额外索引，不是4万任务整个快照的大小，也不能外推整应用150～250MB目标。

## 验证与证据边界

服务层和工作区均使用QTemporaryDir。Release性能测试7通过；工作区39通过、0失败、1跳过（本轮未配置真实Go TLS端到端夹具）。task_service、calendar、recurrence、reminders四项CTest通过。calendar_interaction、desktop_task_menu、task_list_interaction在正常用户权限下通过，覆盖实际QML输入、键盘、草稿、菜单及列表交互。

完整desktop_surfaces在删除菜单处暴露与先前1.8.1 Windows CI同位置的失败；单独widget-actions切片通过。保留widget-long-diagnostic、widget-geometry日志：菜单窗口高226但菜单本体高12，目标未命中。为区分等待呈现与布局问题，诊断中显式读取已呈现帧后仍为12，未保留该无效强制重绘补丁。最后完整链仍FAIL（widget-presentation.log/xml），不把切片通过冒充完整通过；探针仅留下几何/命令状态诊断，断言与预算不变。现有证据不足以确定其全部触发条件，修复仍待独立切片。

Windows原生短流程在既有DAYMARK_SMOKE_MENU_FALLBACK=1配置下通过：快捷添加/菜单/拖动/提醒后，22.995秒运行2轮创建、时段编辑、完成/重开、软删/撤销、备份/完整性检查与重载；2次完整性检查，最大事件循环延迟13ms，无QML警告，退出0（native-fallback-soak.json及stderr日志）。私有提交/工作集每500ms采样，共64点；覆盖启动、界面链及短循环的峰值分别541.46 / 512.57 MiB，末次231.68 / 255.04 MiB。存在明显阶段性峰值，不能称为稳定平台或宣称整应用目标已达成；没有相同源码基线下的整应用对照，不能宣称整应用节省百分比。优先后续分离Qt/D3D初始化、窗口/字体及交互峰值的分配来源，再决定是否修改渲染资源生命周期。

原生首轮漏设上述菜单验证后端开关，在阶段225后退出，native-short日志与采样保留，未进入短循环，不记PASS。按仓库已有后端契约修正配置后才得到前述结果；普通客户端不设置该变量。两轮短循环不能证明长期无泄漏或物理桌面无闪烁。

失败/无效数据保留而不纳入收益：最初稀疏任务未设ACTIVE；一个Copy-Item保留旧时间戳导致旧对象未重编；新测试的备注null违反NOT NULL、标题修改误用saveDetails。分别修正夹具/API/构建输入后才生成上表。旧10000行逆序开发基线耗时过长主动停止，使用有界1000行对照，不反复重试。沙箱中单实例IPC被拒导致UI提前退出，之后用正常用户权限和相同隔离数据运行；未禁用IPC保护。

## 复现与后续门槛

### 干净打包补验

同日用户要求便携包后，从eb59dfb建立全新工作树，383步Release及两项QML lint通过；完整CTest一次31/31 PASS、346.63秒，含desktop_surfaces。前述菜单阶段232失败在此未复现，未修改菜单、不宣称根因已解决。旧镜像9份UI/账号测试文件最初有字节差异，确认仅CRLF/LF格式不同；干净重建避免依赖旧生成资源。

标准包Windows原生完整smoke在Mini阶段76等待呈现超时，标准ZIP未生成。按ADR-024交付本地预览，复用同源码/同程序已完成的31项真实控制台结果，完整库存/状态/日志散列核对后保留摘要，不再跑整套测试；BUILD-INFO另外明确nativeFullSmokePassed=false，不能把CTest全通过当原生全通过。预览既有contextual-workspace原生SDK隔离验证通过（17964ms、2窗、无QML警告），清单/运行库来源/ZIP及SHA256通过。包名、散列和完整失败证据见PROGRESS；未签名、不上传，版本仍1.8.0，1.8.1候选继续独立。

Windows设置DAYMARK_QT（Qt6.10.2）与DAYMARK_MINGW后，使用`scripts/build-windows.ps1`及`scripts/test-windows.ps1`；确认实际Release优化参数。定向入口：`test_performance`、`test_task_workspace`，以及上述四个核心CTest和界面入口。索引测试可在两个独立进程中分别设置DAYMARK_PERF_INDEX=full / 清除该变量，运行`test_performance snapshotIndexFootprint`；模型规模可用DAYMARK_PERF_MODEL_SIZE=1000，默认10000。不得使用用户资料目录作为数据库。

macOS使用`bash scripts/build-macos.sh`后在对应目录运行`ctest -R 'performance|task_workspace'`，本轮未实测。测试没有放宽既有时间阈值。Windows物理DWM帧延迟、冷启动、整应用/GPU内存、休眠/多屏、长时间增长斜率、真实同步负载和Go并发/分配仍UNVERIFIED；短测试不能证明长期无泄漏。
