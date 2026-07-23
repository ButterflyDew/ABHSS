# Official-source data freeze and interface build

## 1. Scope and identity rule

Freeze `official-latest-20260722` contains only graph-owner official downloads. A formal identity is the tuple `(official source snapshot, raw SHA-256, ordered transform, interface SHA-256)`; an informal name such as DBLP or Toronto is insufficient. Current rebuilds use qualified names and are not claimed byte-identical to historical paper graphs.

If a source query collection exists, it is used. Otherwise, queries are regenerated from official source features/groups using the cited related-group protocol. Every executable query must pass a solver-independent common-component gate before timing.

The complete URL list, redirect metadata, requested/final URL, retrieval UTC, bytes and hashes are machine-readable in `experiments/official_sources.json` and `data_sources/official/official-latest-20260722/download_manifest.json`.

## 2. Official sources and formal role

| Dataset ID | Frozen official source | Formal role | Query origin |
|---|---|---|---|
| `DBLP-AMiner-V18` | AMiner DBLP Citation Network V18, object modified 2025-07-14 | A controlled | real source labels |
| `IMDb-daily-20260722` | IMDb daily non-commercial export retrieved 2026-07-22 | A controlled | real source labels |
| `SNAP-Wikipedia-2018` | English Wikipedia article networks, Dec. 2018 | B cross `g` | official node features |
| `SNAP-Twitch-2018` | Twitch social networks, May 2018 | B cross `g` | official node features |
| `SNAP-GitHub-2019` | GitHub social graph, June 2019 | B cross `g` | official node features |
| `SNAP-YouTube` | official SNAP graph and top-5000 communities | B cross `g` | official communities |
| `SNAP-Orkut` | official SNAP graph and top-5000 communities | B cross `g` | official communities |
| `SNAP-LiveJournal` | official SNAP graph and top-5000 communities | B cross `g` | official communities |
| `MovieLens-32M` | GroupLens 32M, generated 2023-10-13/released 2024 | B cross `g` | official genres |
| `Toronto-current` | complete Centreline V2 modified 2026-07-21 and POI modified 2026-02-20 | B cross `g` | official POI categories |
| `DBpedia-2022.12-en` | DBpedia 2022.12.01 English objects and labels | C natural | DBpedia-Entity v2 stopped queries |
| `LinkedMDB-2012` | official LinkedMDB dump dated 2012-02-10 | C natural | Meta ParlAI WikiMovies mirror |
| `SteinLib-WRP` | official WRP3/WRP4 archives | secondary correctness | published terminal structure |

The old author-repository graph/query products are not fallback inputs. The retired WikiMovies origin is the only special case: Meta ParlAI's active official mirror has the exact recorded original archive SHA-256, so it is a source-preserving mirror rather than an independently reconstructed query set.

## 3. Repository interface

Each converted graph directory contains `graph.txt` with header `n m` followed by `u v w`, using dense 1-based vertex IDs and nonnegative edge weights. `candidate_groups.txt` contains `g<ID>: <vertex IDs>`. Query files contain a query count, then for each query its `g` and `g` nonempty vertex sets. Parallel edges are retained only when the declared transform requires relation multiplicity; self-loops are removed.

Large graph, group, expanded query and raw source files are ignored by Git. Compact dataset manifests, source-normalization/build statistics, the official source catalog, panel manifests and hashes are tracked. `tools/data/finalize_official_dataset.py` validates every row before writing `dataset_manifest.json`.

## 4. Exact graph transforms

### 4.1 DBLP-AMiner-V18

Stream all 6,729,828 AMiner JSONL paper records twice. In pass 1, allocate one vertex per paper and one per distinct author identity, preferring the source author ID and otherwise using normalized name+organization. Emit one unit edge per distinct paper-author relation. In pass 2, emit one unit edge for each distinct non-self reference whose paper ID is present in the snapshot. Extract lower-case ASCII alphanumeric labels from title, keywords, venue, document type and author name, deduplicated per vertex; retain frequencies `50..4800`.

The resulting graph has 6,729,828 paper vertices, 5,447,921 author vertices, 22,987,638 authorship edges and 73,640,521 citation edges. The build records 1,990,160 fallback author identities, 32 authors without usable identity, 10,343 duplicate within-paper author rows and 432 out-of-snapshot references. It does not substitute the Practical-paper coauthor graph or a GPU4GST processed DBLP file.

