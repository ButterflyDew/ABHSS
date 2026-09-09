# 数据、baseline 与第三方 artifact 历史

本卷保存数据来源、格式转换、作者 workload、baseline 复现与第三方依赖的历史细节。当前实验选择、任务数和报告口径以 [`../EXPERIMENT_PLAN.md`](../EXPERIMENT_PLAN.md) 为准；本卷负责可追溯性与恢复说明。

> 归档时态约定：下方机械合并的旧正文中，“当前”“本文”“正式”等词只表示原文件写成时的方案，不具有现行规范效力。尤其其中的 P2 `g=5..16` 和 S2 DBLP/IMDb 设计均已退出；现行范围是 P2 `g=5..15` 与 S2 DBLP/Toronto，且只以实验计划和机器矩阵为准。

## 卷内目录

- [[归档] PrunedDP++ baseline 复现说明](#history-baseline) — 原文件 `BASELINE.md`
- [[归档] 数据来源、转换与归档](#history-data-provenance) — 原文件 `DATA_PROVENANCE.md`
- [[归档] GPU4GST 作者 workload 与跨 `g` 扩展](#history-gpu4gst-data) — 原文件 `GPU4GST_DATA.md`
- [[归档] 第三方 correctness gate 依赖](#history-third-party) — 原文件 `THIRD_PARTY.md`

<a id="history-baseline"></a>

## [归档] PrunedDP++ baseline 复现说明

> 原始记录：`BASELINE.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文说明仓库对 SIGMOD 2016 论文 *Efficient and Progressive Group Steiner Tree Search* 中 PrunedDP++ 的复现。论文给出了伪代码和复杂度声明，但没有公开本文可获得的源码，也没有规定状态容器、树 witness 的表示或优先队列更新细节。因此，仓库把三个会实质影响时间、空间或正确性的复现选择显式暴露为开关，并在输出中记录实际配置。

默认配置现按“主 baseline 必须精确”的口径设置为：**Hash 状态、启用逐状态 MST 上界、关闭 pathmax 并允许更小 `g` 的状态 reopen**。Algorithm 4 第 31 行的 paper-pathmax 行为仍可显式开启以复现论文口径，但它已在三个 SteinLib 已知最优值实例上返回非最优值，不能再作为主精确 baseline。

> 历史范围说明（已由现行方案取代）：随机小图、旧 MovieLens q1 和外部算法 artifact 数字只用于复现/正确性审计；当时的投稿矩阵曾计划使用 DBLP/IMDb 受控副实验。现行范围以 [`EXPERIMENT_PLAN.md`](../EXPERIMENT_PLAN.md) 和 [`experiments/paper_matrix.json`](../../experiments/paper_matrix.json) 为准。

### 1. 论文中的 PrunedDP++ 主线

对查询组集合 `P`，状态 `(v,X)` 表示一棵以 `v` 为根并覆盖 `X subseteq P` 的树，其已付成本记为 `g(v,X)`。搜索包含两类转移：沿图边把根从 `v` 移到相邻顶点，以及在同一根合并两个不相交的已完成状态。PrunedDP 的 optimal-tree decomposition 和 conditional merging 只扩展 `g(v,X)<best/2` 的状态，并只接受合并成本不超过 `2best/3` 的候选。

PrunedDP++ 在此基础上使用 A* 优先级

```text
f(v,X) = g(v,X) + max(pi_1(v,X), pi_t1(v,X), pi_t2(v,X)).
```

`pi_1` 是到最远未覆盖组的距离。`pi_t1` 和 `pi_t2` 使用 Algorithm 3 预计算的组间最短访问路线 `W(i,j,R)`：前者在起止组对上取最小值，后者在起始组上取最大值。仓库现在严格按 Algorithm 3 的递推初始化 `W(i,i,{i})=0`，每次只向尚未访问的组扩展；旧实现额外插入了空 mask、单端 mask 和“不加入新组”的转移，与原文并不一致，现已删除。

### 2. 不确定性一：稀疏状态如何存储

Algorithm 4 只把 `Q` 写成优先队列、把 `D` 写成已完成状态集合，没有说明 `(mask,v)` 使用 Hash、树结构、分层数组还是完整 `2^k n` 表。论文同时声称 PrunedDP++ 生成的状态更少并使用更少内存；如果无条件开满 `2^k n`，剪掉状态并不会减少主状态表的空间，因此这种实现不能代表论文所强调的稀疏收益。

仓库提供两个语义相同的后端：

| 配置 | 实现 | 主状态空间 |
| --- | --- | --- |
| `hash`，默认 | `unordered_map<(mask,v), StateEntry>`，只在候选首次进入搜索时建立记录 | `O(s)`，`s` 为实际发现状态数 |
| `dense` | 连续 `StateEntry[2^k][n+1]`，用 `present` 区分未发现状态 | `O(2^k n)` |

搜索不遍历 Hash，所有合并仍枚举补集子 mask 并做一次键查找，所以两个后端的状态语义、候选集合和优先级完全一致。组距离 `O(kn)` 与 Algorithm 3 路线表 `O(2^k k^2)` 是论文明确需要的公共预处理，不属于 `(mask,v)` 主状态表。

### 3. 不确定性二：逐状态 MST 可行解

Algorithm 4 第 11--15 行要求每弹出一个状态 `(v,X)` 都执行以下操作：恢复该状态对应的树 `T(v,X)`；对每个未覆盖组恢复一条从 `v` 出发的最短路径；对这些边的并集求 MST；用 MST 权重更新 `best`。这不是简单地把若干距离相加，因为状态树和不同最短路径可能共享边，MST 还会删除并集中的环。

启用 `mst_upper` 时，仓库为每个被优先队列接受的候选建立不可变 witness DAG。边扩展节点记录一条原图边和父 witness，合并节点记录两个 witness。状态弹出后，代码恢复 witness 边与剩余组最短路径边，按 edge id 去重，再用 Kruskal 得到论文要求的真实 MST 上界。关闭 `mst_upper` 时，不构造 witness，也不执行逐状态可行解拼接；`best` 只由完整状态或互补状态合并更新。

论文声称 PrunedDP++ 的最坏复杂度不高于 PrunedDP，即 `O(3^k n + 2^k(n log n+m))`，但其 cost analysis 没有计入 Algorithm 4 第 11--15 行。若第 `i` 个弹出状态的 witness 与补全路径并集包含 `e_i` 条不同边、恢复过程访问 `w_i` 个 witness 节点，则严格实现额外需要

```text
sum_i O(w_i + e_i log e_i)
```

时间，并需要保存 witness DAG。该项不能一般性吸收到论文给出的 DP 上界中。因此文档与结果必须区分“严格 MST 复现时间”和“不执行 MST 的理论主搜索时间”，不能把前者直接标成满足论文原复杂度。

### 4. 不确定性三：`lb_2` 的强制一致性

论文先证明原始 `pi_t2` 是 admissible lower bound，随后明确承认它不一致。正文提出对每条转移传播父下界，Algorithm 4 第 31 行把新候选的总优先级改成

```text
candidate_f = max(raw_g_plus_h, parent_f).
```

这个 pathmax 值是**生成路径相关的候选属性**，不是只由 `(v,X)` 决定的状态函数。同一个 `(v,X)` 从两个父状态到达时可以得到两个不同的传播值；但 Algorithm 4 又只用 `(v,X)` 作为 `Q/D` 的键，第 35 行只在新 `lb` 更小时更新队列，状态进入 `D` 后永久关闭。于是，一个优先级较小但 `g` 较大的候选可能先关闭状态，之后 `g` 更小但传播优先级不更小的候选会被忽略。Lemma 7 把传播后的路径相关值当作统一的 `pi_t2(v,X)`，没有覆盖这一冲突，因而不能推出伪代码实现的状态级正确性。

仓库提供两种明确语义：

| 配置 | 行为 | 正确性口径 |
| --- | --- | --- |
| `lb2_pathmax=on` | 执行第 31 行 `max`；按论文 `Q/D` 语义永久关闭状态；队列中同键候选只在优先级严格减小时替换 | paper-pathmax 复现；已有非最优反例，只能审计 |
| `lb2_pathmax=off`，默认 | 使用原始 admissible 下界；按更小 `g` 更新，并允许已关闭状态 reopen | 标准 inconsistent-A* 的安全精确实现，主性能 baseline |

关闭 pathmax 不是关闭 `pi_t2`：`pi_t2` 仍参与 `max(pi_1,pi_t1,pi_t2)`，只是不再把父候选的优先级强行写入后继。reopen 可能使一个状态被展开多次，所以该安全模式也不能机械沿用论文“一状态只弹出一次”的最坏时间分析。

### 5. 实现结构

当前实现位于 `src/pruneddp/pruneddp.cpp`，公开配置位于 `src/pruneddp/pruneddp.h`：

```cpp
struct PrunedDpOptions {
    StateStorage state_storage = StateStorage::Hash;
    bool use_mst_upper_bound = true;
    bool enforce_lb2_pathmax = false;
};
```

一次查询依次执行：

1. 对每个组执行多源 Dijkstra，同时保存从任意顶点走向最近组终端的最短路径 witness；
2. 在组间度量上执行 Algorithm 3，得到 `W(i,j,R)` 和 `W(i,R)`；
3. 将每个组终端作为零成本单组状态加入 A* 队列；
4. 弹出状态，按开关更新 MST 上界，再执行互补合并、条件边扩展和条件子集合并；
5. Hash 模式只建立实际发现状态，dense 模式使用同一套更新与弹出逻辑；
6. 队列最小优先级不再小于可行上界时返回 `best`。

冻结包已删除研究统计计数器和 `pruneddp_stats.txt`，避免计数影响正式计时。`weights.txt` 的 run header 仍记录三个开关，防止不同复现口径被误当成同一次实验。

### 6. 空间统计口径

原文第 5 节把空间指标定义为查询处理分配的平均内存开销，并明确说明不计入所有查询共同使用的原图存储。仓库因此在图和全部查询加载完成后记录固定 RSS 基线；每条 `weights.txt` 记录的第三列为

```text
max(0, 询问执行期间的绝对 peak RSS - 固定的加载后 RSS 基线)
```

该值既不是包含原图的完整进程 RSS，也不是进程启动以来的历史 peak。多询问仍在同一进程内顺序执行；固定基线会保守地计入前序询问后由内存分配器保留的求解页，避免逐条减 `rss_before` 低估后续询问。运行 header 的 `memory_metric=query_processing_peak_rss_overhead_mb` 标识新口径；旧 header `memory_metric=query_peak_rss_mb` 表示包含原图的绝对 RSS，不能直接用于论文空间比较。

### 7. 构建与运行

Release/O2 构建：

```powershell
cmake --build build --config Release --target pruneddp
```

默认 Safe 精确模式：

```powershell
.\build\Release\pruneddp.exe Toronto result g10 data 1 5
```

三个开关位于六个公共位置参数之后，可以独立组合：

```powershell
.\build\Release\pruneddp.exe Toronto result g10 data 1 5 `
  --state-storage=dense --mst-upper=off --lb2-pathmax=off
```

| 参数 | 可选值 | 默认值 |
| --- | --- | --- |
| `--state-storage` | `hash` / `dense` | `hash` |
| `--mst-upper` | `on` / `off` | `on` |
| `--lb2-pathmax` | `on` / `off` | `off` |

### 8. 正确性验证

随机验证均使用 Release/O2，并以 `1e-6` 与 DPBF 比较：

| 配置 | 随机实例 | 结果 |
| --- | ---: | --- |
| Hash + MST + pathmax | `350` | 全部一致，但不能排除系统性反例 |
| dense + MST + pathmax | `150` | 全部一致 |
| Hash + no-MST + pathmax | `150` | 全部一致 |
| Hash + MST + raw `lb_2` reopen | `400` | 全部一致 |
| dense + no-MST + raw `lb_2` reopen | `150` | 全部一致 |

MovieLens 默认查询 q1 另以 Safe 和 DPBF 分别完整运行，两者答案均为 `0.0032922542`；PrunedDP++ 实际只发现 `2725` 个 `(mask,v)` 状态。这验证了零权边输入上实现可终止，也直观说明默认 Hash 不应预分配完整 `2^k n` 主状态表。独立迁移二进制在删除统计后又以 Strict 和 Safe 两种口径分别对 DPBF 完成 `300/300`，seed 为 `72172203/72172204`；迁移版 MovieLens q1 仍返回同一答案。

随机小图并未暴露 paper-pathmax 的生成路径冲突；新增 SteinLib WRP3/WRP4 已知最优值测试后得到确定反例：

| 实例 | 已知最优值 | Safe / ABHSS / DPBF / SCIP-Jack | paper-pathmax 重实现 |
| --- | ---: | ---: | ---: |
| wrp4-11 | `179` | `179` | `180` |
| wrp4-15 | `405` | `405` | `407` |
| wrp4-16 | `1190` | `1190` | `1213` |

11 个 `g=11..16` WRP 转换实例上，Safe、ABHSS Base、ABHSS 全增强配置、DPBF 与 SCIP-Jack 全部匹配已知最优值；Strict 仅 `8/11` 匹配。两个 ABHSS 配置调用同一 `abhss` 可执行文件，只是查询开始前冻结的增强掩码不同。GPU4GST 2025 作者 CPU artifact 在 wrp4-11 返回 `179`，说明这里的反例针对本仓库对 2016 伪代码 pathmax/closed 组合的重实现，不能错误归因于 2025 artifact。机器可读证据见 [`experiments/correctness_audit.json`](../../experiments/correctness_audit.json)。

结论：`lb2_pathmax=off` 的 reopen 模式是主精确 baseline；`on` 只用于重现实验，任何速度数字都不能进入精确方法主表。

### 9. 引用边界

PrunedDP、PrunedDP++、Algorithm 3 的组路线 DP、三类下界、逐状态 MST 可行解、optimal-tree decomposition 和 conditional merging 均来自上述 SIGMOD 2016 原文。Hash/dense 双后端、不可变 witness DAG、开关接口、运行配置记录，以及关闭 pathmax 后按更小 `g` reopen，是本仓库为消除复现歧义所做的工程与正确性处理，不应归因于原论文。

<a id="history-data-provenance"></a>

## [归档] 数据来源、转换与归档

> 原始记录：`DATA_PROVENANCE.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

本文冻结不再采用“所有图都换成最新官方版本”的统一策略。为了最大化与最近直接相关工作的可比性，主实验 1 使用 MonoGST+ 与 GPU4GST 实际发布/提供的处理后图和原始查询；只有没有历史作者图的 IMDb 敏感性实验使用最新官方冻结。图身份由来源、转换过程与最终 `graph.txt` SHA-256 共同决定，同名不表示同图。

### 1. 主实验 1：原论文完整 workload

#### MonoGST+

使用 *A Practical Sublinear Approximation for Group Steiner Tree* 作者实验中的五套完整接口：Toronto、MovieLens、DBLP、LinkedMDB、DBpedia。图和 `query.txt` 均保持作者版本，不从当前官方站点重新下载或重生成。

| 正式身份 | 图目录 | 查询 | 上游角色 |
|---|---|---:|---|
| `Toronto-MonoGSTPlus` | `data/Toronto` | 160 | ImprovAPP 图与受控查询 |
| `MovieLens-MonoGSTPlus` | `data/MovieLens` | 160 | ImprovAPP 图与受控查询 |
| `DBLP-MonoGSTPlus` | `data/DBLP` | 160 | ImprovAPP 图与受控查询 |
| `LinkedMDB-MonoGSTPlus` | `data/LinkedMDB` | 200 | KeyKG+/WikiMovies 自然查询 |
| `DBpedia-MonoGSTPlus` | `data/DBpedia` | 438 | KeyKG+/DBpedia-Entity v2 自然查询 |

MonoGST+ 已被 VLDB 2026 接收但尚未公开，当前手稿及大文件只供内部研究。公开 artifact 前必须取得作者许可，或提供作者认可的获取步骤与哈希，不能默认再分发。

#### GPU4GST

使用作者 OneDrive/GitHub artifact 中的八张处理后图、候选组和查询 CSV：Musae、Twitch、Github、Youtube、DBLP、Orkut、LiveJournal、Reddit。转换器只完成格式适配：

1. 从作者 `.in` 读取无向加权边，保留顶点编号和整数权重；
2. 从 `.g` 展开候选组；
3. 把 CSV 中 0-based 候选组行号转换成求解器需要的显式顶点组；
4. 对 `g=3,5,7` 各保留论文实际使用的前 300 行，不抽样、不按运行结果筛选。

详细编号、边权与作者成品差异见 [`DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data`](DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data) 和 [`data_origin/README.md`](../../data_origin/README.md)。作者原始文件的 OneDrive 元数据与 SHA-256 分别在 `data_origin/OFFICIAL_ONEDRIVE_MANIFEST.json` 和 `data_origin/SHA256SUMS.txt`。

主实验 1 共 13 个图身份、29 个可执行 query blocks、8,318 条查询。唯一机器真值为 [`experiment_data/p1_published_workloads/manifest.json`](../../experiment_data/p1_published_workloads/manifest.json)，其中固定最终图/查询哈希、点边数和查询分布。

两个 DBLP 必须分开。虽然当前接口实测点边数相同，`DBLP-MonoGSTPlus` 与 `DBLP-GPU4GST` 的图哈希分别为 `f3f60f5b…e5e0e9` 和 `841ddb99…629a82e`，因此不能合并结果或互换查询。

### 2. 主实验 2：GPU4GST 图上的跨 `g` 扩展

P2 复用 GPU4GST 作者图与 `.g` 候选组，只新增 `g=5..16` 查询。生成语义来自 GPU4GST 所沿用的 related-group 方法：在候选组共现图上选根组并 BFS 到足够深度，再抽取相关组；没有共同原图连通分量的查询被拒绝。

每个 `(graph,g)` 使用生成 seed `2025` 固定产生 300 条候选，然后仅依据输入组的 `log1p` 平均大小秩分成五层，每层用 panel seed `20260723` 派生的稳定 SHA-256 key 排序。第一轮每层取第一名形成原 q1--q5，第二轮每层取第二名并追加为 q6--q10；扩展不改变原五条身份和顺序。选择过程不读取任何求解器时间、内存、目标值或完成状态。正式六图为 Musae、Twitch、Youtube、DBLP-GPU4GST、Orkut、Reddit，共 72 格、720 条。

- 原始生成器：`tools/gpu4gst_data/prepare_gpu4gst.cpp`
- 固定选择器：`tools/data/build_gpu_query_panels.py`
- 输出清单：`experiment_data/p2_cross_g/cells.json`

这些查询是本文扩展，不得称为 GPU4GST 作者原始查询；作者原查询仅指 P1 的 `g=3,5,7` CSV。

### 3. 副实验：DBLP 与 IMDb 的 `<g,f>`

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

### 4. 查询可行性与完整性

共同连通分量审计覆盖当前矩阵中的每个图/查询对。P2、受控副实验和 correctness gates 必须全部可行。P1 的原作者自然查询中存在 55 条无可行树：LinkedMDB 46 条、DBpedia 9 条。因为 P1 承诺“全询问”，这些查询保留原索引并要求三种精确方法一致返回 `infeasible`，不能删除、替换或从总时间分母中排除。

审计结果固定在 [`experiments/query_feasibility_audit.json`](../../experiments/query_feasibility_audit.json)。改变图、查询、矩阵或生成器后必须重建审计。

### 5. 旧数据归档与恢复

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

### 6. 投稿与公开检查

- 论文表格同时写正式身份、来源和最终图哈希；不要只写 “DBLP”。
- P1 明确称为作者 published workload reproduction；P2 明确称为本文生成的跨 `g` 扩展。
- P1 全部查询都必须运行；不能根据难度或结果筛选。
- S2 报告目标 `f` 与实际组大小，不声称复刻 PrunedDP++ 的历史数值。
- 公开前逐项核验 MonoGST+、GPU4GST、IMDb 和第三方源码的再分发许可。
- 本地归档含未必允许公开的数据与论文，只作恢复备份，不随 GitHub artifact 发布。

<a id="history-gpu4gst-data"></a>

## [归档] GPU4GST 作者 workload 与跨 `g` 扩展

> 原始记录：`GPU4GST_DATA.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

GPU4GST 作者数据现在承担两个明确且互不混淆的角色：P1 直接复用作者图和全部论文查询，P2 在其中六张作者图上按原 related-group 思路生成 `g=5..16` 的新查询。本文不比较 GPU 时间；GPU4GST 的异构实现只提供数据与研究脉络，正式性能方法仍是三个单线程 CPU 精确方法。

### 1. P1：作者原始图和查询

八张图为 Musae、Twitch、Github、Youtube、DBLP、Orkut、LiveJournal、Reddit。原始 `.in` 保存无向整数加权图，`.g` 保存候选组，`3.csv`、`5.csv`、`7.csv` 保存候选组编号。

CSV 编号是 `.g` 的 **0-based 行号**，而 `.g` 标签写成 `g1,g2,...`。`prepare_gpu4gst` 将编号转换为显式顶点组，不改变图、权重或组选取。每图每个 `g` 使用前 300 行，因此每图 900 条、八图共 7,200 条。LiveJournal CSV 虽有 2,000 行，但作者实验脚本只运行 `0..299`，所以其余行不属于 P1。

P1 不按 `g` 报最终主表：三个 query blocks 只是运行与审计单位，汇总时每图、每方法只有一行总工作量。最终图/查询哈希见 `experiment_data/p1_published_workloads/manifest.json`。

### 2. 作者成品与论文表的已知差异

主实验以作者实际成品为准，并公开记录差异：

- GPU4GST 论文表写 DBLP 2,423,455 点，作者 `.in` 实测为 2,497,782 点；
- 论文表写 Orkut 3,072,440 点，作者 `.in` 实测为 3,072,441 点；
- 作者 OneDrive 中 Orkut CSR 二进制被截断，但 `.in` 文本图完整；本仓库从完整 `.in` 读取/转换，不使用截断 CSR；
- Youtube、Orkut 的 `.g` 行数与论文候选组总数不同，P1 仍使用作者实际 `.g`，不根据当前 SNAP 文件猜测重建；
- 两个 DBLP 接口点边数相同但哈希不同，永不合并。

这些差异是复现实验需要报告的 artifact facts，不应静默改用另一张同名图。逐文件证据和哈希见 [`data_origin/README.md`](../../data_origin/README.md)。

### 3. P2：`g=5..16` 新查询

P2 使用六张作者图，按规模分为：

| 层级 | 图 |
|---|---|
| small | Musae、Twitch |
| medium | Youtube、DBLP |
| large | Orkut、Reddit |

候选查询使用 related-group 共现 BFS：两个候选组共享原图顶点时在组图中相邻；均匀选择根组，BFS 到能够找到足够相关组的最小深度，再抽取 `g-1` 个组，并拒绝没有共同原图连通分量的查询。该方法来自 GPU4GST 所沿用的 *Approximating Probabilistic Group Steiner Trees in Graphs* 查询协议。GPU4GST 没有公开其原始随机种子和完整生成代码，因此 P2 只声称“按同类方法独立生成”，不声称恢复作者输出。

每张图、每个 `g=5..16` 用生成 seed `2025` 固定产生 300 条候选。正式 panel 按 `log1p` 后的真实平均组大小排序为五个等秩层，每层用 panel seed `20260723` 派生的稳定哈希排序。第一轮每层选第一名形成原 q1--q5；第二轮每层选第二名并追加为 q6--q10，原五条的身份和顺序不变：

- 每格 10 条；
- 每图 60 条；
- 六图共 720 条。

分层只读输入，不读 solver 结果，因此不会按 ABHSS 的有利区域挑查询。`query_g*.txt` 是 300 条候选；`experiment_data/p2_cross_g/*/cross_g*.txt` 才是正式十条 panel。对应 `.group_ids.txt`、查询哈希和选择规则都在 `cells.json/csv` 中。

### 4. 转换和重建

取得 `data_origin` 中的作者 `.in`、`.g` 与 CSV 后运行：

```powershell
cmake --build build --config Release --target prepare_gpu4gst
./build/Release/prepare_gpu4gst.exe data_origin data all --seed 2025 --queries 300 --min-g 5 --max-g 16
python tools/data/build_published_workloads.py
python tools/data/build_gpu_query_panels.py
```

第一步同时生成 P1 的作者查询接口和 P2 的 300 条候选；后两步冻结 P1 身份与 P2 十条 panel。正式运行前还必须执行 feasibility audit 和环境校验，详见 [`RUN.md`](../../RUN.md)。

### 5. 再分发边界

作者仓库为 <https://github.com/toziki/GPU4GST-sigmod>，完整数据由作者 OneDrive 提供。当前冻结未发现明确覆盖全部数据/代码的仓库级再分发许可。内部实验可按来源与哈希恢复，公开 artifact 前应取得许可，或仅发布转换器、manifest、下载说明和哈希，由评测者自行取得作者文件。

<a id="history-third-party"></a>

## [归档] 第三方 correctness gate 依赖

> 原始记录：`THIRD_PARTY.md`。以下正文仅做机械合并和标题降级；历史结论的适用边界以原文为准。

`third_party/` 和 `build_external/` 不属于仓库源文件。它们是可重新获取的外部 checkout 与构建产物，清理后的工作区中默认不存在，并由 `.gitignore` 整体排除。

正式大图性能实验只需要本仓库的单一 `abhss` 二进制（Base 与全增强两种固定配置）以及 PrunedDP++-Safe。以下依赖仅用于 SteinLib 已知最优值 correctness gate 或历史实现审计，不进入主性能表。

### 1. 冻结版本

| 组件 | 版本/commit | 恢复位置 | 当前用途 |
|---|---|---|---|
| GroupSteinerTree | `09f3bda70f3afa546fd88d446cdb185aee01b278` | `third_party/GroupSteinerTree` | Basic+ correctness gate |
| Boost | 1.85.0，归档 SHA-256 `7009fe1faa1697476bdc7027703a2badb84e849b7b0baad5086b087b971f8617` | `third_party/boost_1_85_0` | Basic+ 头文件依赖 |
| SCIP-Jack/SCIP | `74c11e60cd2d45e02f34b2afbf1fc3079c713e03`（tag `v703`） | `third_party/scip-jack-7.0.3` | SteinLib 独立精确验证 |
| SoPlex | `e24c304ef21f1f319de0413e14fed8e5414440b8`（release 5.0.2） | `third_party/soplex-5.0.2` | SCIP-Jack LP 后端 |
| GPU4GST code | `716a19c240c480cb2d23435bbaca55163a48e174` | `third_party/GPU4GST-sigmod` | 仅在需要重做历史 artifact audit 时恢复 |

机器锁定信息也保存在 [`experiments/environment_lock.json`](../../experiments/environment_lock.json)。

### 2. 恢复源码

以下命令只说明目标目录与 commit；执行前应自行确认上游许可和网络环境。

```powershell
New-Item -ItemType Directory -Force third_party

git clone https://github.com/YahuiSun/GroupSteinerTree.git third_party/GroupSteinerTree
git -C third_party/GroupSteinerTree checkout 09f3bda70f3afa546fd88d446cdb185aee01b278

git clone https://github.com/scipopt/scip.git third_party/scip-jack-7.0.3
git -C third_party/scip-jack-7.0.3 checkout 74c11e60cd2d45e02f34b2afbf1fc3079c713e03

git clone https://github.com/scipopt/soplex.git third_party/soplex-5.0.2
git -C third_party/soplex-5.0.2 checkout e24c304ef21f1f319de0413e14fed8e5414440b8
```

Boost 1.85.0 从 <https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2> 获取。验证上述 SHA-256 后解压为 `third_party/boost_1_85_0`。GroupSteinerTree 仓库中的 `ysgraph_20210203.zip` 需原样解压到该仓库目录。

如需重做 GPU4GST 历史实现审计：

```powershell
git clone https://github.com/toziki/GPU4GST-sigmod.git third_party/GPU4GST-sigmod
git -C third_party/GPU4GST-sigmod checkout 716a19c240c480cb2d23435bbaca55163a48e174
```

GPU4GST 数据本身的获取与哈希不依赖这个代码 checkout，见 [`DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data`](DATA_BASELINES_AND_ARTIFACTS.md#history-gpu4gst-data)。

### 3. 构建

恢复 GroupSteinerTree 与 Boost 后，主 CMake 会自动启用 `basic_plus`：

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
```

恢复 SCIP-Jack 与 SoPlex 后，构建独立 correctness solver：

```powershell
python tools/build_external_baselines.py --target scip-jack
```

外部构建输出位于 `build_external/`，同样不应提交。随后执行：

```powershell
python tools/experiments/validate_environment.py --require-binaries
python tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S1_steinlib_exactness_gate
```

### 4. 许可边界

- SCIP-Jack 与 SoPlex 受其上游学术/开源许可约束；论文和 artifact 需按许可引用。
- Boost 使用 Boost Software License 1.0。
- 冻结的 GPU4GST 与 GroupSteinerTree checkout 未发现覆盖全部仓库内容的 repository-level license。公开 artifact 不应直接打包这些目录，应只发布 commit、adapter、哈希和获取说明，或先取得作者许可。
- `src/baselines/` 中只保留本仓库的兼容 adapter；上游核心算法文件不复制进 Git。
