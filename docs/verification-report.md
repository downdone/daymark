# UI-15 动作图标与语义配色（2026-10-24）

设计与逐屏动作矩阵见[UI-15](design/actions-and-color.md)，映射P-13/HIG Buttons/Icons/Color。当前结果见PROGRESS；本轮不引入数据模式或外部资源变化。

最终Debug17/17（36.70秒），应用/样式qmllint无诊断；`python scripts/check-ui-colors.py`直接从token检查浅深及增强四组共260对，普通文字最低4.586、必要边界最低3.074，原始值均达4.5/3目标。禁用/装饰线及任意壁纸不作此承诺。原生191阶段全截图退出0、双窗口、warnings=false且无未完成创建诊断：纯图标＋按钮真实命令提交并清空、图标名称/图文布局、增强对比开关及实色背景/原透明度恢复；此前菜单、草稿、日期、列表与备注滚动检查保留。已查看浅深/增强200%动作、设置解释、任务详情、日历及独立420×480/200%详情底部动作截图。所有自动数据为QTemporaryDir。

中途生成清单未含新IconToolButton导致一次qml_startup FAIL，稳定清单重新configure后修正并全测；移除未使用import后lint干净。源码cead7b0冻结后严格Release17/17（39.82秒）及SDK隔离191阶段PASS；双窗口/warnings=false/无未完成创建，32个实际模块来源、1471文件清单、ZIP及SHA侧车全部通过。最终包与SHA见PROGRESS。macOS、真实辅助技术/IME/物理DPI及长时平台验收仍UNVERIFIED。

# CAL-05 系统评估与日程浏览定位（2026-10-24）

[系统评估/路线](review-rc2.md)、[FR-022设计](design/calendar-browsing.md)。新增TaskWorkspace临时库用例：旧系列16点与新单次9点按本次时间重排、全天优先；后续实例日期及系列起点/提醒摘要；预览不改rowVersion；无效、改期和删除key不回退错误实例。领域初测Debug17/17（39.07秒），界面/最终Debug17/17（35.18秒）与两项lint通过。无migration或新依赖。

新原生180阶段覆盖后续实例只读详情（无编辑器/非模态）、实际点击进入系列编辑、日期错误保留、2028闰日定位及独立窗2024闰日不影响主窗。默认、深色200%详情和非法日期截图已查看，完整标题、实例日期、系列起点及固定编辑/关闭区可见；没有用工程实现字段当产品文案。

首次lint弹层声明布局警告，迁入ScrollView非布局节点后复跑干净；首次原生在既有备注轮事件场景FAIL，焦点与合成滚轮同回调使投递/布局不稳定。探针按既有几何场景等待焦点渲染稳定，原位移/选择/焦点断言保留，复测轮事件接受/正文位移，180阶段退出0/双窗口/warnings=false，无未完成创建。该改动只增强合成场景时序，不冒充真实鼠标/触摸板人工验收。补充独立窄窗200%详情及最终严格包结果见PROGRESS。

最终b5ee06b：Debug17/17（35.43秒），严格包Release17/17（35.90秒），SDK隔离及最终Release截图180阶段均PASS，退出0/2窗口/warnings=false，无未完成创建。补测明确调用resize(420,480)并断言实际逻辑尺寸；已查看独立窗200%预览PNG（630×720物理像素），长内容由正文滚动，关闭/编辑操作保持可见。首轮只验证独立窗200%，未固定最小尺寸，不作为窄窗通过证据；最终包及全截图以新断言通过。没有改业务断言或放宽45秒普通/90秒截图时限。

1470文件、33库来源、实际EXE/Qt6.10.2/schema4及ZIP/侧车PASS；报告a3c5d143175b429da785118533ef29ae。文件名、SHA及当前限制统一见[PROGRESS](PROGRESS.md)。macOS及真实系统/输入/长时门禁保持UNVERIFIED，不将视觉截图当物理设备验收。

# CAL-04循环事件与事件提醒（2026-10-23）

FR-021、UX-01/05/06/07/09/10/11；HIG映射及交互契约见[事件系列设计](design/event-series.md)。领域/schema004/JSON兼容、每实例去重、事件表单与主/桌面月格已集成；最终Windows部署结果见PROGRESS。

新增测试：schema3事件升级先快照并保留默认值；旧JSON/完整schema4规则与去重历史往返；31日跳月、闰日、固定当地时间与DST间隙跳过、间隔/包含结束日；全天09:00及提前量；只读失败后事务重试、重启去重、软删恢复无重复。自动数据全部QTemporaryDir。首次DST间隙用例FAIL暴露无效QDateTime先toUTC被归一化，已先校验当地时间并保留原用例复验；全天提醒UTC导出也已补测。

中途原生滚轮检查FAIL：备注手动滚动被后续布局/光标回调吸回。新增手动滚动暂停自动定位，焦点/编辑恢复；原滚轮和焦点断言保留。修正后原生172阶段PASS（退出0、双窗口、warnings=false，无未完成创建）；系列表单真实保存每周/提前5分钟后重开一致。截图定位曾在文字缩放布局未稳定时读旧坐标，普通与200%滚动到错误段落；采用与旧编辑器几何检查相同的有界渲染等待，并新增分组实际可见断言。最终截图另记，不能把旧错误定位图当通过证据。

Repository集中SQL及提醒记录事务，EventService集中规则和时间计算，无QML循环筛选。重构后Debug17/17（38.16秒）、Release17/17（33.62秒）及两项lint通过；最终截图与部署将重建最新探针。真实系统通知送达、macOS、IME/读屏、物理DPI/多屏/8h和独立干净机仍UNVERIFIED。

最新探针源码复建：Debug17/17（32.62秒），两项lint通过，Release构建成功。172阶段原生全截图PASS，退出0/双窗口/warnings=false，无未完成创建；已查看默认完整循环与提醒组、深色200%控件重排/固定提交区及月格实例色条，分组实际可见断言通过。系列编辑仍只有一个正文滚动；200%说明文字超出部分可继续滚动。最终已提交源码严格部署另记于PROGRESS。

最终617d4f0严格包重建Release17/17（32.98秒），SDK隔离172阶段退出0/2窗口/warnings=false，无未完成创建；33运行库来源、实际版本/Qt6.10.2/schema4、1468文件及最终ZIP/SHA侧车PASS，报告0d2239a7ed76419f90fcd3e6921daf37。最新本地交付文件、大小/校验和及剩余人工门禁统一见[PROGRESS](PROGRESS.md)。源码提交前路径/凭据扫描无新增命中，工作区仅交付证据文档更新。

# CAL-03 独立桌面日历（2026-10-23）

源码e2a3c8277d641b64c6bb2a6718d5c8d5931b0f86，Windows/Qt6.10.2。Debug17/17（30.59秒），应用/样式lint通过；严格打包重建Release17/17（32.04秒）。新增TaskWorkspace用例验证独立42天/7天查询、闰日翻月、非法输入、同源任务、主窗视图不变、偏好越界拒绝及重启恢复；WindowStateStore用例验证独立命名空间和最小尺寸，不覆盖主窗几何。

