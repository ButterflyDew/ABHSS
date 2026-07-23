# Frozen experiment configuration

This directory is the tracked control plane for the SIGMOD/VLDB experiment artifact. Large source archives, converted graphs, queries and result records are intentionally ignored; their identities are pinned here by hashes and manifests.

| File | Purpose |
|---|---|
| `paper_matrix.json` | executable A/B/C primary matrix plus secondary gates and ablation |
| `official_sources.json` | official URLs, snapshot names, source roles and exact transforms |
| `data_snapshot.json` | hashes and counts for raw downloads, interfaces and fixed panels |
| `query_feasibility_audit.json` | common-component gate for every graph/query pair in the matrix |
| `environment_lock.json` | timeout, scope, required correctness dependencies and reporting contract |
| `probe_15h_plan.json` | predeclared 15-hour feasibility-probe strata and diagnostic policy |
| `correctness_audit.json` | known-optimum and PrunedDP++ reproduction evidence |
| `graph_identity_audit.json` | same-name graph policy and LinkedMDB reconstruction evidence |
| `scip-jack.set` | single-thread SCIP-Jack correctness-gate settings |

The primary experiment families are:

- A: controlled real-label `<g,f>` panels on DBLP-AMiner-V18 and IMDb-daily-20260722;
- B: related-group cross-`g` panels on six SNAP graphs, MovieLens-32M and Toronto-current;
- C: all feasible natural DBpedia queries and a fixed 200-query LinkedMDB/WikiMovies panel, including natural low and high `g`.

The only formal performance methods are ABHSS-Light, ABHSS-Heavy and PrunedDP++-Safe, all single-compute-thread with a 10,000-second solver deadline. Light and Heavy remain separate; no per-instance best-of-two is permitted.

Before a long run, rebuild `query_feasibility_audit.json` and `data_snapshot.json`, then run `tools/experiments/validate_environment.py --deep-snapshot`. A changed matrix intentionally invalidates the old feasibility audit until it is regenerated.

Old author-repository graph/query products, synthetic MovieLens controls, GPU/heterogeneous timing and approximate-only quality comparisons are excluded from the formal matrix. They may remain locally for parser or provenance audits but must not enter aggregate tables.
