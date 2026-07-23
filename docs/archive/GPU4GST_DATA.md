# [归档] GPU4GST 作者 workload 与跨 `g` 扩展

GPU4GST 作者数据现在承担两个明确且互不混淆的角色：P1 直接复用作者图和全部论文查询，P2 在其中六张作者图上按原 related-group 思路生成 `g=5..16` 的新查询。本文不比较 GPU 时间；GPU4GST 的异构实现只提供数据与研究脉络，正式性能方法仍是三个单线程 CPU 精确方法。

## 1. P1：作者原始图和查询

八张图为 Musae、Twitch、Github、Youtube、DBLP、Orkut、LiveJournal、Reddit。原始 `.in` 保存无向整数加权图，`.g` 保存候选组，`3.csv`、`5.csv`、`7.csv` 保存候选组编号。

CSV 编号是 `.g` 的 **0-based 行号**，而 `.g` 标签写成 `g1,g2,...`。`prepare_gpu4gst` 将编号转换为显式顶点组，不改变图、权重或组选取。每图每个 `g` 使用前 300 行，因此每图 900 条、八图共 7,200 条。LiveJournal CSV 虽有 2,000 行，但作者实验脚本只运行 `0..299`，所以其余行不属于 P1。

P1 不按 `g` 报最终主表：三个 query blocks 只是运行与审计单位，汇总时每图、每方法只有一行总工作量。最终图/查询哈希见 `experiment_data/p1_published_workloads/manifest.json`。

## 2. 作者成品与论文表的已知差异

主实验以作者实际成品为准，并公开记录差异：

- GPU4GST 论文表写 DBLP 2,423,455 点，作者 `.in` 实测为 2,497,782 点；
- 论文表写 Orkut 3,072,440 点，作者 `.in` 实测为 3,072,441 点；
- 作者 OneDrive 中 Orkut CSR 二进制被截断，但 `.in` 文本图完整；本仓库从完整 `.in` 读取/转换，不使用截断 CSR；
- Youtube、Orkut 的 `.g` 行数与论文候选组总数不同，P1 仍使用作者实际 `.g`，不根据当前 SNAP 文件猜测重建；
- 两个 DBLP 接口点边数相同但哈希不同，永不合并。

这些差异是复现实验需要报告的 artifact facts，不应静默改用另一张同名图。逐文件证据和哈希见 [`data_origin/README.md`](../../data_origin/README.md)。

## 3. P2：`g=5..16` 新查询

P2 使用六张作者图，按规模分为：

| 层级 | 图 |
|---|---|
| small | Musae、Twitch |
| medium | Youtube、DBLP |
| large | Orkut、Reddit |

候选查询使用 related-group 共现 BFS：两个候选组共享原图顶点时在组图中相邻；均匀选择根组，BFS 到能够找到足够相关组的最小深度，再抽取 `g-1` 个组，并拒绝没有共同原图连通分量的查询。该方法来自 GPU4GST 所沿用的 *Approximating Probabilistic Group Steiner Trees in Graphs* 查询协议。GPU4GST 没有公开其原始随机种子和完整生成代码，因此 P2 只声称“按同类方法独立生成”，不声称恢复作者输出。

每张图、每个 `g=5..16` 用生成 seed `2025` 固定产生 300 条候选。正式 panel 按 `log1p` 后的真实平均组大小排序为五个等秩层，每层用 panel seed `20260723` 派生的稳定哈希选一条：

- 每格 5 条；
- 每图 60 条；
- 六图共 360 条。

分层只读输入，不读 solver 结果，因此不会按 ABHSS 的有利区域挑查询。`query_g*.txt` 是 300 条候选；`experiment_data/p2_cross_g/*/cross_g*.txt` 才是正式五条 panel。对应 `.group_ids.txt`、查询哈希和选择规则都在 `cells.json/csv` 中。

## 4. 转换和重建

取得 `data_origin` 中的作者 `.in`、`.g` 与 CSV 后运行：

```powershell
cmake --build build --config Release --target prepare_gpu4gst
./build/Release/prepare_gpu4gst.exe data_origin data all --seed 2025 --queries 300 --min-g 5 --max-g 16
python tools/data/build_published_workloads.py
python tools/data/build_gpu_query_panels.py
```

第一步同时生成 P1 的作者查询接口和 P2 的 300 条候选；后两步冻结 P1 身份与 P2 五条 panel。正式运行前还必须执行 feasibility audit 和环境校验，详见 [`RUN.md`](../../RUN.md)。

## 5. 再分发边界

作者仓库为 <https://github.com/toziki/GPU4GST-sigmod>，完整数据由作者 OneDrive 提供。当前冻结未发现明确覆盖全部数据/代码的仓库级再分发许可。内部实验可按来源与哈希恢复，公开 artifact 前应取得许可，或仅发布转换器、manifest、下载说明和哈希，由评测者自行取得作者文件。
