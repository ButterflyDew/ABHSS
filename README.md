# ABHSS exact GST paper environment

这是面向 SIGMOD/VLDB Research Track 的**纯单线程精确 Group Steiner Tree** 全量实验包。当前冻结矩阵包含 239 个 case、8,587 个逐方法实例任务，统一使用 10,000 秒逐询问上限。

环境包括：

- ABHSS-Light、ABHSS-Heavy 与三个结构消融；
- 主精确 baseline `PrunedDP++-Safe`；
- DPBF、Basic+、GPU4GST 作者 CPU PrunedDP++ artifact、SCIP-Jack；
- Practical 原询问、严格受控 `(g,f)`、GPU4GST 分层面板、自然关键词询问与 SteinLib 已知最优值实例；
- 可恢复、逐实例 watchdog、稳定分片、结果一致性检查、PAR-2/配对汇总和论文图生成。

首先运行：

```powershell
python tools/experiments/validate_environment.py
python tools/experiments/run_experiments.py --dry-run
```

文档入口：

- [`docs/FULL_EXPERIMENT_PLAN.md`](docs/FULL_EXPERIMENT_PLAN.md)：最终投稿实验方案、baseline 取舍、统计和 claim 边界；
- [`docs/DATA_PROVENANCE.md`](docs/DATA_PROVENANCE.md)：每份图/询问的来源、身份、转换和 `g/f` 协议；
- [`RUN.md`](RUN.md)：构建、运行、恢复、分片、汇总；
- [`docs/METHOD.md`](docs/METHOD.md)：ABHSS 方法与证明材料；
- [`docs/BASELINE.md`](docs/BASELINE.md)：PrunedDP++ 复现歧义与正确性审计；
- [`third_party/README.md`](third_party/README.md)：外部 baseline 版本、构建和许可。

重要边界：两份相同规模的 DBLP 图并不相同，论文 ID 固定为 `DBLP-VEW21` 与 `DBLP-GPU25`；询问不得跨图迁移。论文 pathmax 重实现已在三个 WRP 已知最优值实例上产生错误，只保留为 audit，不能作为精确主 baseline。

## 公共仓库的数据边界

GitHub 仓库不直接再分发体积约 20 GiB 的原始/转换图、第三方源码与编译产物，也不包含受版权保护或尚未公开的论文 PDF。`experiment_data/gpu4gst_panel` 中约 239 MiB 的展开查询文本同样不入库；固定选择 JSON 和生成脚本仍被保留。

克隆后请按以下入口恢复完整内部实验环境：

- [`data_origin/README.md`](data_origin/README.md)：官方数据下载、哈希和转换；
- [`docs/DATA_PROVENANCE.md`](docs/DATA_PROVENANCE.md)：数据来源与格式映射；
- [`third_party/README.md`](third_party/README.md)：外部 baseline 的冻结版本、许可与构建；
- [`tools/data/`](tools/data/)：受控询问及 GPU4GST 面板的确定性生成器。

仓库保留 tiny smoke graph、SteinLib 小实例、受控/原协议查询、数据 manifest 和完整实验配置；恢复被忽略的大图与第三方依赖后，再运行环境校验与正式矩阵。