原生探针扩展167阶段，验证独立日历和小窗可见、独立翻页不改主视图、关闭日历后小窗继续显示，最后恢复双窗退出。默认/200%截图已查看，背景RGBA采样alpha179/255符合70%设置；不是系统桌面合成/真实背景可读性的完整人工验收。早期同名事件/议程控件跨窗口定位错误导致FAIL，改为主日历面板内查找后原断言通过；验证helper翻页后恢复选中日期，避免截图议程日期与锚点不一致。

严格SDK隔离检查PASS，报告3bd379fc8ede44c9901d366db9aedbd6，退出0/双窗口/warnings=false，无未完成创建诊断。Qt/MinGW来源、实际EXE版本/Qt/schema、清单1466文件与ZIP逐条/SHA侧车全部通过。包Daymark-1.0.0-rc.2-windows-x64-local-20261023-214427.zip，44990300字节，SHA256 b4458b60d5f19a178e4b4b588c05e222dcfa4ee9657a181ae127c5801c75f213。

全部自动库QTemporaryDir，未触碰用户库/旧进程，无schema变更、无推送上传。macOS、真实拖动/缩放/遮挡层级、物理多屏/DPI、系统桌面合成、IME/读屏、8h/干净机、签名许可保持UNVERIFIED。最终截图复核另记于PROGRESS。
# UI-14 Apple HIG全界面重设计（2026-10-23）

Windows11 x64、Qt6.10.2/MinGW13.1，schema3。按[设计](design/hig-redesign.md)分三切片实施；初次完整Debug构建/CTest17/17（29.46秒），两项QML lint无警告。当次原生162阶段退出0、双窗口、warnings=false，无未完成创建或DelegateModel取消诊断。此前集成Release17/17（30.01秒）；最终提交的Release与严格部署另记，不把早期结果冒充最终包结果。

查看合成截图：主导航/Today、连续文档详情、默认/200%组织与重复、事件、批量、数据、三类设置200%及默认深色、日历月格/议程、小窗/Mini。批量默认收缩到内容尺寸、组织空列表收紧，详情滚动条改为共享轻量样式。选中分段以字重/填充/颜色同时反馈；标题备注直接编辑、日期手输/选择、原子草稿和失败状态继续保留。

新增断言覆盖设置三类实际按钮信号切换、200%导航/内容存在、通用类系统提醒开关可见，规则草稿取消保留与放弃后继续新建；原有日期键盘、滚轮边界/焦点、任务/事件草稿与冲突测试继续运行。程序化信号不等于物理鼠标；新分段左右键已实现/静态检查，物理键盘/读屏仍待实测。

失败与修正：首次设置点击因Repeater委托不在C++预期QObject子树而FAIL，改由类型化QML helper调用实际Button.clicked并保留内容断言；同目录构建/lint误并发触发Ninja锁冲突，串行复跑通过。一次原生退出复现未完成创建5项及DelegateModel取消提示；下拉框只在展开时绑定选项后，连续两次带截图原生复跑干净，严格部署仍拒绝该诊断，不宣称已普遍修复Qt内部生命周期。逐项审查补回系统提醒选项，并加入当前设置窗错误提示。

数据全部QTemporaryDir，启动项用隔离ini，未操作真实用户库/启动项/旧进程。macOS、IME/Narrator/VoiceOver、OS高对比/物理DPI、多屏/睡眠/8h、独立干净机、许可与签名仍UNVERIFIED。原始日志和截图仅留忽略的build目录。
## UI-14严格部署首轮及创建时序修正

5d27e52源码的首次严格部署FAIL（171729目录、报告89301ddc）。Release重建CTest17/17（30.38秒），SDK隔离探针退出0/2窗口/warnings=false，但退出记录DelegateModel::cancel及未完成创建2项，校验工具正确拒绝，没有生成ZIP。这覆盖前文源码环境两次干净复跑，说明仅限制隐藏下拉框不足以消除部署场景问题。

