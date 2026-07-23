# IMDb 2026-07-22 官方冻结

当前正式矩阵中，只有 S2 的 IMDb 图需要从最新官方源独立构建。P1/P2 使用 MonoGST+ 与 GPU4GST 作者实验数据，不再由本目录的通用官方下载流程替换。

冻结名为 `IMDb-latest-20260722`。原始来源是 IMDb non-commercial daily exports：

- `title.basics.tsv.gz`
- `title.principals.tsv.gz`
- `name.basics.tsv.gz`

下载 URL、响应信息、字节数与 SHA-256 固定在：

- `experiments/official_sources.json`
- `data_sources/official/official-latest-20260722/download_manifest.json`

原始压缩包和展开文件由 Git 忽略。Linux 恢复三个已冻结文件后运行：

```bash
make tools JOBS=16
python3 tools/data/prepare_imdb_dataset.py --replace-existing
```

Windows 可用 `cmake --build build --config Release --target build_imdb_graph` 编译转换器，再把上面的 `python3` 改为 `python`。

转换结果位于 `data/official-latest-20260722/imdb-20260722/graph.txt`：title 和 person 为两类顶点，`title.principals` 形成权重为 1 的无向边，最终使用稠密 1-based ID。`dataset_manifest.json` 固定原始文件哈希、转换规则与最终点边数/图哈希。

IMDb URL 是 mutable `latest`。不得把日后下载的新内容写回同一 freeze ID；新内容必须使用新日期、新 manifest，并完整重跑 S2。数据受 IMDb non-commercial 条款约束，不能仅因本地存在而上传到公开 artifact。
