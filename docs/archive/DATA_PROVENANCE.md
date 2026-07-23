# [归档] 数据来源、转换与归档

本文冻结不再采用“所有图都换成最新官方版本”的统一策略。为了最大化与最近直接相关工作的可比性，主实验 1 使用 MonoGST+ 与 GPU4GST 实际发布/提供的处理后图和原始查询；只有没有历史作者图的 IMDb 敏感性实验使用最新官方冻结。图身份由来源、转换过程与最终 `graph.txt` SHA-256 共同决定，同名不表示同图。

## 1. 主实验 1：原论文完整 workload

### MonoGST+

使用 *A Practical Sublinear Approximation for Group Steiner Tree* 作者实验中的五套完整接口：Toronto、MovieLens、DBLP、LinkedMDB、DBpedia。图和 `query.txt` 均保持作者版本，不从当前官方站点重新下载或重生成。

| 正式身份 | 图目录 | 查询 | 上游角色 |
|---|---|---:|---|
| `Toronto-MonoGSTPlus` | `data/Toronto` | 160 | ImprovAPP 图与受控查询 |
| `MovieLens-MonoGSTPlus` | `data/MovieLens` | 160 | ImprovAPP 图与受控查询 |
| `DBLP-MonoGSTPlus` | `data/DBLP` | 160 | ImprovAPP 图与受控查询 |
| `LinkedMDB-MonoGSTPlus` | `data/LinkedMDB` | 200 | KeyKG+/WikiMovies 自然查询 |
| `DBpedia-MonoGSTPlus` | `data/DBpedia` | 438 | KeyKG+/DBpedia-Entity v2 自然查询 |

MonoGST+ 已被 VLDB 2026 接收但尚未公开，当前手稿及大文件只供内部研究。公开 artifact 前必须取得作者许可，或提供作者认可的获取步骤与哈希，不能默认再分发。

### GPU4GST

使用作者 OneDrive/GitHub artifact 中的八张处理后图、候选组和查询 CSV：Musae、Twitch、Github、Youtube、DBLP、Orkut、LiveJournal、Reddit。转换器只完成格式适配：

1. 从作者 `.in` 读取无向加权边，保留顶点编号和整数权重；
2. 从 `.g` 展开候选组；
3. 把 CSV 中 0-based 候选组行号转换成求解器需要的显式顶点组；
4. 对 `g=3,5,7` 各保留论文实际使用的前 300 行，不抽样、不按运行结果筛选。

详细编号、边权与作者成品差异见 [`GPU4GST_DATA.md`](GPU4GST_DATA.md) 和 [`data_origin/README.md`](../../data_origin/README.md)。作者原始文件的 OneDrive 元数据与 SHA-256 分别在 `data_origin/OFFICIAL_ONEDRIVE_MANIFEST.json` 和 `data_origin/SHA256SUMS.txt`。

主实验 1 共 13 个图身份、29 个可执行 query blocks、8,318 条查询。唯一机器真值为 [`experiment_data/p1_published_workloads/manifest.json`](../../experiment_data/p1_published_workloads/manifest.json)，其中固定最终图/查询哈希、点边数和查询分布。

两个 DBLP 必须分开。虽然当前接口实测点边数相同，`DBLP-MonoGSTPlus` 与 `DBLP-GPU4GST` 的图哈希分别为 `f3f60f5b…e5e0e9` 和 `841ddb99…629a82e`，因此不能合并结果或互换查询。

## 2. 主实验 2：GPU4GST 图上的跨 `g` 扩展

P2 复用 GPU4GST 作者图与 `.g` 候选组，只新增 `g=5..16` 查询。生成语义来自 GPU4GST 所沿用的 related-group 方法：在候选组共现图上选根组并 BFS 到足够深度，再抽取相关组；没有共同原图连通分量的查询被拒绝。

每个 `(graph,g)` 使用生成 seed `2025` 固定产生 300 条候选，然后仅依据输入组的 `log1p` 平均大小秩分成五层，每层用 panel seed `20260723` 派生的稳定 SHA-256 key 选一条。选择过程不读取任何求解器时间、内存、目标值或完成状态。正式六图为 Musae、Twitch、Youtube、DBLP-GPU4GST、Orkut、Reddit，共 72 格、360 条。

