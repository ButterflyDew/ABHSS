# GPU4GST author data: audit-only status

The local GPU4GST author artifact was useful for understanding the related-group query generator and checking one recovered graph transform. It is not a formal input to the current paper.

## Why it is excluded

The current policy requires every graph to come from the graph owner's official distribution endpoint. The GPU4GST repository/OneDrive contains already processed graph products, sometimes sharing informal names with graphs from other papers. Equal names, vertex counts or edge counts do not establish snapshot/topology/weight identity. Heterogeneous GPU runtime is also outside this paper's single-thread CPU comparison model.

Therefore:

- `data_origin/`, `data/GPU4GST_*` and `third_party/GPU4GST-sigmod` are local engineering/audit material;
- author query CSVs and their CPU artifact do not appear in `experiments/paper_matrix.json`;
- no GPU speedup is reported in the formal tables;
- these large/unlicensed payloads are not redistributed in the public repo.

## Reused protocol, independently rebuilt data

The generic related-group generator in `tools/gpu4gst_data/prepare_gpu4gst.cpp --generic` is retained as source code. It now reads only a graph and candidate groups independently rebuilt from the official SNAP downloads. For each `(dataset,g)` it:

1. treats candidate groups as vertices of a co-occurrence graph;
2. selects a root group and explores related groups by BFS;
3. samples `g-1` reachable groups with a fixed derived seed;
4. rejects a query unless one original-graph component intersects all groups;
5. writes expanded 1-based groups plus the selected source group IDs.

The official freeze uses seed `20260722`, `g=4..16` and 300 source candidates per cell. The formal panel then chooses ten queries per cell using only pre-run mean-group-size rank strata. Experiment B applies this protocol to six official SNAP graphs plus MovieLens-32M and Toronto-current, for 1,040 formal queries. See [`OFFICIAL_DATA_BUILD.md`](OFFICIAL_DATA_BUILD.md) for source URLs, hashes and the exact selection rule.

Reusing an algorithmic query-generation method does not reuse the author's processed graph. The output identity is determined by the official graph hash, official feature/community groups, generator version, seed and emitted query hash.

## Limited semantic reconstruction audit

LinkedMDB is the one case where an official RDF rebuild was compared against a prior author interface. The endpoint multiset and all weights match exactly, which validates the recovered RDF transform; only the official RDF rebuild is eligible for use. Its natural panel is independently rebuilt from Meta ParlAI's official WikiMovies mirror, not from the author artifact. Details are in `experiments/graph_identity_audit.json`.