### 4.2 IMDb-daily-20260722

Scan frozen `title.principals.tsv`; assign title/person dense IDs at first occurrence and preserve every source row as one unit-weight title-person edge. Attach labels from `title.basics.tsv` and `name.basics.tsv`: title type, primary/original title, genres, primary name and profession. Tokenize into lower-case ASCII alphanumeric runs, deduplicate per vertex and retain frequency `50..4800`. Person IDs present in principals but absent from name metadata remain valid graph vertices but add no text labels.

This is the PrunedDP++ graph-construction family on a current official snapshot, not its unavailable 2016 IMDb bytes. The complete title/person counts, metadata coverage and largest component are recorded in `source_normalization.json`.

### 4.3 SNAP feature/community graphs

Wikipedia is the disjoint union of the official Chameleon, Crocodile and Squirrel feature graphs; Twitch is the disjoint union of DE, ENGB, ES, FR, PTBR and RU. GitHub is one official feature graph. Self-loops are removed and undirected pairs are deduplicated. Each edge receives integer Jaccard distance `round(100 × (1 - |F(u)∩F(v)|/|F(u)∪F(v)|))`; an official binary feature defines a candidate group.

YouTube, Orkut and LiveJournal use the official undirected edge list and top-5000 community file. Their edge weight is the same integer Jaccard distance, now computed from community membership sets. Zero distances are legal and preserved.

### 4.4 MovieLens-32M

A vertex is a movie. For every user, collect distinct movies rated exactly 5.0; connect every co-rated pair, globally deduplicate the undirected pair and assign unit weight. Each official movie genre is one source candidate group. Movies outside the 5-star co-rating core remain vertices if present in the movie catalog, so connected-component feasibility is checked at query generation.

### 4.5 Toronto-current

Read the complete EPSG:2952 Centreline V2 CSV, retain rows with two valid endpoint intersections, and assign dense IDs to all retained intersections. For duplicate undirected endpoint pairs, keep the smallest positive polyline length. Round the metric length to the nearest positive integer millimetre; no traffic vertex value is folded into the edge weight because this repository solves edge-weighted GST.

Map all 895 official Cultural Hotspot POI points to their exact nearest retained intersection in EPSG:2952. Prefixed `Interests` and `Neighbourhood` values form source groups. The build records every POI assignment and the nearest-distance distribution; no distance threshold or partial road download is used.

### 4.6 DBpedia and LinkedMDB

Discard `rdf:type`, non-URI objects and self-loops. Preserve every remaining URI-URI triple as an undirected edge, including parallel edges from distinct predicate occurrences. For predicate `p`, assign weight `ln(1 + freq(p))`, where the frequency is counted over the retained frozen stream. Candidate groups are lower-case label tokens; DBpedia restricts materialized groups to the official query vocabulary, while LinkedMDB materializes all label tokens.

As an independent transform audit, the official LinkedMDB rebuild has the same undirected endpoint multiset and every edge weight as the historical author interface (`max_abs_error=0`); only the official rebuild is eligible for formal use.

## 5. Frozen graph statistics

The final values below are also stored with full SHA-256 in each `dataset_manifest.json`.

| Interface | `n` | `m` | candidate groups | memberships | edge weights | zero edges |
|---|---:|---:|---:|---:|---|---:|
| `dblp-aminer-v18` | 12,177,749 | 96,628,159 | 57,727 | 24,701,679 | unit | 0 |
| `imdb-20260722` | 18,588,661 | 100,556,350 | 61,020 | 19,661,596 | unit | 0 |
| `snap-wikipedia-2018` | 19,109 | 400,497 | 13,183 | 1,060,946 | 0..100 | 4 |
| `snap-twitch-2018` | 34,118 | 429,113 | 3,163 | 687,900 | 30..100 | 0 |
| `snap-github-2019` | 37,700 | 289,003 | 4,005 | 690,358 | 19..100 | 0 |
| `snap-youtube` | 1,134,890 | 2,987,624 | 5,000 | 72,959 | 4..100 | 0 |
| `snap-orkut` | 3,072,441 | 117,185,083 | 5,000 | 1,078,576 | 1..100 | 0 |
| `snap-livejournal` | 3,997,962 | 34,681,189 | 5,000 | 139,012 | 0..100 | 2,037 |
| `movielens-32m` | 87,585 | 44,143,611 | 20 | 154,170 | unit | 0 |
| `toronto-current` | 44,184 | 63,400 | 59 | 1,865 | positive millimetres | 0 |
| `dbpedia-2022.12-en` | 7,958,883 | 22,787,803 | 1,176 | 2,824,808 | 0.693147..14.959362 | 0 |
| `linkedmdb` | 1,326,784 | 2,132,796 | 292,816 | 1,672,764 | 0.693147..13.147928 | 0 |

