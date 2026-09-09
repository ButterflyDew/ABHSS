# ABHSS

单线程精确 Group Steiner Tree 求解器。Base 和 Enhanced 是同一算法的两种模式：共享预处理、普通子集 DP、提前 A1 与条件式见证树 DP；Enhanced 增加对偶证书，并用反向 H 完成高层状态。

本分支提供随论文阅读和运行的算法代码，不包含 baseline、实验调度器和历史结果。

## 编译与运行

需要 Linux、CMake 3.16 以上，以及支持 C++17 的 GCC 或 Clang。

```bash
make release JOBS=4
./build/abhss data/example data/example/query.txt base
./build/abhss data/example data/example/query.txt enhanced
```

也可以单独指定构建目录：

```bash
cmake -S . -B build-paper -DCMAKE_BUILD_TYPE=Release
cmake --build build-paper --parallel 4
```

程序本身不创建计算线程；`JOBS` 只控制编译并行数。运行接口为：

```text
abhss <graph_folder> <query_file> <base|enhanced> [first_query] [query_count]
```

查询编号从 1 开始；省略区间时运行整个查询文件。每完成一条查询立即输出一行：

```text
query_index seconds best_weight mask_vertex_states
```

`seconds` 是本条查询的预处理与搜索时间，不含输入加载；不可行时 `best_weight=-1`。最后一列是 D/A/H 各逻辑行首次发现的状态数，既不是堆操作次数，也不是峰值内存。程序输出最优权值，不输出最优树的边集合。

## 输入

图目录包含小写 `graph.txt`：

```text
n m
u_1 v_1 w_1
...
u_m v_m w_m
```

每条边只列一次，程序按无向边加载。顶点编号为 `1..n`；边权为有限非负数，按 `double` 读取。允许零权边、重边、自环和非连通图。实现以 `1e100` 为无穷哨兵，输入应保证涉及的有限路径、候选费用和中间和远小于它。

查询文件开头是查询数。随后每条查询先写组数 `g`，每组写 `size` 和对应顶点编号：

```text
2
2
2 1 3
1 4
3
1 1
1 2
2 3 4
```

上例有两条查询，分别包含两个组和三个组。组之间可以重叠；输入应满足组非空、顶点编号合法、`0 <= g <= 16`。读取器信任这些条件，不检查损坏的输入。大图和正式查询集不随本分支提交；已有相同格式的数据可以直接使用，无需其他运行时依赖。

## 阅读路径

| 路径 | 内容 |
|---|---|
| `src/main.cpp` | 命令行、逐查询计时与输出 |
| `src/graph.h`、`src/io.cpp` | 图和查询结构、缓冲读取、连通分量 |
| `src/abhss/` | 预处理、普通 D、前向 A、反向 H 和证书模块 |
| `src/common/` | 路径恢复使用的浮点辅助比较 |
| `docs/CODE_GUIDE.md` | 入口、函数调用和关键实现的代码导读 |
| `docs/METHOD.md` | 状态定义、递推、正确性与复杂度 |
| `data/example/` | 可直接运行的小示例 |

建议先读[代码导读](docs/CODE_GUIDE.md)，再沿[方法说明](docs/METHOD.md)中的公式定位实现。源代码为每个函数、lambda 及较长函数的主要步骤保留中文注释。