依据[Qt 6.10 ListView cacheBuffer](https://doc.qt.io/qt-6.10/qml-qtquick-listview.html#cacheBuffer-prop)：缓冲区会异步创建视口外委托。采用公共属性cacheBuffer=0，覆盖任务、小窗、议程、关联选择、提醒及共享菜单/下拉/按钮行，保持可见行虚拟化、模型增量与完整数据；停止视口外预创建以减少切换/重置与委托创建交叠。这是本Kit的保守规避，不修改Qt或业务模型，不声称已定位Qt内部根因。取舍为滚动新行时即时创建，尚未实测10k复杂委托的滚动帧率。最终结果见后续记录。

中途验证还暴露两项：取消缓存后下拉框初始视口为零，原生实际选项测试FAIL；补非零估算高度后复验。备注立即滚动与焦点定位排队重叠，原滚轮断言FAIL；正文新增非阻断轮事件观察，取消过期定位且继续交由原Flickable滚动，阻断观察的初次实现失败后改为blocking=false。新增诊断曾漏QObject到QQuickItem类型转换导致编译失败，已修正。所有原断言保留；修正后Debug17/17（36.71秒），当前最新视口修正与Release连续检查另记。
最新视口/滚轮修正后：Debug17/17（31.35秒），两项lint无警告；Release连续3次原生162阶段均PASS，退出0/双窗口/warnings=false，无未完成创建/DelegateModel取消。失败日志保留，未删除或绕过任何原断言；最终带截图检查与严格部署另记。

最终带截图Release复验首跑在phase151触发45秒看门狗，功能断言未报错；截图模式涉及逐页同步读回，单独设置90秒有界预算。普通/CTest/严格包检查保留45秒，未放宽业务、滚轮、焦点或创建诊断门禁。最新源码重建并复跑截图及CTest，结果另记。

最终源码Debug17/17（29.45秒），Release全截图162阶段PASS（退出0/双窗口/warnings=false，无创建/取消诊断）。查看最新选项弹层、短备注焦点/滚轮、浅深/200%和三类设置截图；较早源码的失败证据保留。最后UI改动的两项lint已通过，随后仅C++诊断/截图时限调整。下一段记录严格部署。

## UI-14最终严格部署（源码433ac10）

源码433ac102eb990ac2b4a83ffe4f4f8c0e84199c80，最终Debug17/17（29.45秒），QML应用/样式lint通过；严格打包重新构建Release17/17（31.39秒）。全截图162阶段及普通Release连续3次均PASS。最终SDK隔离原生162阶段退出0/双窗口/warnings=false，日志无未完成创建或DelegateModel取消诊断；33个部署模块审计通过，唯一系统运行库例外仍为System32/D3DCompiler_47，Qt/MinGW/QML来自包内。

报告443bfe63835a469e86395a4117e68740，passed=true，userDataAccessed=false。清单1465文件、EXE版本/Qt6.10.2/schema3元数据、ZIP逐文件SHA/长度、侧车校验全部通过。交付ZIP Daymark-1.0.0-rc.2-windows-x64-local-20261023-205422.zip，44960270字节，SHA256 b236aa2a4aec704b991981c49120393c0f1eff52df6a7c8dc11eb66d58284812。

源码快照Daymark-1.0.0-rc.2-ui14-source-20261023-205346.zip，485548字节，SHA256 104a69a978814e6b934a40c22c9194f122285001c59e4c7d09e9e70137c5efbe，敏感文件扫描0项。保留旧目录/包与用户进程，无远端发布。未重新运行中文/空格解压路径或独立干净机；macOS、真实辅助技术/IME/DPI、多屏/睡眠/8h、复杂10k委托滚动帧率及许可签名仍UNVERIFIED。
# UI-13视觉系统与月格事项（2026-10-23）

Windows11 x64、Qt6.10.2/MinGW13.1，schema3。最新Debug构建/CTest17/17（28.43秒），应用/样式lint无警告；此前Release17/17（29.82秒），最新探针随严格打包再验。原生152阶段退出0、双窗口、warnings=false，本次日志无unfinished creation诊断。

实际断言：1000×720默认紧凑日历和880×580/200%日历每个非空日期均创建真实标题Label、文字对应原事项、格内标题不裁切；+N保留议程入口。日期/任务/事件模型和业务语义不变。首轮新增校验失败因校验字符串缓存缺少Repeater.count依赖；修正绑定后全流程复跑通过，没有删除断言。QML检查函数仅供合成验证，不扫描真实用户数据。

截图查看日历默认紧凑/200%、详情宽窄/200%、浅深控件和标题/短备注：外框已移除；标题/备注直接编辑，日期/规划/归属等共享FieldSurface；焦点/草稿/错误、单正文滚动和固定提交路径保留。其他原生运行曾打印系统clipboard重试提示，最终原生复跑干净完成；不将合成检查当作IME/读屏/物理DPI验证。

sRGB对比计算：浅色底线#808A99/填充#F5F6F8为3.23，深色#858C99/#292C32为4.14；焦点线/填充为5.77/6.91，次级文字/填充为5.11/7.03。此为所列组合计算，不是全状态或OS高对比模式验收。官方HIG已复核，P-13写入项目约束；自主数值与官方原则分列，未复制字体/图标资产。

原始日志和合成截图留在忽略的build。临时库QTemporaryDir，不访问真实用户任务库，不操作旧用户进程，不推送。macOS、Narrator/VoiceOver、真实睡眠/多屏、完整8h和公开许可仍UNVERIFIED。最终部署见下段。

## UI-13最终严格部署

源码f0392c6d49de70dae739a80938dde08c1234d469，Release重建/CTest17/17（28.62秒）。SDK隔离152阶段PASS：退出0、双窗口、warnings=false，无unfinished creation诊断。清单1464文件、EXE版本及Qt/schema元数据、完整生命周期模块来源审计、最终ZIP每个条目内容和SHA侧车检查全部通过。唯一系统运行库例外D3DCompiler_47，其余Qt/MinGW/QML来自包内；报告c5cf5936保留在忽略的build。

二进制ZIP44885877字节，SHA256 7829f453ad1ef69a15d5c7dea7acbfc5f7425458761bc9843ab1ffb0f3824597。源码ZIP468017字节，SHA256 578c7a88378a08a53e47115bd0be722cae716873a070eb01f13327df390e674e，源包敏感文件扫描0项。无用户库访问、不推送；独立干净机、macOS、真实辅助技术/多屏/8h及公开许可仍UNVERIFIED。当前包未另跑中文路径，不重复声称UI-12该项证据为本包新结果。

# UI-12整体精修（2026-10-23）

Windows11 x64、Qt6.10.2/MinGW13.1，schema3。Debug构建/CTest17/17（29.47秒）、Release17/17（29.90秒），两个QML lint通过。当前Windows原生152阶段退出0、双窗口、warnings=false，无unfinished creation诊断；另有系统clipboard重试提示，不计为QML错误。

截图查看Today默认/200%、统一详情宽窗/200%、浅深控件、日历宽窗深色、默认/200%小窗和Mini。Today上下文去掉重复标记、元信息合并、完成圆不再被按钮拉伸；原断言继续覆盖日期/草稿/菜单/长短备注和焦点路径。未把截图或合成事件当作物理DPI/读屏/IME/跨屏验收。

应用图标资源在合成启动时检查16/256像素pixmap非空。用Windows实际资源API读取Release EXE的RT_GROUP_ICON，8帧尺寸为16/20/24/32/48/64/128/256；ExtractAssociatedIcon提取32像素图像查看通过。ICNS头长及7个PNG块结构通过，macOS Finder/Dock实机UNVERIFIED。图标由仓库脚本和系统Drawing生成，未下载第三方资产或引入运行时依赖。脚本语法与实际生成通过。

完整图标/布局设计见[UI-12](design/ui-polish.md)。原始日志/截图保留在忽略的build目录，数据来自QTemporaryDir，无用户库访问。V-03W既有Release部署失败仍按原记录保留；UI修改不会自动解除发行门禁。

## UI-12最终本地交付 / V-03W复验

源码2c349320082d49af8f6792e99cc2a9562467b813；打包入口重建Release并运行17/17 CTest（28.04秒）。新候选155600：SDK路径隔离、完整生命周期DLL审计、1463文件清单、程序版本、最终ZIP逐条内容及SHA侧车全部PASS。Windows原生152阶段退出0、2窗口、warnings=false，无unfinished creation诊断。

再从最终ZIP解压到独立中文/空格目录，直接运行包内verification/Test-Package.ps1（不传PackageDirectory），再次PASS；调用前注入无效QT_PLUGIN_PATH哨兵，PATH和该变量均正确恢复。报告范围仍为本机SDK隔离，不是独立干净机。用户库访问标记false，license/cleanMachine验证标记false。

本地ZIP44852604字节，SHA256 cf08e97c44bbb2e58222d114e744a0a635dcbe211c40562e4301bb1bee39d96e。源码ZIP459625字节，SHA256 d104ba66ea2f3e0fe19ba94508836f17b8dcc6017e9ff30f831b400acaa1b73e，源包敏感扩展/构建/Git扫描0项。原始报告f4076fcb/68dd2a11保留在忽略的build内。

早期153556/123917/124722的退出诊断FAIL仍保留，不改写历史记录。当前候选两次通过不表示Qt根因普遍修复；完整8h、真实系统矩阵、独立干净机、公开许可、签名和macOS继续UNVERIFIED。

# UI-11及小窗置顶入口（2026-10-23）

Windows11 x64、Qt6.10.2/MinGW13.1，schema3。自动测试使用QTemporaryDir；未访问用户任务库、未结束已有用户进程。

- Debug构建/CTest17/17，28.39秒；Release构建/CTest17/17，28.49秒。应用与样式两个QML lint通过。
- Windows原生152阶段退出0、可见窗口2、warnings=false，本次Debug日志无unfinished creation诊断。短备注聚焦不移正文、滚轮交给外层、手动滚动不吸回、下拉框值不变、短列表不可滚动断言通过；默认/窄窗/200%、长短备注检查与截图通过所检区域。
- 展开小窗图钉为真实置顶状态绑定，原生探针260宽/200%点击往返及按钮边界检查通过；默认/200%小窗与Mini截图人工查看。Mini保留摘要/展开，继承置顶；未证明其他应用遮挡、真实触摸板或读屏体验。
- V-03W：清单/ZIP篡改、敏感文件、重复键/路径、版本、重解析路径与DLL来源守卫均在package_validation中通过。7b5658c的Release部署153556严格验证FAIL：原生完成但退出报告未完成QML创建6项，未生成最终ZIP。此前123213旧脚本检查通过的包同有退出诊断，后续严格检查正确拒绝123917/124722。等待incubator和补齐deferred创建未稳定解决，生产销毁顺序尝试已撤回；当前Debug干净退出不能覆盖Release失败。

原始日志与截图在忽略的build目录，失败部署目录留在dist仅供诊断。下一步定位生命周期并重新严格打包/中文路径验收；未把失败包当作交付。独立干净机、许可/签名、完整8h/真实睡眠/多屏、macOS仍UNVERIFIED。

# V-02W · Windows稳定性与恢复基础（2026-10-23）

Windows11 26H2 x64、Qt6.10.2、MinGW13.1、i7-13700HX/24逻辑核；版本仍1.0.0-rc.2、schema3。临时库均QTemporaryDir，无现有用户任务库访问。只实现并验证本切片；完整V-02仍有平台与时长门槛。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| Debug构建/CTest | PASS；16/16，26.61秒 | 含新增storage_integrity/system_recovery及维护并发回归 |
| Release构建/CTest | PASS；16/16，26.90秒 | 业务、导入备份、事件、模型与QML回归 |
| QML应用/样式lint | PASS；无警告 | 显式单一Flickable、后台表单状态、几何变化后的焦点追随 |
| Windows原生SystemRecoveryTest | PASS；6/6，1129ms | 1000请求合并、相关MSG、析构取消、几何持久化、隐藏/最小化及销毁安全；不是实际睡眠/拔屏 |
| Windows原生148阶段与截图 | PASS；退出0、双窗口、warnings=false | 长备注+维护、880/1440、200%、单正文与焦点可见、菜单无空行；截图人工查看默认和200%所检区域 |
| CLI错误参数 | PASS；4种均退出2 | 仅report、已有report、附data-dir、时长越界；原报告内容保留、未新建报告 |
| Debug连续短跑 | PASS；请求30秒，实跑33.997秒，3循环/3完整性检查 | 同进程，非重启多次；不等于8h |
| Release连续短跑 | PASS；请求60秒，实跑67.383秒，6循环/6完整性检查 | 最大事件循环延迟383ms、长暂停0；退出0 |
| Release 10k混合数据 | PASS；加载129ms、搜索1ms、月索引8ms | 20领域/200项目/50标签/30k清单/30%完成；数据层测量 |
| SDK隔离部署/归档 | PASS；原生148阶段退出0、双窗口、warnings=false | Qt/MinGW模块在包内；manifest1457文件逐一SHA与长度核对；二进制ZIP侧车一致，两个ZIP敏感扩展/日志/Git扫描0项 |

Release短跑包含UI启动的总墙钟95.348秒、47次采样；私有内存峰值508,616,704字节、工作集峰值496,939,008字节。CSV含逐次CPU，属于活跃合成负载，不是闲置基准；没有将单次峰值当作泄漏结论，也尚未证明150–250MB优化目标。二进制SHA256为4257179ec1937dc411d47b29f67b1ceb5c97e34053a88c21ec3679e0e2b36d41。Debug原始报告build/stability-20261023-120319-7ccb7b，Release为build/stability-20261023-120431-5e1dfd；原生恢复/性能/截图/CLI日志均留在忽略的build内，不提交设备路径或原始诊断。

早期短跑FAIL：以Hidden启动的首个Win32窗口没有正常呈现，长备注布局检查失败，尚未进入连续循环。修正原生探针为显式呈现再检查后，按原断言重跑通过，没有跳过焦点可见性或修改FAIL报告。详情同时补后台维护可编辑、显式正文尺寸及polish后几何追随；不把早期隐藏窗口结果直接推断为真实可见界面已损坏。

完整8h、真实睡眠/唤醒、物理多屏/DPI、午夜时区、实际通知送达、闲置CPU/长时内存趋势、独立干净机、macOS均UNVERIFIED。执行方法见[稳定性指南](stability-guide.md)。没有修改用户进程、启动项、真实库，没有GitHub上传或远程发布。

# UI-10 · 单一任务编辑面板验证（2026-10-23）

Windows11 x64、Qt6.10.2、MinGW13.1；schema仍3。所有自动测试/合成数据使用QTemporaryDir，没有访问用户任务库。

| 检查 | 结果 | 实际覆盖 |
|---|---|---|
| Debug构建/CTest | PASS，14/14，25.74秒 | 标题/备注/归属/标签/清单一份TaskPatch提交、仅推进一个版本；错误提醒/日期/标签、陈旧版本零修改、重启一致 |
| Release构建/CTest | PASS，14/14，25.57秒 | 同上；分组投影与原查询/导入备份/事件回归 |
| 应用/样式QML lint | PASS，无警告 | Pane、完整字段、固定section角色、菜单隐藏项高度 |
| Windows原生148阶段合成检查 | PASS，退出0、2窗口、warnings=false | 窄窗非模态主内容页、宽窗并排，未提交清单与标题一起保存；非法提醒保留全部草稿，取消/放弃导航不误写 |
| SDK隔离部署 | PASS | 原生148阶段退出0、2窗口、warnings=false；Qt/MinGW模块来自包内，ZIP敏感扩展/日志/Git扫描0项，SHA与侧车一致 |
| 长备注/大字/菜单 | PASS，所检布局 | 24行备注自然增高；面板仅1个可见Flickable，清单输入焦点可见；880/1440往返及200%，隐藏菜单项高度0、有效项连续无空行 |
| 文档与diff | PASS | 45份Markdown本地链接、git diff --check |

最终截图位于build/ui10-settled-screens：unified-fields/page/page-200/wide/error及task-menu。布局重排后等待渲染与16ms焦点调度稳定再检查；offscreen没有活动窗口焦点，仅显式验证滚动目标，Windows后端验证真实QQuick焦点。QMetaObject/QKeyEvent仍为合成输入，不替代物理鼠标键盘、IME、读屏与真实用户测试。

修复过程：原生曾在Qt Quick列表分组路径崩溃；改为模型提供固定viewSection角色、避免隐藏列表切视图时重设section.property，最终原生流程通过。另修正文高度变化后焦点调度过早、清单焦点目标短暂失活丢失、右侧滚动条定位。全部旧包和用户进程保留；macOS、读屏/IME、干净机、多屏/睡眠/8小时仍UNVERIFIED。SDK隔离部署及最终包以PROGRESS为准。

# UI-09 · 详情、日期与菜单精修验证（2026-10-23）

Windows11 x64 / Qt6.10.2 / MinGW13.1，版本仍1.0.0-rc.2、schema3。全部业务和UI探针使用QTemporaryDir，没有触碰真实任务库。

| 检查 | 结果 | 覆盖与边界 |
|---|---|---|
| Debug build / CTest | PASS，14/14，21.35秒 | 新增排序能力边界：首尾、搜索、筛选、Upcoming、Today跨组禁用；原数据/草稿回归 |
| Release build / CTest | PASS，14/14，21.53秒 | 同一套业务与QML启动回归 |
| 两个QML lint目标 | PASS，无警告 | DateField、主窗/事件/菜单和既有界面 |
| Windows原生132阶段合成检查 | PASS，退出0，2窗口，warnings=false | 日期展开保留文本焦点、QKeyEvent输入非法日期后打开日历不篡改输入、选择/清除只改草稿；改期表单保存只改开始日、截止不变 |
| 菜单与截图 | PASS，所检布局 | 260宽组件独立菜单窗口默认/200%文字可读，所有文字项目宽度断言；详情/Today、窄窗搜索及改期日历200%截图 |
| 文档与diff | PASS | 44份Markdown本地链接，git diff --check |
| SDK隔离部署 | PASS | Qt/MinGW运行库均来自包内，原生132阶段smoke退出0、2窗口、warnings=false；ZIP敏感扩展/日志/Git元数据扫描0项，SHA与侧车一致 |

截图为build/ui09-verified-screens下合成数据；默认/880×580/1440×900逻辑尺寸及浅深/200%沿用探针。当前截图PNG/逻辑尺寸比含1.25和1.5；不是物理多屏或全部DPI验收。窄窗大字日历可滚动，弹层避开日期输入框。修复两位日期被水平内边距省略、200%菜单勾选间距；菜单使用独立Popup.Window、明确文字测宽和右侧对齐，避免小组件范围压缩/裁切。

offscreen不验证OS文本焦点；该断言仅在windows后端执行。QKeyEvent/信号调用是合成操作，不能替代物理键盘、中文IME、Narrator/VoiceOver和真实用户验收。macOS、干净机、多屏/睡眠/8小时仍UNVERIFIED。部署与包信息最终以PROGRESS为准。

# CAL-01/02 · RC2验证（2026-10-23）

环境：Windows11 x64、Qt6.10.2、MinGW13.1；全部业务测试使用QTemporaryDir，截图为合成数据。新增FR-019及ADR-013，schema3。实际实现见RC2说明，状态与包信息只维护在PROGRESS。

| 检查 | 结果 | 实际覆盖 |
|---|---|---|
| Debug build / CTest | PASS，14/14，19.56秒 | 新增event_service及workspace事件/关系/恢复/重启用例 |
| Release build / CTest | PASS，14/14，20.51秒 | 同上，原任务/目录/日历/重复/提醒/备份回归继续通过 |
| 应用/样式两个QML lint目标 | PASS，无警告 | EventDialog、响应式议程及原组件 |
| Windows原生114阶段smoke | PASS，退出0，2窗口，warnings=false | 创建全天/定时事件并关联任务；事件表单保存到SQLite；议程按钮直接完成且事件不取消；事件脏草稿阻止主窗关闭、取消退出保留草稿 |
| PNG审查 | PASS（所检布局） | 宽窗议程、事件表单、已完成、880×580/200%月历与议程直达、200%固定表单底部、1440×900深色；证据在忽略目录build/calendar-final-screens |
| 关系及失败原子性 | PASS | 关联不存在任务、后续SQL触发器失败均回滚事件及原关系；CAS冲突拒绝；双方删除不级联；任务恢复重现关系；事件8秒撤销；仅添加导入跳过已有事件及其关系 |
| 时间语义 | PASS（服务与workspace） | 全天排他结束、午夜截止不进入下一日、跨日/时区投影、NY DST缺失/歧义拒绝、秒/毫秒编辑精度保留 |
| 数据兼容 | PASS | schema1/2先一致快照、备份失败阻止升级；003追加，旧任务保留；schema3 JSON事件/关联往返，旧1/2空事件兼容，损坏导入零修改 |
| 10k事件Release基准 | PASS，加载20ms，可见日期索引<1ms（整数记录0ms） | 合成10000个全天事件，同日，42日范围；不代表定时混合分布、全UI帧率或冷启动 |
| 文档链接/diff | PASS，43份Markdown | 本地链接和空白检查 |

首轮测试暴露的空备注NULL绑定、QML对象分隔符、动态日期项父项销毁时宽度访问已修复。ListView动态委托的点击检查改查可视树，实际调用按钮clicked并检查数据库结果。扩展全量PNG首次触发旧25秒smoke上限；改为45秒有界上限并增加阶段诊断后，单独运行全部截图通过。CTest上限60秒、部署等待55秒；超时仍按失败退出，不忽略错误。

上述属于方法/信号驱动及真实Qt窗口加载/渲染；没有执行物理键鼠、Narrator/VoiceOver、IME、真实时区切换/睡眠、多屏/DPI切换、8h、macOS或干净机，保持UNVERIFIED。事件不提供小时轴、事件重复/提醒、外部账户或云同步。

最终部署PASS：清除SDK PATH及Qt/QML搜索变量，验证Qt和编译器运行库均加载自包内；114阶段Release检查退出0、2窗口、warnings=false。二进制ZIP清单1453文件，重新计算的SHA与侧车一致；二进制/源码ZIP敏感扩展、日志、Git元数据扫描均0项。包名、大小、校验和和源码修订见PROGRESS及包内BUILD-INFO。部署工具仍提示缺少dxcompiler.dll/dxil.dll；当前原生加载通过不替代其他GPU与干净机验收。

---
# UI-08 组件降干扰验证（2026-10-23）

Windows11 x64 / Qt6.10.2 MinGW13.1，临时库与合成数据。关联FR-012/013、UX-01/04/09/11/13；FR-010/011本轮只做官方资料与现有代码评估，不宣称日历新能力已实现。

- Debug build/CTest：PASS，13/13，16.05秒；Release：PASS，13/13，16.14秒。
- daymark_style_qmllint、daymark_qmllint：PASS，无警告；41份Markdown本地链接与diff检查通过。
- Windows原生96阶段合成smoke：PASS，退出0、2窗口、warnings=false。新增260DIP浮窗浅/深及200%截图，收起按钮调用后进入Mini，展开图标调用后恢复Floating；断言按钮32～40DIP、列表至少120DIP可用高度、展开入口不越出48DIP迷你条。
- 截图审查：PASS（所检布局）；build/ui08-screens中的v1-widget、compact-widget、compact-widget-dark、compact-widget-200、compact-mini-200。340宽普通表头由两排大按钮变为一排低强调图标，任务阅读空间增加；200%保留字号，长摘要省略，操作入口仍可达。
- 本轮不改变背景alpha、整窗穿透或平台窗口策略；隐藏/打开主窗移入菜单，托盘与主窗穿透恢复保持原命令。真实鼠标/键盘/读屏、多屏/macOS仍UNVERIFIED，QML方法调用不作为完整OS操作证据。

评估及来源见[组件与日历评估](design/widget-calendar-review.md)。日历建议先做现有任务日历的议程与完成交互，再单独评估本地事件模型；尚未进行事件/关系迁移。

本地部署PASS：移除SDK与Qt/QML搜索变量，运行库路径均为包内；96阶段Release smoke退出0、2窗口、warnings=false。ZIP清单1451文件、敏感扩展/日志/Git扫描0项；包名、校验和及源码修订见PROGRESS，部署记录见包内BUILD-INFO。干净机与签名/公开再分发门禁仍未验收。

---
# UI-07 全界面视觉统一验证（2026-10-23）

Windows11 x64、Qt6.10.2、MinGW13.1；使用临时库与合成任务。关联FR-001～018展示层，UX-01/02/04/05/06/07/09/10/11/13。业务命令、草稿、schema与原生系统对话框保持既有契约。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| 最终Debug构建 / CTest | PASS，13/13，15.96秒 | 包含原业务/草稿/批量/迁移回归及扩展QML smoke |
| 最终Release构建 / CTest | PASS，13/13，16.24秒 | 同上 |
| daymark_style_qmllint / daymark_qmllint | PASS，无警告 | 自定义样式及应用两个目标 |
| Windows原生合成smoke | PASS，退出0，2窗口，warnings=false | 90阶段，真实Qt Quick窗口，QML方法/信号调用 |
| 控件行为 | PASS（方法/信号范围） | 下拉delegate点击改变currentIndex并关闭popup；勾选切换、数值增减；原有标准确认与草稿保护路径继续通过 |
| 合成截图审查 | PASS（所检场景） | 主窗、停靠/覆盖详情、组织/重复/设置/数据/日期/日历/批量/Floating/Mini；1120/880/1440 DIP、浅深色、默认/200%文字；菜单、下拉、焦点、禁用态 |
| 窄窗复杂表单 | PASS（布局范围） | 组织/重复在大字下上下排列，正文滚动；数据工具按钮换行；保存/返回固定底部。截图存忽略目录build/ui-unify-screens |

实际修复了首轮发现的TextField无效padding属性、DialogButtonBox高度循环、SpinBox无效indicator属性、QML对象分隔符、输入占位提示缺失，以及菜单宽度和管理表单固定双栏问题。Qt数值输入的键入提交依赖完整输入事件，手动调用editingFinished不能模拟该流程；本轮自动控件检查仅验证增减，不宣称物理键入通过。

macOS、物理鼠标/键盘完整操作链、中文IME、Narrator/VoiceOver、高对比、跨屏/DPI切换、8h与干净机仍UNVERIFIED。200%正文出现滚动裁切是可滚动区域边界，不将截图当作全字段或辅助技术验收。

最终本地部署PASS：清除SDK PATH与Qt/QML搜索变量，验证Qt和编译器运行库均加载自包内；Release合成smoke退出0、2窗口、warnings=false。部署工具仍提示未找到dxcompiler.dll/dxil.dll，本机原生启动通过不等于其他GPU或干净机验收。ZIP包含1450份清单文件，敏感扩展/日志/Git元数据扫描0项；包名、校验和与源码修订唯一记录在PROGRESS，包内BUILD-INFO记录部署证据。未修改旧标签或推送。

---
# 1.0.0-rc.1 Windows候选版验证（2026-10-22）

环境：Windows11 x64、Intel Core i7-13700HX、Qt6.10.2、MinGW13.1；源码与自动测试只使用合成数据及QTemporaryDir。用户确认先交付Windows候选版。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| 最终Windows Debug build / CTest | PASS，13/13，14.71秒 | 新增启动配置隔离测试及批量/归档/迁移/筛选回归 |
| 最终Windows Release build / CTest | PASS，13/13，14.40秒 | 同上 |
| daymark_qmllint | PASS，无警告 | QML静态检查 |
| Windows原生合成smoke | PASS，退出0，2窗口，warnings=false | 宽窄共享草稿、批量提交真实SQLite、20项列表、各视图位置恢复、折叠恢复、200%工具区/列表/导航、Floating/Mini切换 |
| 合成PNG视觉审查 | PASS（所检场景） | 1120/880/1440 DIP、浅深色、200%详情/设置/列表/导航、批量对话框、Floating/Mini；截图只存忽略的build/v1-screens |
| 批量事务 | PASS | 后续任务版本冲突、后续SQL触发器注入失败均回滚先前写入；成功后最新版本与模型一致 |
| schema1升级/归档/导入 | PASS | 迁移前快照、备份失败阻止升级、任务保留、归档/恢复、旧JSON缺少archived默认0、schema2往返 |
| 备份周期与启动设置 | PASS（隔离配置） | 周期7天跳过近期快照、每日生成新快照；启动登记仅测临时INI读写/引用/失败，不修改真实Run项 |
| 10k混合数据层Release基准 | PASS，加载155ms、搜索2ms、月索引11ms | 20领域、200项目、50标签、30k清单、30k标签关联、30%完成；不是进程冷启动/全UI帧率 |
| 文档链接与diff检查 | PASS，39份Markdown，无空白错误 | 本地链接，不替代外部平台验收 |

首次批量测试曾错误假设持续关注Today任务可直接设未来开始，后将底层事务测试改为Anytime数据，另加workspace测试验证批量未来开始同时取消持续关注。编译检查发现的QML处理器分隔和类型注册问题已修复。原生截图发现窄窗200%工具区/侧栏挤占或遮蔽入口后，改成可滚动区域并增加尺寸与恢复回归；不隐藏这些初次失败。

本轮未运行macOS构建、真实系统登录启动、真实输入法/读屏/高对比、物理多屏/睡眠/时区、8h或独立干净机；均保持UNVERIFIED。Windows事件适配只实现了广播入口，不能把代码存在或方法调用当作真实广播验收。候选包的SDK隔离部署结论由包内BUILD-INFO.json及下方交付记录承载。

最终部署PASS：移除Qt/MinGW PATH及Qt/QML插件搜索变量，验证加载的Qt/编译器运行库均在包内；完整smoke退出0、2窗口、QML warnings=false。ZIP 44,182,548字节，清单1449文件，包名Daymark-1.0.0-rc.1-windows-x64-local-20261022-201647.zip；SHA256为418a88f85a6a4436c2611d1c6f777396a8aa3d2e65782077087da8dc55cf1839。ZIP未发现数据库、私钥、原始日志或Git元数据。首次打包在隔离PATH后调用Git失败，已把读取修订提前到隔离前，重新部署和压缩通过。

---

# UI-03A响应式详情验证（2026-10-22，历史切片）

本轮源码在v0.1本地交付后新增响应式详情；下方原v0.1包记录保持历史范围。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| Windows Debug build / CTest | PASS，12/12，9.60秒 | QTemporaryDir隔离数据库 |
| Windows Release build / CTest | PASS，12/12，9.41秒 | 同上 |
| daymark_qmllint | PASS，无警告 | QML静态检查 |
| 草稿/导航集成smoke | PASS | 宽窄往返保留草稿、任务/视图切换取消与放弃、保存后导航、保存后完成使用最新rowVersion；既有非法日期失败保留仍通过 |
| Windows原生后端smoke及截图 | PASS，退出0，2窗口，warnings=false | 1120×720、880×580、1440×900 DIP，默认和200%文字；原生渲染，动作由QML方法触发 |

截图为合成数据，保存在忽略的build/responsive-screens；检查了宽窗列表与详情预留空间、窄窗覆盖、按钮换行及可滚动内容。真实鼠标/焦点、中文输入法、Narrator/VoiceOver、macOS、多屏仍UNVERIFIED；不以程序触发信号替代人工验收。没有改schema或历史migration。

首次部署验证发现隐藏窗口未渲染时读取了旧布局尺寸；smoke现先触发渲染再断言布局，最终Debug/Release回归通过。另一次部署在固定500ms取模块时尚未加载运行库；打包检查改为最多5秒的有界等待，并在失败时回收探针进程。后续隔离SDK环境的部署smoke退出0、2窗口、无QML警告；未把上述初次失败当作成功。

---

# v0.1本地版验证（2026-10-22，历史交付）

环境：Windows11 x64，Qt6.10.2，MinGW13.1，CMake3.29.2，Ninja1.12。以下为v0.1原始本地包，后文保留更早历史证据。

| 检查 | 实际结果 | 边界 |
|---|---|---|
| 最终Debug build / CTest | PASS，12/12，12.01秒 | 临时SQLite/服务/模型/单实例/窗口/QML |
| 最终Release build / CTest | PASS，12/12，8.04秒 | 同上 |
| Debug daymark_qmllint | PASS，无警告 | 属性/类型/布局静态检查 |
| Windows原生窗口测试 | PASS，8项含init/cleanup，0失败/错误，1后端特定跳过 | Qt状态；不证明物理拖动/系统层级 |
| 原生扩展QML与合成截图 | PASS，退出0，2窗口，warnings=false | 月/周/详情/组织/规则/设置/数据/日期/深色/200%详情 |
| 10k Release数据层基准 | PASS，加载38ms、搜索1ms、月索引11ms | 当前机器、真实临时SQLite；不是全UI帧率或干净机指标 |
| 文案对比计算 | 浅色正文14.97、次级5.52、选中12.96、强调6.24、危险6.54；深色14.50/8.09/10.29/7.95/9.48 | 相邻content/selected表面；不覆盖所有控件或透明桌面 |

用例覆盖TC-001～020主要数据流程、TC-021～025日历边界、TC-026同源模型、TC-030单实例、TC-031～039重复提醒与恢复、TC-045数据层基准。真实桌面TC-027～029/041、8h TC-042及系统事件TC-043/044不能因纯函数/状态测试而标PASS。版本化SQL错误、项目迁出失败注入、无效JSON零修改、重复ID/日期/外键、提醒认领一次和重启持久化均由真实临时库验证。

源码不含用户数据库、私人任务、原始机器日志或SDK。截图/log只在忽略的build目录。Qt许可服务版本诊断仍存在，未绕过检查；可选Vulkan头缺失，本轮不验证该后端。完整未验证项见平台矩阵与发行说明。用户已要求先交付本地，不推送或创建GitHub Release。

最终本地部署PASS：去除Qt/MinGW PATH及Qt/QML插件搜索环境，检查加载模块均来自包目录；原生smoke退出0、双窗口、无QML警告。包1443个文件，ZIP 44,074,547字节，SHA256为4111e1cefb9f2bf3ce1855120c9dcb1ba6647b9205bec41fe31437e22e703f24。包名Daymark-0.1.0-windows-x64-local-20261022-192938.zip。ZIP检查未发现数据库、私钥或原始日志。本机路径隔离不等于独立干净机验收。

---

## 历史记录（旧功能范围，仅保留证据）

# 验证记录

## UX-RESEARCH-01 UI/UX目标（2026-10-22）

本轮只改文档与开发约束，不改运行源码或数据库。读取Cultured Code官方产品/支持页面、Apple HIG及其页面提供的同域Markdown正文；来源、年代和访问日期见design/ui-research.md。没有安装Things或测量其像素，未获取专有产品资产；没有声称已验证实际Things客户端交互或效率。

| 检查 | 实际结果 | 限制 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | 当前Windows Debug基线，无运行源码变更 |
| scripts/test-windows.ps1 | PASS，6/6，3.91秒 | 既有基本功能回归，不验证新UI目标 |
| scripts/check-docs.ps1 | PASS，30份Markdown本地链接有效 | 外部来源另外阅读，不由脚本验证 |
| 官方资料→目标→UX验收追踪 | PASS（文档审查） | 来源事实与Daymark自主token/阈值已区分，历史截图已标注 |

新UI目标是待实施设计：颜色候选尚未逐对实际对比计算；布局断点、文字缩放、读屏、真实用户动作预算与双平台实现均待后续UI-01～06验证。本轮不将文档完成当作界面达标，不重复声称macOS或窗口人工测试通过。

## BASIC-01 基础任务闭环（2026-10-22）

Windows11 x64、Qt6.10.2、GCC13.1，使用本机已有Kit，无新依赖。当前功能范围见 [设计](design/basic-tasks.md) 和 [任务记录](tasks/BASIC-01.md)，不是完整V1验收。

| 命令/检查 | 实际结果 | 边界 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | Debug源码、SQL资源和QML编译链接 |
| scripts/test-windows.ps1 | PASS，6/6，4.89秒 | SQLite、Task服务、异步模型、进程单实例、窗口、完整QML流程 |
| scripts/build-windows.ps1 -Preset windows-release | PASS | Release编译链接 |
| scripts/test-windows.ps1 -Preset windows-release | PASS，6/6，4.05秒 | 同上，非性能基准 |
| cmake --build --preset windows-debug --target daymark_qmllint | PASS，无警告 | Bound delegate及命名空间模型类型已明确 |
| scripts/test-window-native.ps1 -Preset windows-release | PASS | Windows窗口Qt状态和完整合成任务QML流程；不代表实际鼠标/焦点验收 |
| daymark --smoke-test --screenshot <build内PNG> | PASS，退出0，两窗口，无QML警告 | QTemporaryDir、真实Windows后端、合成数据 |
| 主窗/窄窗详情PNG视觉检查 | PASS | 1120×720及880×580 DIP；当前150%渲染下主操作、中文和日期错误提示无截断 |
| 追加WAL非空断言后，分别构建并运行Debug/Release task_service | PASS，1/1及1/1 | 确认未checkpoint内容被快照保留 |
| scripts/check-docs.ps1及共享文件审查 | PASS，26份Markdown、33个源/测试/脚本无尾部空白 | 未检出本机绝对路径/用户目录/许可绕过变量，构建截图与日志被Git忽略 |

QML流程实际触发快速输入accepted和详情保存：新建同步Today→非法日期拒绝且草稿保留→成功编辑→完成至Logbook→恢复→软删/撤销→脏草稿阻止主窗关闭→取消离开保留编辑→明确放弃而不改数据库。自动测试不使用生产AppData目录。

服务测试覆盖20条连续录入、512个补充平面Unicode标量边界、三态patch、日期/版本冲突零修改、未来focus/Someday优先级、完成/取消/恢复、项目JOIN隐藏恢复、只读写入失败、过新/未知库不覆盖、WAL快照内容一致。WAL快照用例关闭自动checkpoint并确认-wal非空，快照回读逐字段一致；备份失败不改任务。

异步模型以QAbstractItemModelTester验证增量信号、两个模型同步、失败不改缓存、退出等待已接受写入。单实例用真实第二进程和含空格临时目录验证激活协议，第二实例不创建任务库。

修复记录：QML可默认构造类型的final限制导致初编失败，去掉注册类型final后通过；Shortcut多绑定告警改为sequences，模型指针使用完整命名空间，delegate采用Bound；Windows offscreen字体目录来自本机Windows Fonts，仅用于测试，无下载字体。smoke stderr诊断可读，未吞启动错误。

截图和原始日志仅保留在忽略的build目录，截图均为合成数据。未验证：真实OS鼠标拖动/窗口层级/焦点、完整多屏DPI、系统午夜/睡眠事件、macOS、10k性能、8h稳定性和干净机部署。构建仍输出既有许可服务/可选Vulkan诊断，未修改系统配置。未提交或推送Git。

## W-01 窗口控制（2026-10-22）

环境沿用Windows11 x64 / Qt6.10.2 / MinGW13.1。实现和自动验证通过；真实拖动、跨应用窗口遮挡、焦点、DPI和macOS尚未人工验证，任务保持VERIFY。

| 命令 | 最终结果 | 范围 |
|---|---|---|
| scripts/build-windows.ps1 | PASS | Debug增量构建 |
| scripts/test-windows.ps1 | PASS，3/3，1.46秒 | SQLite基线、widget_window、QML启动 |
| scripts/build-windows.ps1 -Preset windows-release | PASS | Release增量构建 |
| scripts/test-windows.ps1 -Preset windows-release | PASS，3/3，0.66秒 | 同上；不是性能基准 |
| cmake --build --preset windows-debug --target daymark_qmllint | PASS | QML静态类型/属性检查 |
| scripts/test-window-native.ps1 -Preset windows-release | PASS，QWindow失败0、错误0、跳过1，QML启动退出0 | 4个行为用例通过；另有init/cleanup；仅offscreen守卫在windows后端跳过 |
| Windows原生QML启动探针 | PASS，退出0 | 真实后端创建双窗口，无QML警告 |
| scripts/check-docs.ps1 | PASS，22份Markdown | 本地链接 |

widget_window验证：模式往返保留尺寸位置、隐藏时设置不误显示、flags保留Tool/Frameless、解除transient owner、可见性/close同步、失败不提交新模式/置顶、恢复模式失败仍可重试、销毁后安全失败、成功无操作清除旧错误。失败注入只验证命令契约，不当作OS能力成功；Qt接受置顶flag也不代表桌面遮挡测试通过。

初次编译因QML自动注册生成的头文件查找路径不足失败，补齐src/models包含路径后复测成功。原生测试命令初次用了不支持的junit格式名称，按实际Qt Test支持改为junitxml后通过；可复现入口已加入scripts/test-window-native.ps1。原始日志与JUnit（可能包含设备名）仅在忽略的build目录，不提交。

人工步骤见 [W-01任务](tasks/W-01.md)，平台边界见 [矩阵](platform-test-matrix.md)。无任务库、迁移或用户数据写入。

## 2026-10-22 工程基线

环境见 [environment](environment.md)，执行范围F-01/F-02；没有任务Repository、MigrationManager、托盘或真实穿透功能验收。

| 命令/检查 | 实际结果 | 结论边界 |
|---|---|---|
| scripts/build-windows.ps1 | PASS；Debug配置与29步构建成功 | Qt6.10.2/GCC13.1 |
| scripts/test-windows.ps1 | PASS；2/2 CTest，总耗时4.21秒 | 见下方测试职责 |
| scripts/build-windows.ps1 -Preset windows-release | PASS；Release配置与29步构建成功 | 未部署运行库 |
| scripts/test-windows.ps1 -Preset windows-release | PASS；2/2 CTest，总耗时0.60秒 | 非性能基准 |
| Release daymark --smoke-test，清除QT_QPA_PLATFORM后启动 | PASS；进程退出码0 | 原生启动探针，无视觉/交互验收 |
| scripts/check-docs.ps1 | PASS；21份Markdown本地链接有效 | 包含根目录和docs，不验证外部网站 |
| 共享源码/脚本/文档路径与隐私审查 | PASS；未检出本机盘符绝对路径、用户目录和许可绕过变量 | 构建原始日志仅留忽略的build目录 |

sqlite_runtime：QTemporaryDir隔离；真实QSQLITE文件库切WAL，写入中文任务样本文本，事务内写第二条并回滚，关闭重开后仅第一条保留。它验证Kit能力，不验证尚未交付的应用schema、备份或Repository。

qml_startup：真实应用加载Daymark模块，offscreen运行，检查恰好两个可见且尺寸非零的QQuickWindow，无QQmlEngine警告；250ms后自动退出。正常业务启动没有这一退出计时器；无业务数据库访问。

构建诊断：缺WrapVulkanHeaders；moc打印许可服务版本不匹配信息，但命令仍成功、产物链接成功且测试通过。未注册/修改本机许可服务，未设置绕过检查变量。Kit许可与分发核查保持待办。

原始日志保留在本机构建目录（不提交），当前记录仅保留可共享结果。macOS、透明视觉、DPI、穿透恢复、窗口焦点、通知、10k性能和部署均UNVERIFIED，详见 [平台矩阵](platform-test-matrix.md)。
