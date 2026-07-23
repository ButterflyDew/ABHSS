# [归档] 第三方 correctness gate 依赖

`third_party/` 和 `build_external/` 不属于仓库源文件。它们是可重新获取的外部 checkout 与构建产物，清理后的工作区中默认不存在，并由 `.gitignore` 整体排除。

正式大图性能实验只需要本仓库的单一 `abhss` 二进制（Base 与全增强两种固定配置）以及 PrunedDP++-Safe。以下依赖仅用于 SteinLib 已知最优值 correctness gate 或历史实现审计，不进入主性能表。

## 1. 冻结版本

| 组件 | 版本/commit | 恢复位置 | 当前用途 |
|---|---|---|---|
| GroupSteinerTree | `09f3bda70f3afa546fd88d446cdb185aee01b278` | `third_party/GroupSteinerTree` | Basic+ correctness gate |
| Boost | 1.85.0，归档 SHA-256 `7009fe1faa1697476bdc7027703a2badb84e849b7b0baad5086b087b971f8617` | `third_party/boost_1_85_0` | Basic+ 头文件依赖 |
| SCIP-Jack/SCIP | `74c11e60cd2d45e02f34b2afbf1fc3079c713e03`（tag `v703`） | `third_party/scip-jack-7.0.3` | SteinLib 独立精确验证 |
| SoPlex | `e24c304ef21f1f319de0413e14fed8e5414440b8`（release 5.0.2） | `third_party/soplex-5.0.2` | SCIP-Jack LP 后端 |
| GPU4GST code | `716a19c240c480cb2d23435bbaca55163a48e174` | `third_party/GPU4GST-sigmod` | 仅在需要重做历史 artifact audit 时恢复 |

机器锁定信息也保存在 [`experiments/environment_lock.json`](../../experiments/environment_lock.json)。

## 2. 恢复源码

以下命令只说明目标目录与 commit；执行前应自行确认上游许可和网络环境。

```powershell
New-Item -ItemType Directory -Force third_party

git clone https://github.com/YahuiSun/GroupSteinerTree.git third_party/GroupSteinerTree
git -C third_party/GroupSteinerTree checkout 09f3bda70f3afa546fd88d446cdb185aee01b278

git clone https://github.com/scipopt/scip.git third_party/scip-jack-7.0.3
git -C third_party/scip-jack-7.0.3 checkout 74c11e60cd2d45e02f34b2afbf1fc3079c713e03

git clone https://github.com/scipopt/soplex.git third_party/soplex-5.0.2
git -C third_party/soplex-5.0.2 checkout e24c304ef21f1f319de0413e14fed8e5414440b8
```

Boost 1.85.0 从 <https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2> 获取。验证上述 SHA-256 后解压为 `third_party/boost_1_85_0`。GroupSteinerTree 仓库中的 `ysgraph_20210203.zip` 需原样解压到该仓库目录。

如需重做 GPU4GST 历史实现审计：

```powershell
git clone https://github.com/toziki/GPU4GST-sigmod.git third_party/GPU4GST-sigmod
git -C third_party/GPU4GST-sigmod checkout 716a19c240c480cb2d23435bbaca55163a48e174
```

GPU4GST 数据本身的获取与哈希不依赖这个代码 checkout，见 [`GPU4GST_DATA.md`](GPU4GST_DATA.md)。

## 3. 构建

恢复 GroupSteinerTree 与 Boost 后，主 CMake 会自动启用 `basic_plus`：

```powershell
cmake -S . -B build
cmake --build build --config Release --parallel 4
```

恢复 SCIP-Jack 与 SoPlex 后，构建独立 correctness solver：

```powershell
python tools/build_external_baselines.py --target scip-jack
```

外部构建输出位于 `build_external/`，同样不应提交。随后执行：

```powershell
python tools/experiments/validate_environment.py --require-binaries
python tools/experiments/run_experiments.py --run-id gate --run-dir results/paper_runs/gate --suite S1_steinlib_exactness_gate
```

## 4. 许可边界

- SCIP-Jack 与 SoPlex 受其上游学术/开源许可约束；论文和 artifact 需按许可引用。
- Boost 使用 Boost Software License 1.0。
- 冻结的 GPU4GST 与 GroupSteinerTree checkout 未发现覆盖全部仓库内容的 repository-level license。公开 artifact 不应直接打包这些目录，应只发布 commit、adapter、哈希和获取说明，或先取得作者许可。
- `src/baselines/` 中只保留本仓库的兼容 adapter；上游核心算法文件不复制进 Git。
