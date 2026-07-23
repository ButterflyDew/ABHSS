# Fast Optimal Group Steiner Tree Search using GPUs 数据说明

## 1. 作者下载来源、归档与当前状态

- 论文作者仓库：<https://github.com/toziki/GPU4GST-sigmod>
- 作者完整数据目录（OneDrive）：<https://1drv.ms/f/c/683d9dd9f262486b/Ek6Fl_brQzhDnI2cmhGIHxMBQ-L1ApeSqxwZKE4NBsDXSQ?e=3RBc8S>
- 论文：*Fast Optimal Group Steiner Tree Search using GPUs*（SIGMOD 2025）

作者仓库的 `README.md` 规定共有 8 个数据集，每个数据集包含 8 个文件。仓库本身只提交了 Twitch，完整成品放在上述 OneDrive。

截至 2026-07-17，研究环境曾从作者 OneDrive 的 `GSTdata` 文件夹逐文件下载全部 64 个成品文件，总计 7,809,970,520 字节。下载时取得的 OneDrive 文件 ID、文件长度、修改时间和 QuickXorHash 保存在 `OFFICIAL_ONEDRIVE_MANIFEST.json`；全量 SHA-256 保存在 `SHA256SUMS.txt`。

2026-07-23 仓库清理时，原始大文件已移入根目录归档 `ABHSS_ignored_data_before_workload_redesign_20260723.tar.gz`，本目录只保留来源元数据、作者 CSV 和核验脚本；当前正式求解接口位于 `data/GPU4GST_*`。需要逐文件重新核验时，先从归档恢复原始文件，再运行：

```powershell
powershell -ExecutionPolicy Bypass -File data_origin/verify_data.ps1
```

`verify_data.ps1` 会同时检查 OneDrive 文件长度、64 项 SHA-256 与作者成品内部的 CSR 结构。只调试结构检查时可以加 `-SkipSha256`，但正式验收不应跳过。必须区分“下载完整”与“作者成品有效”：64 个文件与 OneDrive 元数据及 SHA-256 逐一匹配，但作者上传的 Orkut 两个二进制文件本身截断，详见第 8 节。正式接口从完整 `Orkut.in` 重建，不使用截断二进制。`download_author_data.ps1` 仅作为缺文件时的官方入口提示。

各数据集在 OneDrive 中的 8 个文件总大小如下：

| 数据集 | 文件数 | 总字节数 |
|---|---:|---:|
| DBLP | 8 | 1,211,958,687 |
| Github | 8 | 17,785,602 |
| LiveJournal | 8 | 1,837,105,572 |
| Musae | 8 | 24,817,015 |
| Orkut | 8 | 3,731,896,986 |
| Reddit | 8 | 806,828,345 |
| Twitch | 8 | 24,437,688 |
| Youtube | 8 | 155,140,625 |

## 2. 论文中的 8 个数据集

下表来自论文第 8.1 节 Table 1。注意文件前缀严格使用作者代码中的拼写，例如 `Github` 和 `Youtube`。

| 文件前缀 | 顶点数 | 无向边数 | 候选组数 | 原始来源 |
|---|---:|---:|---:|---|
| `Musae` | 19,109 | 400,497 | 13,183 | SNAP Wikipedia Article Networks |
| `Twitch` | 34,118 | 429,113 | 3,163 | SNAP Twitch Social Networks |
| `Github` | 37,700 | 289,003 | 4,005 | SNAP GitHub Social Network |
| `Youtube` | 1,134,890 | 2,987,624 | 8,385 | SNAP Youtube communities |
| `DBLP` | 2,423,455 | 12,786,329 | 127,726 | AMiner |
| `Orkut` | 3,072,440 | 117,185,083 | 6,288,363 | SNAP Orkut communities |
| `LiveJournal` | 3,997,962 | 34,681,189 | 664,414 | SNAP LiveJournal communities |
| `Reddit` | 4,262,834 | 12,502,767 | 1,146,657 | Pushshift Reddit API |

