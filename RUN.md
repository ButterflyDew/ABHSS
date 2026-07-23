# Build and run the frozen paper experiments

The formal target is a SIGMOD/VLDB single-compute-thread exact GST evaluation. The machine-readable source of truth is `experiments/paper_matrix.json`; the prose rationale is in `docs/FULL_EXPERIMENT_PLAN.md` and the dataset-level review sheet is in `docs/EXPERIMENT_REVIEW_GUIDE.md`.

## 1. Build

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The formal binaries are `abhss_light`, `abhss_heavy` and `pruneddp`. The build also creates correctness-gate binaries, ablations, official-data converters and the component-feasibility auditor.

## 2. Download and rebuild the official freeze

```powershell
python tools/data/download_official_sources.py --rehash
python tools/data/rebuild_official_freeze.py
```

Individual stages can be resumed without repeating finished downloads:

```powershell
python tools/data/rebuild_official_freeze.py --stage extract
python tools/data/rebuild_official_freeze.py --stage interfaces
python tools/data/rebuild_official_freeze.py --stage queries
python tools/data/rebuild_official_freeze.py --stage finalize
```

The pipeline uses only the official URLs and transforms frozen in `experiments/official_sources.json`. Large raw archives and interface files are ignored by Git; hashes, retrieval metadata and transformations remain tracked. Never delete a frozen manifest and redownload a mutable endpoint under the same freeze ID.

## 3. Build the input gate and validate

```powershell
python tools/data/build_query_feasibility_audit.py
python tools/data/build_official_snapshot.py
python tools/experiments/validate_environment.py --deep-snapshot
```

For a quick development check, omit `--deep-snapshot`; this still checks paths, counts, manifests, panel grids and matrix expansion but does not rehash every multi-gigabyte payload.

## 4. Correctness gates

```powershell
python tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S0_smoke
python tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S1_steinlib_exactness_gate
```

Do not start formal timing unless every available exact method agrees with the converted SteinLib optimum within the configured tolerance.

## 5. Optional 15-hour feasibility probe

```powershell
python tools/experiments/run_probe_15h.py --hours 15
```

The probe uses separate diagnostic binaries, short timeouts and a predeclared A/B/C sample. It is a submission-risk check, not paper evidence. Its outcome-adaptive follow-ups are labelled diagnostic and excluded from claim estimates.

## 6. Main A/B/C run

Use one run directory so resume and duplicate detection work across suites:

```powershell
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite A_controlled_dblp
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite A_controlled_imdb
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite B_related_cross_g
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite C_dbpedia_natural
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite C_linkedmdb_natural
```

Run the fixed secondary ablation separately so it cannot be confused with the primary comparison:

```powershell
python tools/experiments/run_experiments.py --run-id paper_ablation --run-dir results/paper_runs/paper_ablation --suite S2_ablation
```

The formal solver timeout is 10,000 seconds for all three performance methods. ABHSS-Light and ABHSS-Heavy are separate curves; the runner never forms a per-instance oracle.

## 7. Stable sharding and resume

For multiple identical workers, assign every worker the same shard count and a distinct shard index:

```powershell
python tools/experiments/run_experiments.py --run-id paper_main --run-dir results/paper_runs/paper_main --suite B_related_cross_g --shards 4 --shard-index 0
```

Tasks are assigned by a stable key. Reusing the same run directory skips completed records. Do not run memory-bandwidth-heavy shards concurrently on the same physical machine when producing formal timing.

## 8. Targeted diagnosis

Targeted runs are allowed for debugging but are not an alternative sampling rule:

```powershell
python tools/experiments/run_experiments.py --run-id diagnose --run-dir results/diagnose --case A_controlled_imdb__IMDb-daily-20260722_10_400 --query-index 1 --method abhss_light --method pruneddp_safe
```

If a query is rerun for a paper value, rerun all three primary methods for the complete predeclared cell and retain the earlier records.

## 9. Summaries

```powershell
python tools/experiments/summarize_results.py --run-dir results/paper_runs/paper_main
python tools/experiments/analyze_probe_results.py --run-dir results/paper_runs/<probe-id>
```

Paper tables must retain timeout/OOM/error denominators and report both query-weighted and dataset-equal-weight aggregation where applicable.

## 10. Suite names

```text
S0_smoke
S1_steinlib_exactness_gate
A_controlled_dblp
A_controlled_imdb
B_related_cross_g
C_dbpedia_natural
C_linkedmdb_natural
S2_ablation
```

The formal matrix contains no old paper-author graph/query payload, synthetic MovieLens control, GPU/heterogeneous timing comparison or approximate-only quality table.
