# 数据来源、身份与统一格式

更新时间：2026-07-21。本文件是投稿数据表和 artifact README 的唯一来源；完整逐文件哈希见 [`experiments/data_snapshot.json`](../experiments/data_snapshot.json)，同名图判定见 [`experiments/graph_identity_audit.json`](../experiments/graph_identity_audit.json)。

## 1. 冻结原则

1. **图身份与询问身份分离。** 同一加权图可以承载自然询问、作者原询问和受控询问；每套询问必须有独立名称、manifest、生成协议和 seed。
2. **同名不等于同图。** 只有顶点数、边数、排序后的无向端点多重集和每条边权全部一致，才视为同一图；冻结文件再以 SHA-256 标识。
3. **不同图绝不迁移询问。** 即使两份图具有相同 `n,m` 或来自同一网站，也不假设顶点 ID、拓扑、边权或快照一致。
4. **原始面板不改写，扩展面板另建目录。** `data/` 保存作者/论文交付的标准化图和原询问；`experiment_data/` 只保存确定性派生视图。
5. **选择与算法运行时间独立。** 所有抽样在计时前按固定 seed 和哈希完成，不能根据 ABHSS 或 baseline 的结果换询问。

## 2. 本文统一输入格式

### 2.1 图

每个图目录包含 `graph.txt`：

```text
n m
u_1 v_1 w_1
...
u_m v_m w_m
```

- 顶点为 `1..n`；
- 每条无向边只写一次；允许重边，不允许负权；
- 权重以 `double` 读取，GPU4GST 作者 CPU artifact 仅接受非负整数权；
- `m` 必须等于后续边行数。

### 2.2 询问

每个询问文件采用：

```text
q
g
f_1 vertex_1 ... vertex_f1
...
f_g ...
```

`q` 为询问数，`g` 为该询问的组数，后续每行是一个非空顶点组。组可重叠；组内顶点去重且必须在 `1..n`。本文把某条询问的实际平均组大小定义为

```text
f_real = (f_1 + ... + f_g) / g.
```

只有生成器显式约束均值时才使用 `f_target`；自然/相关组面板只报告 `f_real`，不得把它们标成受控 `f` 实验。

## 3. 数据族与论文名称

| 论文 ID | `|V|` | `|E|` | 图的直接来源 | 询问来源与角色 |
| --- | ---: | ---: | --- | --- |
| Toronto | 46,073 | 68,353 | *Finding Group Steiner Trees in Graphs with both Vertex and Edge Weights* 作者包 | 原文 160 条 `(g,f)` 半合成询问；另有本文受控扩展 |
| MovieLens | 62,423 | 35,323,774 | 同上 | 同上 |
| DBLP-VEW21 | 2,497,782 | 12,786,329 | 同上 | 同上；禁止使用 GPU25 的组/询问 |
| LinkedMDB | 1,326,784 | 2,132,796 | *Keyword Search over Knowledge Graphs via Static and Dynamic Hub Labelings* 数据处理链，经未公开 Practical 论文包交付 | 200 条自然语言问题派生询问，主文只用 `g=4..10` |
| DBpedia | 5,887,296 | 18,338,729 | 同上 | DBpedia-Entity v2 的 438 条多词询问，主文只用 `g=4..10` |
| GPU4GST_Musae | 19,109 | 400,497 | GPU4GST 作者成品 | 作者 CSV audit；本文相关组扩展 |
| GPU4GST_Twitch | 34,118 | 429,113 | 同上 | 同上 |
| GPU4GST_Github | 37,700 | 289,003 | 同上 | 同上 |
| GPU4GST_Youtube | 1,134,890 | 2,987,624 | 同上 | 同上 |
| DBLP-GPU25 | 2,497,782 | 12,786,329 | GPU4GST 作者成品 | GPU25 组/询问专用；禁止使用 VEW21 询问 |
| GPU4GST_Orkut | 3,072,441 | 117,185,083 | GPU4GST 作者 `.in` 文本成品 | 同上 |
| GPU4GST_LiveJournal | 3,997,962 | 34,681,189 | GPU4GST 作者成品 | 同上 |
| GPU4GST_Reddit | 4,262,834 | 12,502,767 | GPU4GST 作者成品 | 同上 |
| SteinLib-WRP | 72–298（转换后） | 112–549（转换后） | SteinLib WRP3/WRP4 | 11 个 `g=11..16`、带已知最优值的标准 GST 实例 |

