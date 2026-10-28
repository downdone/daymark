# Daymark 文档梳理与重构方案（正式发布准备）

日期：2026-10-28。状态：方案，待执行。原则：**禁止逻辑修改，保留原意**；仅做文档梳理与重构。

## 0. 现状（实测）

| 指标 | 实测值 |
|---|---|
| Markdown 文件 | 137（1.31 MB） |
| 顶层 `docs/*.md` | 27 |
| `docs/design/*.md` | 73（含 sync-modules 8、sync-contracts 2） |
| `docs/ADR/` | 29（22 个决策 + 索引；ADR-001～006 只存在于 02 §1） |
| `docs/PROGRESS.md` | 954 行 / 223 KB，76 条按时间倒序追加 |
| `docs/verification-report.md` | 584 行 / 84 KB，43 段追加，最新止于 2026-10-25 |
| 根目录 LICENSE / CONTRIBUTING / CHANGELOG | **全部缺失** |
| 全量测试数 | 25 个 CTest；文档中并存 25/23/22/24/17/16/21/12 等口径 |

## 1. 目标结构

```
README.md                 项目门面：是什么、怎么跑、许可、贡献
LICENSE                   LGPL-3.0（用户已选定）
CONTRIBUTING.md           构建、测试、提交、临时库纪律
SECURITY.md               漏洞报告口径（离线、数据不上传）
docs/
  README.md               文档索引（唯一入口：当前 → 设计 → 过程 → 归档）
  product/                01/02/03 归位（保留原文，标注状态）
  design/                 存活契约（UI/日历/任务/同步），按簇分组
  adr/                    决策记录（补齐索引，标注 superseded）
  process/                工程方法、测试、发布、环境
  operations/             交付、稳定性、平台矩阵、用户指南
  archive/                PROGRESS 历史、verification-report、已完成切片、旧发行说明
```

## 2. 动作清单

### A. 立即修（低风险、纯纠正）
1. **补齐 `LICENSE`**（LGPL-3.0 全文），在 README 增加许可章节与 SPDX 标识。
2. **`docs/ADR/README.md`**：补 ADR-024、ADR-025；把 020～028 的列表格式统一回表格；为每行加「状态」列（已接受 / 被取代 / 提案）。
3. **`docs/environment.md`**：修正 `CTest共6项`（现 25）、`初始迁移001`（现 schema9）、「自动每日备份仍属后续」等过时陈述；删除商业许可服务诊断建议。
4. **`docs/user-guide.md`**：标题 1.0 → 1.1.0；修正「没有小时轴」（实际已实现）；统一设置分组为「四类」；合并两处重复的分类/标签小节。
5. **`docs/release-notes-v1.1.0.md:26`**：把「Qt 再分发许可仍待确认」改为 LGPLv3 现状。

### B. 合并重复
6. **`ui-baseline.md` / `ui-acceptance.md`**：7 段逐字重复。保留 baseline 为设计契约，acceptance 改为「指针 + 仅验收差异」。
7. **同步设计 3 处重叠**（`account-and-sync-service` §4/7/9、`server-architecture` §5-7/9、`api/sync-v1` §1-4）：以 `api/sync-v1.md` 为协议唯一事实源，另两份只引用。
8. **`sync-modules/README.md` 与 `sync-contracts/README.md`**：二者都自称开发入口，合并为一个入口，另一个变成索引。
9. **`calendar-creation-and-density.md:3` == `calendar-workspace-v2.md:3`**、**`task-list-context.md:3` == `unified-task-system.md:3`**：删除逐字副本，改为交叉引用。

### C. 拆分超大文件
10. **`PROGRESS.md`** → `docs/PROGRESS.md`（当前状态，≤1 屏：版本、schema、CI 状态、已知 FAIL）+ `docs/archive/progress/`（按年份/月份或按 ID 分文件）。
11. **`verification-report.md`** → `docs/archive/verification-report.md`（已止于 10-25，纯档案）；当前验证结论并入 PROGRESS 当前状态。
12. **`02_TECHNICAL_DESIGN.md`**：先把 §1 的 ADR-001～006 原文迁入 `docs/adr/`（001～006 文件），再把 02 整体移入 `docs/product/`。
13. **`docs/README.md`**：从「按时间倒序的 60 段散文」重写为「当前入口 → 存活设计索引（按簇）→ 过程文档 → 归档」。

### D. 归档（不删除内容，只移位并加 superseded 标注）
14. 自述或事实已过时的设计：`visual-system.md`、`unified-task-editor.md`、`responsive-details.md`、`event-series.md`、`desktop-calendar.md`、`v1-candidate.md`、`ui.md`、`review-rc2.md`。
15. 已完成切片的旧设计（UX-01～UI-33 一大批）移入 `docs/archive/design/`，并在索引标注「历史契约，现行为以 UX-48+ 为准」。
16. 旧发行说明（v0.1、rc、rc2、1.0、1.0.1）移入 `docs/archive/releases/`；`release-notes-v1.0.2.md` 从未打 tag，单独标注。

### E. 删除（确认无引用后）
17. `START_CODEX_PROMPT.md`（Codex 时代产物，AGENTS.md 已替代）。
18. `docs/design/code-structure-roadmap.md`（自述「提案，未实施」，未被任何导航引用）——或移入归档保留。
19. `docs/v1-feature-matrix.md`（与 `release-notes-v1-rc.md` 重复的 schema1–4 时代清单）。

### F. 新增（发布必需）
20. `CONTRIBUTING.md`、`SECURITY.md`；`.github/ISSUE_TEMPLATE/`。
21. `CHANGELOG.md` 或 `docs/operations/releases.md` 版本索引。
22. `docs/design/README.md`（设计索引），让 73 个文件可按簇检索而非靠猜 ID。

## 3. 保留原则

- 所有**需求 ID、断言、证据数字、SHA、文件名**原样保留。
- 「以 PROGRESS 为准」这类指针改为指向新的当前状态页。
- 每个被移动/取代的文件保留原文，并在首行加 `> 状态：已归档 / 已被 XXX 取代（YYYY-MM-DD）`。
- 逻辑、产品行为、代码一律不动。

## 4. 验证

每批改动后运行：`scripts/check-docs.ps1`（链接）、`python -m unittest discover -s tests -p test_release_metadata.py`、`python scripts/publish-source-release.py --check-current`。