原始来源链接：

- SNAP 数据总表：<https://snap.stanford.edu/data/>
- Musae：<https://snap.stanford.edu/data/wikipedia-article-networks.html>
- Twitch：<https://snap.stanford.edu/data/twitch-social-networks.html>
- GitHub：<https://snap.stanford.edu/data/github-social.html>
- Youtube：<https://snap.stanford.edu/data/com-Youtube.html>
- Orkut：<https://snap.stanford.edu/data/com-Orkut.html>
- LiveJournal：<https://snap.stanford.edu/data/com-LiveJournal.html>
- AMiner：<https://www.aminer.org/data>
- Pushshift API：<https://github.com/pushshift/api>

## 3. 每个数据集的 8 个必需文件

以 `Twitch` 为例：

1. `Twitch.in`：可读文本图。首行为 `|V| |E|`；以后每行是 `u v w`，每条无向边只出现一次。
2. `Twitch_beg_pos.bin`：CSR 起始位置，共 `|V|+1` 个 64 位整数。
3. `Twitch_csr.bin`：CSR 邻接点。无向图被展开为两个方向，因此实际共有 `2|E|` 个 64 位整数。
4. `Twitch_weight.bin`：与 CSR 邻接项一一对应的权重，共 `2|E|` 个 64 位整数。
5. `Twitch.g`：候选组。格式为 `g<组号>:<顶点列表>`。
6. `Twitch3.csv`：组数为 3 的 300 个查询。
7. `Twitch5.csv`：组数为 5 的 300 个查询。
8. `Twitch7.csv`：组数为 7 的 300 个查询。

