# Data provenance policy

## Formal rule

Every formal graph is rebuilt from the latest official source available when freeze `official-latest-20260722` was created. A paper-author GitHub artifact is never used as the graph source. If an official query collection exists, it is used; otherwise the query generator follows the cited paper's source-feature protocol. Every raw file, redirect, byte count, retrieval time and SHA-256 is recorded in `experiments/official_sources.json` and the local download manifest.

An informal graph name is not an identity. Two files called DBLP, IMDb, Toronto or LinkedMDB are interchangeable only if their source snapshot, transform and graph hash agree. Current official rebuilds therefore use explicit names such as `DBLP-AMiner-V18`, `IMDb-daily-20260722` and `Toronto-current` and are never presented as byte-identical historical paper graphs.

## Formal datasets and query roles

| Dataset | Graph source | Query source/protocol | Main role |
|---|---|---|---|
| DBLP-AMiner-V18 | official AMiner DBLP Citation Network V18 | fixed-seed real-label controlled panel | A: `<g,f>` control |
| IMDb-daily-20260722 | official IMDb non-commercial daily files | fixed-seed real-label controlled panel | A: `<g,f>` control |
| SNAP Wikipedia/Twitch/GitHub | official SNAP feature graphs | source feature groups, related-group BFS | B: cross `g` |
| SNAP YouTube/Orkut/LiveJournal | official SNAP graphs and communities | source community groups, related-group BFS | B: cross `g` |
| MovieLens-32M | official GroupLens 32M | official genre groups, related-group BFS | B: cross `g` |
| Toronto-current | official Toronto Centreline V2 and POI | official POI categories, related-group BFS | B: cross `g` |
| DBpedia-2022.12-en | official DBpedia objects and labels | official DBpedia-Entity v2 stopped queries | C: natural queries |
| LinkedMDB-2012 | official LinkedMDB RDF dump | Meta ParlAI official WikiMovies mirror | C: natural queries |
| SteinLib WRP3/WRP4 | official SteinLib archives | converted terminal groups | secondary correctness gate |

## Query identity and selection independence

A uses real source labels. The graph-construction family follows PrunedDP++, but this paper chooses `g=4..16` at `f=400`, `f={100,200,400,800,1600,3200}` at `g=10`, and 10 queries per unique cell. These choices are not described as a replication of the historical parameter table.

B generates 300 candidates per `(dataset,g)` from source groups and chooses 10 by a fixed, solver-independent stratification over observed mean group size. MovieLens uses genre semantics and Toronto uses POI category semantics; neither uses a repository-invented random group collection.

C preserves source language. DBpedia retains all 464 component-feasible mappings at `g=1..12`. LinkedMDB fixes 200 WikiMovies queries using a pre-run stratified policy and retains every unique natural mapping at `g=9..12`. Source IDs, normalized tokens, group IDs, rejected disconnected rows and hashes remain in manifests.

No formal query is selected, removed or replaced using runtime, memory, completion status or objective value. The common-component gate is permitted because it tests whether the mathematical instance is feasible, before any solver runs.

## LinkedMDB query archive

The original MovieQA distribution endpoint cited by older code is retired. Meta ParlAI distributes an official WikiMovies mirror whose 57,070,041-byte archive has SHA-256 `ed062b49922b602ebee6073f58951bf38c4772a8b53d46682f3ff80ed57de948`, equal to the recorded original archive. The formal panel uses that mirror and records both the active and retired provenance paths; old author-repository converted queries are not inputs.

## Legacy artifacts

Directories such as `data/GPU4GST_*`, old `data/{Toronto,MovieLens,DBLP,LinkedMDB,DBpedia}` and author-provided queries remain useful for parser tests and semantic reconstruction audits. They are excluded from `experiments/paper_matrix.json` and must not enter formal aggregates. The retired synthetic MovieLens `<g,f>` panel is likewise outside the matrix.

## Publication checklist

- Report the full dataset ID, snapshot description and graph SHA-256.
- State that DBLP/IMDb reproduce a graph-construction family, not historical bytes or parameter values.
- Report A's target and realized `f` separately.
- Call B's group size `f_observed`, not controlled `f`.
- Include every natural low/high `g` stratum in C and show its sample count.
- Archive redistributable inputs or converted interfaces under their source terms; IMDb files remain subject to the non-commercial dataset terms.
- Rebuild the source snapshot and common-component audit after any catalog, converter, panel or matrix change.
