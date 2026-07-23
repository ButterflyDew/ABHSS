# ABHSS: single-thread exact Group Steiner Tree experiments

本仓库实现并评测纯单计算线程、精确、无向边权 Group Steiner Tree 算法，目标是形成可投稿 SIGMOD/VLDB 的完整实验 artifact。正式主 baseline 是 PrunedDP++-Safe；ABHSS-Light 与 ABHSS-Heavy 始终分别报告，不做逐查询 oracle 切换。

当前正式数据冻结为 `official-latest-20260722`。图只从数据所有者的官方站点下载；有来源查询时使用来源查询，否则按所引用论文的生成方法和固定种子重新生成。旧论文 GitHub 仓库中的已处理图和查询只用于工程审计，不进入正式 timing matrix。

## Quick start

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel
python tools/data/rebuild_official_freeze.py
python tools/data/build_query_feasibility_audit.py
python tools/experiments/validate_environment.py
```

已知最优值 gate：

```powershell
python tools/experiments/run_experiments.py --run-id correctness --run-dir results/paper_runs/correctness --suite S1_steinlib_exactness_gate
```

完整构建、分片、恢复运行和汇总命令见 [`RUN.md`](RUN.md)。正式方法逐查询 timeout 为 10,000 秒。

## Frozen experiment structure

- A：DBLP-AMiner-V18 与 IMDb-daily-20260722 上的真实标签 `<g,f>` 受控实验；`g=4..16,f=400`，并在 `g=10` 使用 `f={100,200,400,800,1600,3200}`，每个唯一单元 10 条。
- B：六个官方 SNAP 图、MovieLens-32M 与 Toronto-current 上的 related-group `g=4..16` 实验，每个 `(graph,g)` 固定 10 条。
- C：DBpedia-2022.12-en 的全部 464 条可行自然查询，以及 LinkedMDB-2012 上固定的 200 条官方 WikiMovies 自然查询；自然低和高 `g` 全部进入主实验。
- Secondary：tiny smoke、11 个 SteinLib WRP 已知最优值实例和四格固定消融。

机器可读矩阵在 [`experiments/paper_matrix.json`](experiments/paper_matrix.json)。主实验共有 164 个单元、2,064 条查询和 6,192 个逐方法任务。大文件由 `.gitignore` 排除；catalog、下载哈希、转换 manifest、查询选择和生成器保留在 Git。

## Correctness notes

零权边合法且原样保留，仓库包含专门的 `abhss_zero_weight_regression`。PrunedDP++ 论文式 pathmax + permanent-closed 语义已在 SteinLib 实例上出现非最优答案，因此只用于审计；正式 `pruneddp_safe` 关闭该 pathmax、保留 admissible lower bound，并允许更优 `g-cost` reopen。详见 [`docs/BASELINE.md`](docs/BASELINE.md)。

每条正式查询在计时前通过共同连通分量 gate。任何精确方法之间的目标值不一致都会冻结相应性能结论，不能把错误的小目标值解释为“更优”。

## Documentation

- [`docs/FULL_EXPERIMENT_PLAN.md`](docs/FULL_EXPERIMENT_PLAN.md)：A/B/C 设计、baseline、统计、timeout 与投稿 claim 边界。
- [`docs/EXPERIMENT_REVIEW_GUIDE.md`](docs/EXPERIMENT_REVIEW_GUIDE.md)：给人类 review 的逐图、逐实验细节和风险检查。
- [`docs/OFFICIAL_DATA_BUILD.md`](docs/OFFICIAL_DATA_BUILD.md)：官方来源、转换、哈希、查询生成和全量重建。
- [`docs/DATA_PROVENANCE.md`](docs/DATA_PROVENANCE.md)：投稿用数据来源契约。
- [`docs/METHOD.md`](docs/METHOD.md)：ABHSS 公共骨架、Light/Heavy 差异和精确性不变量。
- [`docs/ABHSS_LIGHT_HEAVY_REFACTOR.md`](docs/ABHSS_LIGHT_HEAVY_REFACTOR.md)：两版本重构审计。
- [`experiments/README.md`](experiments/README.md)：矩阵及 manifest 文件说明。
- [`RUN.md`](RUN.md)：可直接执行的命令清单。

第三方源码、论文 PDF、官方原始大文件和 build/result 目录不随仓库提交。公开 artifact 时需遵守各数据与代码许可；IMDb daily 文件尤其受 non-commercial 条款约束，不能假设可变 URL 会永久提供本冻结字节。
