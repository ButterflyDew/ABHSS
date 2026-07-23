# Experiment status and historical boundary

The current submission matrix is `experiments/paper_matrix.json` and is organized as primary experiments A/B/C plus secondary correctness and ablation. Older result directories and draft E0/E1/E2/E3/E5 names are historical development evidence only.

## Current formal scope

- A: controlled real-label `<g,f>` experiments on DBLP-AMiner-V18 and IMDb-daily-20260722;
- B: related-group `g=4..16` experiments on six SNAP graphs, MovieLens-32M and Toronto-current;
- C: natural DBpedia and LinkedMDB queries, including every represented low/high `g` stratum;
- secondary: tiny smoke, SteinLib known-optimum gate and a four-cell fixed ablation.

All performance claims compare ABHSS-Light and ABHSS-Heavy separately with PrunedDP++-Safe under one compute thread and a 10,000-second deadline.

## Historical results that remain useful

Past probes may be used to diagnose zero-weight handling, unsafe pathmax semantics, parser behavior, memory peaks and Light/Heavy refactoring. They may not be combined with the current freeze because graph identities, query panels, binaries or timeouts differ.

## Retired designs

The following are explicitly retired from paper aggregation:

- author-repository graphs or queries used merely because their display name matches an official graph;
- synthetic uniform-vertex MovieLens `<g,f>` control;
- separate main/appendix treatment that hides natural low or high `g` queries;
- old DBLP comparisons based only on matching `n/m`;
- GPU/heterogeneous timing without comparable hardware and preprocessing boundaries;
- approximate-algorithm quality-only tables.

## Results hygiene

Every result record must identify the matrix, graph, query, executable, commit, timeout and machine. Formal summaries retain timeout/OOM/error denominators, check objective agreement before timing comparisons, and never create a per-query Light/Heavy oracle. Any selective diagnostic rerun remains labelled diagnostic; paper-value reruns cover all methods in the complete predeclared cell.
