# GPU4GST 数据来源与转换协议

本文档描述完整 31,200 条扩展语料的生成。正式论文不全跑该语料，而使用 [`DATA_PROVENANCE.md`](DATA_PROVENANCE.md) 第 6 节冻结的 68 格/680 条分层面板；运行矩阵见 [`FULL_EXPERIMENT_PLAN.md`](FULL_EXPERIMENT_PLAN.md)。其中 GPU 来源的 DBLP 在论文中固定命名为 `DBLP-GPU25`，不得与 `DBLP-VEW21` 混用。

更新时间：2026-07-21。本文只维护数据来源、编号转换、查询生成和复现命令。作者原始文件、校验和已知成品缺陷的详细记录见 `../data_origin/README.md`。

## 1. 数据边界

仓库保留 Jiayu Li 等人在 *Fast Optimal Group Steiner Tree Search using GPUs* 中使用的 8 个数据集。

- `data_origin/`：作者 OneDrive 成品、SHA-256、OneDrive manifest、原文和校验脚本。
- `data/GPU4GST_<name>/`：转换后可直接由本仓库 CLI 读取的 1-based 文本图和询问。

询问分为两层：

1. `query_author_g3/g5/g7.txt` 逐行转换作者 CSV 前 300 条，不重新随机生成。
2. `query_g4.txt` 到 `query_g16.txt` 每个文件 300 条，按相关组共现 BFS 协议和 seed `2025` 独立生成。这是扩展评测，不是 GPU4GST 论文已发布的查询。

## 2. 编号与文件格式

作者 `.in` 和 `.g` 内的顶点为 0-based，转换后 `graph.txt` 和查询顶点统一加 1。`.g` 标签虽写作 `g1,g2,...`，作者 CSV 中的整数却是 0-based `.g` 行号。作者 commit `716a19c` 中 `read_Group` 将 `gN` 放入 `group_graph[N-1]`，`read_inquire` 则不修改 CSV 整数；转换器严格遵循这一语义。

每个转换目录包含：

```text
graph.txt
query_g4.txt ... query_g16.txt
query_author_g3.txt / query_author_g5.txt / query_author_g7.txt
query_*.group_ids.txt
dataset_manifest.json
```

`group_ids` 保留未展开的 1-based 候选组编号，manifest 保留规模、派生 seed、BFS 深度和拒绝采样数。

## 3. `g=4..16` 查询生成

对每个数据集和 `g`，一条询问按下列步骤产生：

1. 将候选组视为节点，两组共享至少一个原图顶点时在组共现图中连边。
2. 均匀选择一个根组，BFS 扫到累计至少发现 `g-1` 个其他组的最小深度。
3. 从已发现近邻中无放回均匀抽取 `g-1` 个，加上根组。
4. 若不存在一个原图连通分量同时命中全部查询组，丢弃并重新采样。
5. 打乱输出组顺序，避免生成根始终成为求解器第一组。

随机引擎为 `std::mt19937_64`。每个 `(dataset,g)` 由基础 seed、数据集名和 `g` 独立派生 64 位 seed，有界随机整数、Fisher-Yates 和 reservoir sampling 都由工具实现，不依赖标准库 `uniform_int_distribution` 的平台细节。

## 4. 已生成数据

| 目录 | 顶点 | 边 | 候选组 | 组成员关系 | 最大 BFS 深度 | 不可行拒绝数 |
|---|---:|---:|---:|---:|---:|---:|
| `GPU4GST_Musae` | 19,109 | 400,497 | 13,183 | 1,060,946 | 2 | 0 |
| `GPU4GST_Twitch` | 34,118 | 429,113 | 3,163 | 687,900 | 1 | 240 |
| `GPU4GST_Github` | 37,700 | 289,003 | 4,005 | 690,358 | 2 | 0 |
| `GPU4GST_Youtube` | 1,134,890 | 2,987,624 | 5,000 | 72,959 | 5 | 0 |
| `GPU4GST_DBLP` | 2,497,782 | 12,786,329 | 127,726 | 76,920,675 | 1 | 51 |
| `GPU4GST_Orkut` | 3,072,441 | 117,185,083 | 5,000 | 1,078,576 | 6 | 0 |
| `GPU4GST_LiveJournal` | 3,997,962 | 34,681,189 | 664,414 | 7,168,359 | 3 | 0 |
| `GPU4GST_Reddit` | 4,262,834 | 12,502,767 | 1,146,657 | 17,471,829 | 3 | 3 |

全部 `g=4..16` 共 `8*13*300=31,200` 条，作者 `g=3/5/7` 转换共 `7,200` 条。DBLP 与 Github 的询问文件较大，是因为当前通用接口在每条询问中完整展开组成员；未为缩小文件改变查询分布。

## 5. 转换命令与校验

```powershell
cmake --build build --config Release --target prepare_gpu4gst
.\build\Release\prepare_gpu4gst.exe `
  data_origin data all --seed 2025 --queries 300 --min-g 4 --max-g 16
```

第三个位置参数可以是 `all`、`Musae`、`Twitch`、`Github`、`Youtube`、`DBLP`、`Orkut`、`LiveJournal` 或 `Reddit`。工具会校验图头、组号、顶点范围、作者 CSV 与查询可行性；写入先落临时文件，完成后再替换正式文件。已存在且头部匹配的 `graph.txt` 默认复用，明确需要重写时加 `--force-graph`。

作者原始数据完整校验：

```powershell
powershell -ExecutionPolicy Bypass -File data_origin/verify_data.ps1
```

Orkut 的作者 CSR 二进制成品已知截断，因此完整校验会在 SHA-256 匹配后有意报错；转换器只从完整 `Orkut.in` 生成文本接口，不读取损坏二进制。

## 6. 引用

1. Jiayu Li et al. *Fast Optimal Group Steiner Tree Search using GPUs*. Proc. ACM Manag. Data 3(6), Article 327, 2025. 本地原文：`../data_origin/Li 等 - 2025 - Fast Optimal Group Steiner Tree Search using GPUs.pdf`。作者仓库：<https://github.com/toziki/GPU4GST-sigmod>。
2. Shuang Yang et al. *Approximating Probabilistic Group Steiner Trees in Graphs*. PVLDB 16(2):343-355, 2022. 第 4.2 节给出相关组共现 BFS 协议：<https://www.vldb.org/pvldb/vol16/p343-sun.pdf>。