注意两套编号并不相同：`.g` 的文本标签写成 `g1,g2,...`，但 `3/5/7.csv` 中保存的是 **0-based 的 `.g` 行号**。例如 CSV 中的 `0` 对应 `.g` 第一行 `g1`。这不是根据 README 示例猜出的：作者仓库 commit [`716a19c`](https://github.com/toziki/GPU4GST-sigmod/tree/716a19c240c480cb2d23435bbaca55163a48e174) 的 `read_Group` 明确把 `gN` 存到 `group_graph[N-1]`，`read_inquire` 则原样保留 CSV 整数，求解时直接以该整数索引 `group_graph`。仓库转换工具会把两者统一为可审核的 1-based group id。

作者的转换程序还会生成 `_deg.bin` 和 `_head.bin`，但作者的 `test_datasets.sh`、GPU/CPU 求解程序和“每个数据集 8 个文件”的清单均不要求这两个文件，所以本目录不把它们算作论文数据的必需部分。

## 4. 边权生成

论文说明，边权与相邻顶点邻域之间的 Jaccard 距离成比例：

$$
d_J(u,v)=1-\frac{|N(u)\cap N(v)|}{|N(u)\cup N(v)|}.
$$

论文没有明写把浮点距离转成整数的倍率和取整规则。对作者仓库中 Twitch 的全部 429,113 条边逐条复算后，零处不一致，实际成品采用：

$$
w(u,v)=\left\lfloor 100\left(1-\frac{|N(u)\cap N(v)|}{|N(u)\cup N(v)|}\right)\right\rfloor.
$$

因此 `Twitch.in` 中的权重范围是 30 到 100。这个“乘 100 后向下取整”的结论是对作者成品的完整核验结果，不是论文正文明确给出的公式；其他 7 个成品仍应以作者 OneDrive 文件为准。

## 5. 图和候选组的生成细节

### Musae

原始包 `wikipedia.zip` 包含 Chameleon、Crocodile、Squirrel 三个互不相交的 Wikipedia 文章图。作者数据把三图做不交并，并给节点编号增加偏移。

- 三个原始边文件共有 433,194 行。
- 统一为无向边并去重后有 400,832 条边。
- 再删除 335 个自环，得到论文的 400,497 条边。
- 三图共享特征编号空间；实际出现的特征恰为 `0..13182`，共 13,183 个。每个特征对应一个候选组。

### Twitch

原始包 `twitch.zip` 包含 DE、ENGB、ES、FR、PTBR、RU 六个语言子图。作者数据将六图做不交并并重新编号。

- 六图顶点数之和为 34,118，边数之和为 429,113，与论文完全一致。
- 六图实际出现 3,163 个不同节点特征；每个特征对应一个候选组。
- 原始特征 ID 范围为 `0..3169`，其中有 7 个 ID 未出现，因此生成 `.g` 时需要只保留实际出现的特征并重新编号。

### Github

原始包 `git_web_ml.zip` 有 37,700 个节点、289,003 条无向边，没有重复边或自环。节点特征 ID 恰为 `0..4004`，共 4,005 个，每个特征对应一个候选组。

### Youtube、Orkut、LiveJournal

图来自 SNAP 提供的无向最大连通分量；用户创建的社区/群组被用作候选组。SNAP 会把一个原始群组中的不同连通分量视为不同社区，并去除小于 3 个节点的社区。论文和作者仓库没有公开从 SNAP 社区文件到最终 `.g` 的完整转换脚本、过滤细节和 ID 映射，因此精确复现实验时应直接使用作者 OneDrive 中的 `.g`，不能仅凭 SNAP 当前页面重新生成后声称与论文完全一致。

### DBLP

论文引用 AMiner，并说明顶点表示专家、顶点关键词表示专家掌握的技能，技能对应候选组。论文/仓库没有给出 AMiner 快照日期、专家图边的构造规则、技能清洗、连通分量选择及 ID 映射脚本。因此 DBLP 的精确版本只能以作者成品为准。

### Reddit

论文引用 Pushshift Reddit API，但没有给出抓取时间范围、帖子/用户过滤、边和关键词的定义、去重规则及 ID 映射脚本。因此 Reddit 同样只能通过作者成品精确复现。

## 6. 查询生成细节和不可复现部分

论文说明：每个数据集、每种参数设置随机生成 300 个 GST 查询；默认组数为 5，并实验组数 3、5、7。论文还说查询会像引用 [61] 一样随机选择“相关”的关键词/组，但 GPU4GST 论文自身没有给出：

- “相关”的算法定义；
- 随机数种子；
- 候选组预过滤规则；
- 查询生成代码。

所以 `3.csv`、`5.csv`、`7.csv` 不是可以根据论文文字确定性重建的辅助文件，而是精确复现实验所必需的作者原始查询记录。

引用 [61] 的原文给出了“相关组”的一般生成方法：构造候选组共现图，若两个组至少共享一个原图顶点则相邻；均匀随机选择根组，BFS 到能够找到足够近邻的最小深度，再从已找到的近邻中均匀抽样，并对不可行查询重新生成。当前仓库用这一定义生成独立的 `g=5..16` 扩展查询，每格从 300 条候选固定选 5 条；但由于 GPU4GST 没有公开实际种子和生成代码，扩展查询不能声称是作者原始输出。冻结包中的转换协议见 [`docs/archive/GPU4GST_DATA.md`](../docs/archive/GPU4GST_DATA.md)。引用 [61] 为：Shuang Yang et al., *Approximating Probabilistic Group Steiner Trees in Graphs*, PVLDB 16(2), 2022，<https://www.vldb.org/pvldb/vol16/p343-sun.pdf>。

## 7. 二进制生成

作者仓库给出的命令是：

```bash
cd data
g++ tuple_text_to_bin.cpp -o tuple_text_to_bin
./tuple_text_to_bin Twitch 1 1
```

两个数字参数分别表示：

- `1`：输入图为无向图，为每条文本边写入两个 CSR 方向；
- `1`：跳过 `.in` 的第一行。

作者转换器使用 Linux LP64 下的 `long int`，所以仓库成品的顶点、偏移和权重元素均为 8 字节。不要在 Windows LLP64 下把 `long` 当成 8 字节后直接生成，否则二进制布局会不同。

## 8. 作者成品与论文 Table 1／实验文字的差异

下面的差异来自对 2026-07-17 作者 OneDrive 原文件的逐项核验，不是下载丢包造成的：所有文件长度均与 OneDrive 元数据一致。

| 项目 | 论文 | 作者成品 | 判断 |
|---|---:|---:|---|
| DBLP 顶点数 | 2,423,455 | `.in` 为 2,497,782 | 成品的 `.in`、`beg_pos`、CSR 和权重彼此自洽，论文表格或数据版本存在差异 |
| Orkut 顶点数 | 3,072,440 | `.in` 为 3,072,441 | 相差 1；成品 `beg_pos` 按 3,072,441 个顶点生成 |
| Youtube 候选组数 | 8,385 | `.g` 仅 5,000 行 | 作者没有公开从原始 8,385 组筛到 5,000 组的规则 |
| Orkut 候选组数 | 6,288,363 | `.g` 仅 5,000 行 | 同样存在未说明的候选组截取／采样 |
| LiveJournal 每种查询数 | 300 | `3/5/7.csv` 各 2,000 行 | 作者实验脚本显式运行索引 `0..299`，即只使用前 300 行；其余 1,700 行是额外查询 |

Orkut 还有一个会直接影响运行的成品缺陷：

- `Orkut.in` 的头部为 `3072441 117185083`，文件实测恰有 117,185,084 行（首行加全部边），所以文本图本身完整；无向图展开后应有 234,370,166 个 CSR 项。
- `Orkut_beg_pos.bin` 长度为 24,579,536 字节，末项恰为 234,370,166，说明它是完整且自洽的。
- 每个邻接点和权重都是 8 字节，因此 `Orkut_csr.bin` 与 `Orkut_weight.bin` 各应为 1,874,961,328 字节。
- OneDrive 原文件却分别只有 745,471,045 和 718,644,700 字节，甚至都不是 8 的倍数；它们分别在一个 64 位元素的第 5 字节和第 4 字节处结束。因此这是作者上传成品自身的截断，不能被求解器安全使用。

修复时应以完整的 `Orkut.in` 重新运行作者转换器，而不是给截断文件补零。其逻辑等价于：读取首行后，以无向模式把每条 `u v w` 同时写入 `u→v` 和 `v→u`，按顶点建立 CSR，并以 Linux LP64 的 8 字节 `long int` 写出三个二进制文件。作者命令为：

```bash
./tuple_text_to_bin Orkut 1 1
```

该程序还会生成非必需的 `_head.bin` 与 `_deg.bin`。重生成后必须检查：

```text
Orkut_beg_pos.bin   24,579,536 bytes
Orkut_csr.bin      1,874,961,328 bytes
Orkut_weight.bin   1,874,961,328 bytes
```

## 9. 完整性清单

- `OFFICIAL_ONEDRIVE_MANIFEST.json`：作者 OneDrive 返回的 64 个文件元数据，不含临时下载令牌。
- `SHA256SUMS.txt`：本次下载后对 64 个文件逐字节计算的 SHA-256。
- `TWITCH_SHA256.txt`：最初从作者 GitHub 仓库取得的 Twitch 校验值，仅用于比较。OneDrive 的 `Twitch.g` 使用 LF，仓库版使用 CRLF；两者归一化换行后内容完全相同。当前 `data_origin/Twitch.g` 保留 OneDrive 的 LF 版本，其 SHA-256 为 `2d91e143d27d39307c2b82787b77179029e8d3b315641a322e9b1a2f30d00844`。

可运行以下命令做完整性与结构核验：

```powershell
powershell -ExecutionPolicy Bypass -File data_origin/verify_data.ps1
```

2026-07-18 重新执行完整校验时，64 项 SHA-256 全部匹配。由于官方 Orkut 二进制截断，核验器随后仍会有意返回失败；这不是下载未完成，而是防止把损坏的作者成品误当作可运行数据。本仓库转换器只从完整的 `Orkut.in` 生成 `graph.txt`，不会读取这两个损坏文件。