- 原始生成器：`tools/gpu4gst_data/prepare_gpu4gst.cpp`
- 固定选择器：`tools/data/build_gpu_query_panels.py`
- 输出清单：`experiment_data/p2_cross_g/cells.json`

这些查询是本文扩展，不得称为 GPU4GST 作者原始查询；作者原查询仅指 P1 的 `g=3,5,7` CSV。

## 3. 副实验：DBLP 与 IMDb 的 `<g,f>`

DBLP 沿用 `DBLP-MonoGSTPlus` 作者图。IMDb 没有可获得的 PrunedDP++ 历史图，因此使用 2026-07-22 从 IMDb 官方 non-commercial daily export 冻结的 title–person 二部图：title 和 person 各为顶点，`title.principals` 关系形成权重为 1 的无向边，顶点重新映射为稠密 1-based ID。

IMDb 的三个原始压缩文件、大小和 SHA-256 固定在：

- `experiments/official_sources.json`
- `data_sources/official/official-latest-20260722/download_manifest.json`
- `data/official-latest-20260722/imdb-20260722/dataset_manifest.json`

IMDb 的 mutable `latest` URL 不能在同一 freeze ID 下重新下载替换。新下载必须取新数据集名并完整重跑该实验。IMDb 数据受 non-commercial 条款约束，不进入公开 Git 仓库。

查询严格采用 MonoGST+ 协议。对每一组独立采样并四舍五入

```text
|S_i| ~ N(f, (0.15f)^2), clamp to [0.5f, 1.5f]
```

然后在 `1..n` 中组内无放回均匀抽取顶点；不同组可重叠。生成器不把一条查询的实际均值强制调整为恰好 `f`，manifest 同时记录目标值和真实 `mean/min/max`。网格为 `g={6,10,14}`、`f={200,400,800,1600,3200}`、每格 5 条，共 150 条。

## 4. 查询可行性与完整性

共同连通分量审计覆盖当前矩阵中的每个图/查询对。P2、受控副实验和 correctness gates 必须全部可行。P1 的原作者自然查询中存在 55 条无可行树：LinkedMDB 46 条、DBpedia 9 条。因为 P1 承诺“全询问”，这些查询保留原索引并要求三种精确方法一致返回 `infeasible`，不能删除、替换或从总时间分母中排除。

审计结果固定在 [`experiments/query_feasibility_audit.json`](../../experiments/query_feasibility_audit.json)。改变图、查询、矩阵或生成器后必须重建审计。

## 5. 旧数据归档与恢复

重设计前由 `.gitignore` 排除的主要数据已在清理前完整打包为仓库根目录的：

```text
ABHSS_ignored_data_before_workload_redesign_20260723.tar.gz
```

- 字节数：`27,761,456,863`
- 大小：约 `25.855 GiB`
- 条目数：`7,420`
- SHA-256：`eb5fb78483a7d9a1ae701e2562f11e54ee9478d3770c750fa471f8d147fdbd04`
- 打包范围：`data/`、`data_origin/`、`data_sources/`、`experiment_data/`、`docs/papers/`、`results/`

归档已通过完整列目录校验。它本身继续由 Git 忽略，不会上传。恢复到当前目录可运行：

```powershell
tar -xzf ABHSS_ignored_data_before_workload_redesign_20260723.tar.gz
```

清理后只从归档恢复当前 P1/P2/S2 和 correctness gate 必需的输入；旧官方全图快照、历史 panel、探针结果与本地论文 PDF 不再作为活跃仓库内容。

## 6. 投稿与公开检查

- 论文表格同时写正式身份、来源和最终图哈希；不要只写 “DBLP”。
- P1 明确称为作者 published workload reproduction；P2 明确称为本文生成的跨 `g` 扩展。
- P1 全部查询都必须运行；不能根据难度或结果筛选。
- S2 报告目标 `f` 与实际组大小，不声称复刻 PrunedDP++ 的历史数值。
- 公开前逐项核验 MonoGST+、GPU4GST、IMDb 和第三方源码的再分发许可。
- 本地归档含未必允许公开的数据与论文，只作恢复备份，不随 GitHub artifact 发布。