前三个图的公开上游和算法代码位于 [GroupSteinerTree](https://github.com/YahuiSun/GroupSteinerTree)，原论文为 [PVLDB 14(7)](https://www.vldb.org/pvldb/vol14/p1137-sun.pdf)。LinkedMDB/DBpedia 的图建模与询问协议来自 [WWW 2020 KeyKG+](https://doi.org/10.1145/3366423.3380110)；本目录中的具体标准化文件来自已录用但未公开的 *A Practical Sublinear Approximation for Group Steiner Tree*，其本地 PDF 是实验事实的首要依据。GPU 数据来自作者[官方仓库](https://github.com/toziki/GPU4GST-sigmod)及其 README 指向的 OneDrive；详细逐文件审计见 [`GPU4GST_DATA.md`](GPU4GST_DATA.md)。

`data/<name>/<name>.zip` 只包含已标准化的 `graph.txt/query.txt`，是 Practical 交付快照而不是上游网站的原始 dump。论文 artifact 应公开这些 ZIP 的哈希和合法可分发副本，不能声称仅凭会更新的网站即可逐字节重建。

## 4. Practical 原始面板 E3

未公开论文明确规定 Toronto、MovieLens、DBLP 每图有

```text
g in {5,6,7,8}, f_target in {200,400,800,1600}, 10 queries per cell.
```

原 `query.txt` 按 `g` 外层、`f` 内层连续存放 16 个区块。工具
[`tools/data/build_practical_original_panels.py`](../tools/data/build_practical_original_panels.py)
只做逐字复制和结构核验，输出 48 个图/参数单元、480 条询问到
`experiment_data/practical_original/`；`source_query_begin/end` 记录原位置。它不重新采样，因此 E3 可直接作为未公开论文实验协议的复现面板。

LinkedMDB 和 DBpedia 的 `query.txt` 保留自然分布。主文报告 `g=4..10`；`g=1..3` 因问题过于简单，只放完整性附录。每条询问报告实际 `f_real`，不做 `f_target` 声明。

## 5. 严格分离 `g` 与 `f` 的受控面板 E1

原始 E3 同时改变 `g` 和 `f`，适合复现但不适合单因素因果叙述。本文在相同三张真实图上建立新的半合成面板：

- `g` sweep：`g=4..16`，固定每条询问的平均组大小恰为 `f_target=400`；
- `f` sweep：`g in {6,10,14}`，`f_target in {200,400,800,1600}`；
- 每格 10 条，基础 seed `20260721`；合并重复的 `(g,400)` 后共 66 格、660 条；
- 组内从 `1..n` 无放回均匀采样，不同组允许重叠；组大小先按 `N(f,0.15f)` 产生，再在 `[0.5f,1.5f]` 内平衡，使**每条询问**的平均值精确等于 `f_target`。

生成器为 [`tools/data/generate_controlled_queries.py`](../tools/data/generate_controlled_queries.py)，manifest 位于 `experiment_data/controlled/manifest.json`。它是已给生成器 `data/generate_queries_for_noGPU4GST.py` 的版本化、精确均值改进；旧脚本和 `query_g10..20.txt` 仅留作来源记录，不进入主矩阵。

这解决了参数矛盾：E1 用于回答“固定 `f` 时随 `g` 如何扩展”和“固定 `g` 时随 `f` 如何扩展”；E2/E3 用于回答自然或来源特定分布下是否稳健。两类证据不可合并成一条 `f` 曲线。

## 6. GPU4GST 相关组面板 E2 与作者面板 E4

GPU4GST 作者论文每个数据集/参数运行 300 条，原文只公开 `g=3,5,7` 的 CSV，且未公开随机 seed。转换器保留两层：

1. `query_author_g3/g5/g7.txt`：逐行展开作者 CSV 的前 300 条；
2. `query_g4..g16.txt`：依据 *Approximating Probabilistic Group Steiner Trees in Graphs* 的“候选组共现图 + 最小 BFS 深度 + 可行性拒绝”协议，由本仓库 seed `2025` 独立生成，不能称为作者原询问。

全量扩展共有 `8*13*300=31,200` 条，10,000 秒精确对照无法全部完成。冻结抽样工具
[`tools/data/build_gpu_query_panels.py`](../tools/data/build_gpu_query_panels.py)
不读取任何运行结果，并按 `log1p(f_real)` 排名十分位分层：

- 八图都取 `g=4,8,12,16`，每格 10 条，共 320 条；
- Musae、Youtube、DBLP-GPU25、Orkut 再取其余 `g=5..15`，每格 10 条，共 360 条；
- 合计 68 格、680 条。每格正好从 `f_real` 排名十分位各取一条，选择的原询问索引和哈希键保存在 `selected_queries.json/csv`。

E4 从作者 `g=5,7` CSV 中每图每 `g` 分层取 10 条，共 160 条，专门比较统一实现与 GPU4GST 作者 CPU artifact。它是 artifact 审计，不承担本文 `g=4..16` 的主 scaling claim。

作者 Orkut 二进制 CSR/weight 文件在官方 OneDrive 成品中截断，但 `.in` 文本图完整；本文只从完整 `.in` 构造通用 `graph.txt`。该事实、文件长度和修复原则见 [`GPU4GST_DATA.md`](GPU4GST_DATA.md)，不能隐去。

## 7. SteinLib WRP 标准实例 E0

[SteinLib](https://steinlib.zib.de/testset.php)把 WRP3/WRP4 列为 Group Steiner Tree test sets，并提供实例与参考最优值。本地归档哈希由 `environment_lock.json` 锁定。

WRP 的 `.stp` 使用昂贵 dummy terminal 连接一个组的候选顶点。工具
[`tools/data/convert_steinlib.py`](../tools/data/convert_steinlib.py)
执行可逆的专用转换：

1. 识别每个 terminal 的零/固定结构连接；
2. 删除 dummy terminal 和 connector 边；
3. 把其邻居集合变成一个 GST 组；
4. 压缩剩余顶点 ID，并保持原边权；
5. 用 `published_STP_optimum - connector_cost_sum` 得到 GST 最优值。

只有满足结构条件且 `g<=16` 的实例进入本文：WRP3 的 `g=11..16` 六个，WRP4 的 `g=11,13,14,15,16` 五个。原 `.stp` 始终保留，SCIP-Jack 直接解原文件；ABHSS、DPBF、Basic+ 和 PrunedDP++ 解转换文件。二者经 connector 常数调整后必须一致。

## 8. 两份 DBLP 的强制隔离

`data/DBLP/graph.txt` 与 `data/GPU4GST_DBLP/graph.txt` 的 header 都是 `2,497,782 / 12,786,329`，但完整审计得到：

```text
topology_multiset_equal = false
weights_equal = false
weights_constant_scale = false
first_sorted_topology_mismatch = 7 (zero-based)
```

它们的 SHA-256 分别为：

```text
DBLP-VEW21  f3f60f5b3ed6689ac573b15946254cf3421fa9d76be4b0e70dcd6aed99e5e0e9
DBLP-GPU25  841ddb99c7be2d65c9a629cd7c014883e8f24ca177e996e1efa221266629a82e
```

因此论文、CSV 和图例只能使用 `DBLP-VEW21`、`DBLP-GPU25`，不能裸写 DBLP 后跨面板合并。重复执行：

```powershell
cmake --build build --config Release --target compare_graphs
.\build\Release\compare_graphs.exe `
  data\DBLP\graph.txt data\GPU4GST_DBLP\graph.txt
```

## 9. 重新生成与验收顺序

```powershell
# GPU 作者数据转换/扩展（通常不必重跑 7.8GB 原始包）
cmake --build build --config Release --target prepare_gpu4gst
.\build\Release\prepare_gpu4gst.exe `
  data_origin data all --seed 2025 --queries 300 --min-g 4 --max-g 16

# 固定论文面板
python tools/data/generate_controlled_queries.py
python tools/data/build_practical_original_panels.py
python tools/data/build_gpu_query_panels.py
python tools/data/build_gpu_author_panels.py
python tools/data/convert_steinlib.py

# 更新数据快照并做全环境验收
python tools/data/audit_dataset_snapshot.py --hash
python tools/experiments/validate_environment.py
```

正式投稿冻结后，除修复可证明的转换 bug 外不得重新生成。任何重新生成都必须更新 seed/版本号、全部 manifest、`data_snapshot.json` 和论文 artifact version；不能保留旧结果继续拼表。
