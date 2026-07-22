# Paper experiment manifests

- `paper_matrix.json`：唯一全量矩阵，timeout=10,000 秒；
- `environment_lock.json`：外部源码 commit、归档哈希和许可状态；
- `data_snapshot.json`：图、原查询和来源归档快照；
- `graph_identity_audit.json`：同名图的逐边身份判定；
- `correctness_audit.json`：已知最优值交叉验证与 pathmax 反例；
- `scip-jack.set`：SCIP-Jack 单线程/10,000 秒配置。

最小检查：

```powershell
python tools/experiments/validate_environment.py
python tools/experiments/run_experiments.py --dry-run
```

正式协议与 suite 角色见 [`../docs/FULL_EXPERIMENT_PLAN.md`](../docs/FULL_EXPERIMENT_PLAN.md)。不要手改生成后的 panel 文件；修改生成协议后必须重建 manifest、更新快照并创建新的 paper run id。
