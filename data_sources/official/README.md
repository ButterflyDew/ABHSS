# Official-upstream benchmark freeze

The formal replacement dataset is named `official-latest-20260722`.  Its
machine-readable source catalog is
[`experiments/official_sources.json`](../../experiments/official_sources.json).
It applies the following non-negotiable rules:

1. graph bytes come from the graph owner's official distribution endpoint;
2. a mutable `latest` endpoint is downloaded once and identified thereafter by
   its SHA-256, size, retrieval time and HTTP metadata;
3. source query collections are retained independently of graph identity;
4. synthetic queries are regenerated from a cited, versioned protocol and a
   predeclared seed;
5. a missing official source or underspecified construction causes exclusion,
   never fallback to a paper repository's processed graph.

## Download

The large payloads are ignored by Git.  The completed hashes and response
metadata are written to the tracked `download_manifest.json` next to `raw/`.
Interrupted files use the suffix `.part` and are resumed when supported.
If a redownload no longer matches an existing freeze-manifest entry, the new
response is moved to `.mismatch-<hash>` and the command fails without changing
the manifest. A newer upstream snapshot requires a new freeze ID.
On Windows the downloader defaults to `curl.exe` with Schannel so HTTPS is
validated against the Windows trust store; it never disables certificate
verification.  `--backend urllib` is available on hosts whose Python CA store
is correctly configured.

```powershell
python tools/data/download_official_sources.py --tier core
python tools/data/download_official_sources.py --tier extended
python tools/data/download_official_sources.py --tier full
```

An exact dataset can instead be selected with a repeatable `--dataset` option.
The converter never reads a URL directly; it reads only a file present in this
freeze and verifies it against the download manifest first.

## Directory contract

```text
data_sources/official/official-latest-20260722/
  download_manifest.json       tracked raw-file hashes and HTTP metadata
  raw/                         ignored official downloads
  extracted/                   ignored deterministic extraction products
  work/                        ignored conversion scratch space

data/official-latest-20260722/<dataset>/
  graph.txt                    ignored solver interface graph
  query*.txt                   ignored solver interface queries
  candidate_groups.txt         ignored normalized source groups
  dataset_manifest.json        tracked construction, counts and hashes
```

The old `data/` directories and `data_origin/` audit remain available only to
reproduce the earlier engineering probe.  They are not inputs to the new formal
matrix.
