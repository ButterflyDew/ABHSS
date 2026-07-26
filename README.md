# ABHSS review branch

本分支只保留论文方法的人类审阅版本：一个单线程精确 Group Steiner Tree 求解器，以及同一求解器的 Base 与 Enhanced 两种最终模式。baseline、实验调度器、中间消融态、探针、格式核验工具和历史文档均不在本分支中。

## 目录

| 路径 | 内容 |
|---|---|
| `src/main.cpp` | 最小批处理入口，只负责加载、模式选择、计时和逐查询输出 |
| `src/abhss/` | ABHSS 算法；按预处理、ordinary DP、前向 A、反向 H 分为多个模块 |
| `src/common/` | 图与查询快速读取、连通分量可行性判断、浮点路径恢复辅助 |
| `docs/CODE_GUIDE.md` | 从入口到热循环的逐文件、逐函数代码导读 |
| `docs/METHOD.md` | 论文方法章节的详细中文底稿，包含递推、证明、复杂度和实现目的 |
| `data/example/` | 唯一随仓库提交的最小运行示例；正式大图由 `.gitignore` 排除 |

## 编译

Linux、macOS 或安装了 GNU Make 的环境：

```bash
make release JOBS=16
```

等价的 CMake 命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 16
```

需要 CMake 3.16 以上及支持 C++17 的 GCC、Clang 或 MSVC。Release 构建开启编译器优化和跨过程优化；程序本身不创建计算线程。

## 运行

```bash
./build/abhss <graph_folder> <query_file> <base|enhanced> [first_query] [query_count]
```

最小示例：

```bash
./build/abhss data/example data/example/query.txt base
./build/abhss data/example data/example/query.txt enhanced
```

`first_query` 使用从 1 开始的编号；省略区间时运行整个查询文件。每行输出：

```text
query_index seconds best_weight mask_vertex_states
```

`seconds` 只覆盖单条查询的预处理与搜索，不包含图和查询文件加载。不可行查询的 `best_weight` 为 `-1`。

## 输入合同

图目录中必须存在小写 `graph.txt`：

```text
n m
u_1 v_1 w_1
...
u_m v_m w_m
```

顶点编号为 `1..n`，图无向，边权为 `double` 可表示的有限非负数；允许零权边、重边和自环。查询文件格式为：首个整数是查询数；每条查询先给组数 `g`，之后每组依次给 `size vertex_1 ... vertex_size`。组非空、顶点合法，组之间允许重叠，且 `g <= 16`。review 分支按竞赛代码约定信任正式输入满足这些条件，不保留针对损坏文件的完整合法性框架。

## 两种模式

Base 与 Enhanced 共用 `SolveOneQuery`、预处理主线、A1、ordinary D、witness rent-or-buy 调度器、树 DP、稀疏 `Row` 和前向 A 内核。最终的 `bool enhanced` 只控制四类论文可说明的差异：组距离采用有界或完整表示；root-path witness 替换为 dual-primal witness；完整前向高层 A 替换为低层 A 加高层 H；Enhanced 安全新增有向割下界和 primal/facility 上界。具体对应关系及精确性证明见 [`docs/METHOD.md`](docs/METHOD.md)。
