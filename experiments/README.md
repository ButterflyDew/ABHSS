# 冻结实验控制文件

本目录是 SIGMOD/VLDB 实验的机器可读控制面。大图、展开后的查询和运行结果由 Git 忽略；所有正式输入都由这里的策略文件及 `experiment_data` 中的 manifest 固定身份。

| 文件 | 作用 |
|---|---|
| `paper_matrix.json` | 当前可执行矩阵：P1、P2、S2 与 correctness gates |
| `data_sources.json` | MonoGST+、GPU4GST、IMDb 和查询生成的来源总表 |
| `official_sources.json` | 仅 IMDb 2026-07-22 官方冻结的下载与转换定义 |
| `query_feasibility_audit.json` | 当前矩阵每个图/查询对的共同分量审计 |
| `environment_lock.json` | 方法、timeout、计时、报告与第三方版本契约 |
| `correctness_audit.json` | 已知最优值、零权边和 PrunedDP++ 复现证据 |
| `abhss_configuration_refactor_gate.json` | 单入口重构前后的小图、高 `g`、大图求解与加载非退化证据 |
| `scip-jack.set` | SCIP-Jack 单线程正确性 gate 配置 |

当前正式性能矩阵：

- P1：MonoGST+ 五图与 GPU4GST 八图的完整作者 workload，13 个图身份、8,318 条查询；
- P2：六张 GPU4GST 作者图上的 `g=5..16`，每格 5 条，共 360 条；
- S2：DBLP-MonoGSTPlus 与 IMDb-latest-20260722 上的 `<g,f>`，共 150 条；
- 计时项：同一 `abhss` 二进制的 Base、全增强配置，以及 PrunedDP++-Safe，均为单计算线程、每查询 10,000 秒。

消融、近似解质量、GPU/异构速度和旧 15 小时探针不属于当前冻结。改变矩阵、图、查询或生成器后，必须重建所有对应 manifest 与 `query_feasibility_audit.json`。Linux 正式性能环境先运行：

```bash
make validate-paper-binaries
python3 tools/experiments/validate_environment.py --deep --require-performance-binaries
```

`--require-performance-binaries` 只要求论文三项计时实际使用的两个二进制 `abhss` 和 `pruneddp`。恢复 Basic+ 与 SCIP-Jack 并执行 SteinLib 六方正确性核验时，才改用 `make validate-all-binaries`；两种模式不可混写。

完整口径见 [`../docs/EXPERIMENT_PLAN.md`](../docs/EXPERIMENT_PLAN.md)，执行步骤见 [`../RUN.md`](../RUN.md)。
