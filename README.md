# ABHSS：单线程精确 Group Steiner Tree

本仓库是面向 SIGMOD/VLDB 投稿的单计算线程、精确、无向非负边权 Group Steiner Tree 实现与实验 artifact。ABHSS 只有一个公开 solver 入口和一个正式二进制：Base 不开增强，然后依次开启 `DirectedCut` 与 `AdjointCompletion` 得到全增强配置。执行关系严格分为“共同操作”“增加安全证书”或“替换同一逻辑职责的 realization”。通过共同分量检查且未被零代价前置条件闭合后， $g\le3$ 的查询由所有配置先调用同一个 bounded root-star 初始化包，再用精确恒等式直接返回；它们不构造 witness、dual 或指数状态。这是一条逐项共同的数学基例，不是按组数选择配置。对仍需搜索的 $g>3$ 查询，所有配置在外层都只调用一次 `BuildDistanceRootInitialization`，取得相同的 `{group_distance, root, upper}` 合同；规范 SPT cutoff bootstrap 与完整距离扩展只是两种距离表示各自的内部成本，不是 Base 独占的论文阶段。例如，只要尚未闭包且平衡完成格包含 A1，三个合法配置都在 ordinary 前以相同 seed、farthest + endpoint-floor cone、非负 fallback 和 top-two 视图生成同一类标准 A1 row，并把同一对象移交前向阶段；A1 内核不读取增强位或 directed-cut 势。Base 与 Enhanced 在预处理中只构造各自的 root-path/dual-primal witness，随后共同运行同一个有序组三元组一步前瞻路径生长上界，都不无条件运行树 DP；预处理后两边从 `rent=0` 开始，让 A1 与 ordinary 连续支付同一种工作量，达到由各自树大小代入的共同 `buy` 阈值且树 DP 输入修订已变化时调用同一个树 DP。DirectedCut 只在 A1 之外加强 ordinary/adjoint 证书，Adjoint $H$ 仅替换 A1 之后的高层前向 $A$。实现不存在按组数经验阈值切换的 A1 路径。两条曲线是查询批次开始前预声明的同一算法配置，不做逐查询 oracle 选择。

当前冻结性能矩阵包含三类实验：

- P1：MonoGST+ 的 5 张作者图与 GPU4GST 的 8 张作者图，共 13 个独立图身份、29 个 query blocks 和全部 8,318 条作者查询；两个 DBLP 保持为不同图，论文主表每图每方法一行。
- P2：在 Musae、Twitch、YouTube、DBLP-GPU4GST、Orkut、Reddit 上做 `g=5..15` related-group 扩展；每个 `(graph,g)` 从 300 条候选中按输入平均组大小分为五个等秩层，以稳定哈希分两轮、每层各选 1 条。第二轮只追加 q6--q10，不改变原 q1--q5，共 10 条/格、660 条。
- S2：逐字节复用 P1 的 DBLP-MonoGSTPlus 与 Toronto-MonoGSTPlus 图，按 MonoGST+ 协议做受控 `<g,f>` 敏感性；`g={3,6,9,12,15}`、`f={200,400,800,1600,3200}`、每格 10 条，共 500 条。

三个正式计时项为 ABHSS Base、ABHSS 全增强和 PrunedDP++-Safe，但只需两个性能二进制 `abhss`/`pruneddp`。逐查询 timeout 为 3,600 秒。准确的数据来源、查询数、报告方式与风险见 [`docs/EXPERIMENT_PLAN.md`](docs/EXPERIMENT_PLAN.md)。

## 快速开始

Linux 服务器推荐直接使用 GNU Make：

```bash
make release JOBS=16
make validate
```

只构建和检查正式性能二进制：

```bash
make paper-binaries JOBS=16
make validate-paper-binaries
```

手工 smoke：

```bash
./build/abhss example results/manual query.txt data 1 1 --enhancements=none
./build/abhss example results/manual query.txt data 1 1 --enhancements=all
./build/pruneddp example results/manual query.txt data 1 1 --state-storage=hash --mst-upper=on --lb2-pathmax=off
```

Windows 可用 Visual Studio 或 MinGW 的 CMake generator，完整命令、数据准备、分片、断点续跑和汇总见 [`RUN.md`](RUN.md)。函数调用链、每个源文件的职责和安全改码流程见 [`docs/CODE_GUIDE.md`](docs/CODE_GUIDE.md)；详细方法、证明与复杂度见 [`docs/METHOD.md`](docs/METHOD.md)。

## 目录结构