Toronto source accounting is 64,400 centreline rows, 64,307 retained rows, 93 discarded rows, 907 duplicate endpoint rows and 63,400 unique edges. Its 895 POIs have nearest-road-node distance min 0.570 m, median 36.636 m, p95 159.984 m and max 1,082.604 m.

## 6. Query construction

### 6.1 A controlled real-label panels

For DBLP and IMDb, the `g` sweep is every integer `4..16` at `f_target=400`. The `f` sweep fixes `g=10` and uses `f_target={100,200,400,800,1600,3200}`. Overlap is not duplicated, so each graph has 18 cells. For each cell, select 10 deterministic random real-label sets; accept only a set whose mean source-label frequency is within 2% of target, otherwise retain the closest of 2,000 attempts for the environment validator to reject if it remains outside tolerance. Store selected group IDs, tokens, sizes, derived seed and realized `f`.

This is this paper's parameter design. Only the graph-construction family, not the old parameter values or 50-query sample count, is attributed to PrunedDP++.

### 6.2 B related-group panels

For each of eight graphs and every `g=4..16`, generate 300 source candidates with seed 20260722. Candidate groups form a co-occurrence graph; select a root, traverse related groups by BFS and reject any query without one original-graph component intersecting all groups. Rank candidates by `log1p(f_observed)`, divide ranks into ten strata and choose one stable-hash item per stratum. This yields `8 × 13 × 10 = 1,040` formal queries without solver feedback.

### 6.3 DBpedia natural queries

Use official `queries-v2_stopped.txt` without applying a second repository stopword pass. Of 467 source rows, one maps to `g=0`; two mapped rows (`INEX_XER-109`, source index 124, and `QALD2_tr-17`, source index 234) fail the common-component gate. Preserve their IDs and reason in the audit. All remaining 464 queries at `g=1..12` are primary and unsampled.

### 6.4 LinkedMDB natural queries

Extract train/dev/test WikiMovies questions from Meta ParlAI's 57,070,041-byte official mirror (SHA-256 `ed062b49922b602ebee6073f58951bf38c4772a8b53d46682f3ff80ed57de948`). Remove punctuation and the frozen English stopword list, then exactly map retained tokens to LinkedMDB label groups. The fixed 200-query quota is `{1:20,2:20,3:20,4:20,5:20,6:20,7:20,8:25,9:26,10:6,11:2,12:1}`. Keep all 35 unique `g=9..12` mappings; choose low `g` by stable hash and replace only component-infeasible rows without solver feedback.

## 7. End-to-end build

```powershell
python tools/data/download_official_sources.py --rehash
cmake --build build --config Release --parallel
python tools/data/rebuild_official_freeze.py
python tools/data/build_query_feasibility_audit.py
python tools/data/build_official_snapshot.py
python tools/experiments/validate_environment.py --deep-snapshot
```

Stages can be resumed with repeated `--stage` arguments. The rebuild supervisor extracts AMiner, IMDb, RDF, WikiMovies and MovieLens inputs; builds the twelve interfaces; generates A/B/C panels; finalizes per-dataset manifests; then writes the compact tracked snapshot.

## 8. Verification and archival notes

The validator checks catalog state, raw byte counts/hashes, all interface manifests, the exact A grid and ±2% realized-`f` condition, the exact B dataset/`g` cross product, LinkedMDB quotas, DBpedia source exclusions, matrix expansion, query payload hashes, connected-component feasibility and required binaries. `--deep-snapshot` rereads every large payload and is intentionally slow.

The pipeline is storage-heavy: AMiner alone is 5.33 GB compressed and 15.94 GB extracted. Before artifact publication, deposit redistributable frozen inputs or converted interfaces in an archival store under each owner's terms. IMDb's mutable daily bytes and non-commercial terms require particular care.
