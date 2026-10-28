# ADR-027：CI 使用开源 Qt（LGPLv3）与依赖合规

日期：2026-10-28。状态：已接受：用户要求「使用开源 Qt，保证依赖合规」。

## 背景

ADR-007 固定 Qt 6.10.2 并假定开发者自备 Kit；此前各轮记录把「Qt 再分发许可未核实」作为只发布源码的理由，二进制包只做本地预览、不公开上传。该待办长期悬置。

GitHub Actions 的 `desktop` 作业此前使用 `install-qt-action` 安装 6.10.2，但**从未通过**：Windows 在 Build 阶段因 MSVC 的 `C1061: blocks nested too deeply` 崩溃（`AppRuntime.cpp` 的单条 `if/else if` 链达 376 分支，MSVC 上限为 122），macOS 在 Test 阶段以退出码 8 失败。此前 CI 未通过不只是许可证问题，构建与测试门禁本身也不成立。

## 决策

1. **CI 只使用 Qt 官方开源归档**，由 `jurplel/install-qt-action`（MIT）调用 aqtinstall 的默认 `opensource` 用户通道下载。该通道只提供社区版（LGPLv3/GPLv3）包；工作流中显式注释许可依据，并在构建日志里回显 Kit 路径与 `LICENSES` 目录内容，便于审计。
2. **按 LGPLv3 动态链接使用 Qt**：应用不修改 Qt 源码、不静态链接 Qt 库，用户可替换 Qt 共享库。发布二进制时须随包提供 LGPLv3 文本、版权声明与 Qt 源码获取方式（见 `docs/third-party-notices.md`）。
3. **不引入额外的运行时第三方依赖**。CI 仅新增 Actions（`actions/checkout` MIT、`install-qt-action` MIT、`aqtinstall` MIT）与测试期工具，均固定到已核对的提交或用例版本。
4. **合规校验进入流水线**：`source-policy` 作业继续执行发布守卫；`third-party-notices.md` 记录许可选择、版本与再分发条件；不再保留「许可未知即跳过验证」作为 CI 失败的借口。

## 后果

- 公开分发的二进制必须由本 ADR 的开源（LGPLv3）Kit 构建；开发者自备的其他 Qt 安装仅用于本机开发与验证，不作为发布构建来源，其版本与许可配置不进入仓库。
- 放宽 Qt 版本或更换许可证通道须更新本 ADR 并复跑双平台矩阵。
- 二进制发布仍需签名、干净机与许可文件随包等门禁，本 ADR 只解决「构建与 CI 使用开源 Qt」这一环。