| 目录 | 内容 | 是否属于当前真值 |
|---|---|---|
| `.github` | GitHub 托管配置；当前只含持续集成 workflow | 是，自动化入口 |
| `.github/workflows` | Ubuntu 24.04 上执行 `make release` 的 Linux CI | 是，平台编译/快速正确性门禁 |
| `data` | 统一 `graph.txt`/`query.txt` 运行接口；包含 tiny example，大图本体通常被 Git 忽略 | P1/S2 实际图接口，身份必须与 manifest 一致 |
| `data_origin` | GPU4GST 作者 CSV、OneDrive 元数据、SHA-256 与下载/验证脚本；大型 `.in`/`.g` 不进 Git | GPU4GST 作者输入的证据层 |
| `data_sources` | 不能直接随普通接口分发的官方快照和已知最优 benchmark 来源层 | 来源证据，不是 solver 直接入口 |
| `data_sources/official` | 2026-07-22 IMDb 官方快照的历史 URL、raw hash、下载和构建说明 | 已退出当前 P1/P2/S2 矩阵，仅保留历史来源证据 |
| `data_sources/steinlib` | WRP3/WRP4 原实例或本地恢复位置 | 只用于已知最优正确性 gate |
| `docs` | 仅保留当前实验方案、代码导读和详细方法三份活文档 | 是，人类可读真值 |
| `docs/archive` | 旧的配置重构、baseline、数据沿革和第三方恢复细节 | 否，只作历史证据，冲突时以活文档/机器矩阵为准 |
| `experiment_data` | 由来源接口派生的冻结查询 panel、身份清单和正确性实例 | 是，正式 workload 控制层 |
| `paper` | 面向 SIGMOD/VLDB 的章节初稿、版面取舍和伪代码设计 | 写作工作区；算法事实仍以 `docs/METHOD.md` 与源码为准 |
| `experiment_data/p1_published_workloads` | 13 图、29 query blocks、8,318 查询的路径、哈希、查询分布与已知无解索引 | P1 输入身份真值 |
| `experiment_data/p2_cross_g` | 66 个 `(graph,g)` cell、每格 10 条 panel 和选择证据；q1--q5 为原 panel，q6--q10 为追加 tranche | P2 输入身份真值 |
| `experiment_data/s1_controlled_gf` | 50 个 $\langle graph,g,f\rangle$ cell、查询、seed 和实现后组大小 | S2 输入身份真值；目录名保留历史 `s1`，矩阵 suite 名为 `S2_controlled_gf` |
| `experiment_data/steinlib` | SteinLib 转换后的图/查询与已知最优 index | 正确性真值 |
| `experiments` | `paper_matrix.json`、来源/环境锁、可行性审计、正确性证据和报告设置 | 是，正式实验机器可读控制面 |
| `src` | 全部 C++ 求解器、公共运行层和 baseline adapter | 代码真值 |
| `src/abhss` | ABHSS 单入口、公共 ordinary/前向内核、directed-cut 和 adjoint 增强 | 方法实现 |
| `src/common` | 图/查询 I/O、可行性、RSS 和结果输出 | 所有本地方法共用的工程层 |
| `src/pruneddp` | PrunedDP++-Safe 及 strict-pathmax 复现开关 | 主精确 baseline；必须标注为 corrected reconstruction |
| `src/dpbf` | 透明的稠密全子集 DPBF | correctness-only baseline |
| `src/baselines` | Basic+ 和 GPU4GST CPU PrunedDP++ 作者代码的可选 adapter | 只在恢复 `third_party` 后构建，不在冻结大图性能矩阵 |
| `tests` | 图/查询 I/O、零权 witness、低组闭包、逐弧 residual、144 随机精确对照和双方实际状态计数 | 每次构建的快速 correctness gate |
| `tools` | 数据转换、实验执行与第三方恢复构建脚本的总入口 | 工具代码真值 |
| `tools/data` | P1/P2/S2/SteinLib 构建、历史 IMDb 转换、哈希和可行性审计 | 输入生成与审计链 |
| `tools/experiments` | 环境校验、稳定分片运行、timeout/断点续跑、汇总和绘图 | 执行与报告链 |
| `tools/gpu4gst_data` | GPU4GST `.in`/`.g`/CSV 到本仓库接口的 C++ 转换器 | GPU4GST 转换链 |

`build` 是 CMake 生成目录，`results`/`build_external` 是运行或外部构建产物，`third_party` 只在恢复可选上游源码时出现；它们都不是应提交的仓库真值。`.agents` 和 `.git` 是本地工具/版本管理元数据，不参与编译或实验。

## 顶层文件

| 文件 | 作用 |
|---|---|
| `CMakeLists.txt` | C++17 target、警告、Release 优化/IPO 探测、可选第三方 adapter 与 CTest |
| `Makefile` | Linux 推荐的配置、编译、测试和 manifest 校验入口 |
| `run_p1_full.sh` | Linux 上完整 P1 的构建、深度校验、分片与断点续跑入口 |
| `RUN.md` | 从恢复数据到正式分片、续跑、诊断和汇总的命令手册 |
| `.gitignore` | 排除大图、展开查询、结果、编译产物和未授权第三方源码 |

## 正确性与声称边界

- 零权边合法并原样保留。图加载期一次计算普通连通分量和最小边权，使逐查询可行性检查只处理组成员；正权图上的 ABHSS 分量 cover 也不再扫描 Orkut 等大图。确实含零权边时，ABHSS 仍在 solver timer 内扫描原边并构造零权分量下界。
- 所有算法上下界闭合使用原始 `double` 顺序比较，不用 epsilon 将微小正 gap 误判为最优；跨实现结果文件仍用 $10^{-6}$ 作为报告核验容差。“精确”指实数算术模型下的组合精确性以及当前 `double` 实现不作算法近似，不代表任意精度或区间算术认证。
- MonoGST+ 自然查询中已审计出 55 条无可行树：LinkedMDB 46 条、DBpedia 9 条。P1 承诺全查询，因此它们被保留并必须一致返回 infeasible；P2/S2 新生成查询则要求全部可行。
- 当前 `abhss` 输出精确最优权值、feasibility 和实际发现的主状态项数，不序列化最优树边集。ABHSS 以“状态族、mask、vertex”为实际键，所以 D/A/H 中数值相同的 `(mask,v)` 是不同项；该指标不能冒充跨算法完全同成本的基本操作。在未增加决策回溯前，论文和 artifact 不得声称当前程序已输出树本身。
- `pruneddp_safe` 是经已知最优值验证的 corrected reconstruction，不是 2016 原作者代码的 bit-for-bit 镜像。GPU4GST 2025 artifact 中的 CPU 实现保留为适用子集校准，它受非负整数边权与 $g\le14$ 限制。

公开 artifact 前还必须逐项确认 MonoGST+ workload 与 GPU4GST 代码/数据包的获取和再分发方案。历史 IMDb 快照不进入当前论文矩阵，也不应随正式输入误发布。本地存在不等于可以随论文公开上传。
