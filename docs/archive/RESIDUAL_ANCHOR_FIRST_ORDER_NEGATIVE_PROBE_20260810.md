# Residual 永久锚优先顺序负面探针（2026-08-10）

## 候选

当前单证书按初始 far-to-near 顺序的严格逆序完成 residual 势。候选利用永久锚存在于每个 ordinary continuation 的结构，把规范锚移到 completion 首位，其余组仍保持严格逆序。它不读取图名、组数区间、时间、状态规模或边权类型，不增加证书数量、理论空间或每状态势读取。

任意固定顺序逐次只扣除尚未使用的 residual 容量，因此候选仍是可采纳证书，并通过仓库 5/5 CTest。

## Orkut g=15, q5 早停门

候选与主线具有相同的 A1 结果和购买价：A1 为 25,188,706 个标量，incumbent 为 35，residual closure buy 为 6,206,733,889 primitive work。主线严格逆序完成 closure、primal、facility 与 refilter 共约 163.7 秒。

候选改变组序后，residual Dijkstra 的实际传播显著增加。进程运行到 588 秒时仍未完成 closure；扣除约 30 秒图加载以及 closure 前的共同预处理和 A1 后，closure 已连续运行超过主线的两倍时间。此时进程 RSS 约 9.69 GiB，尚未得到可以与主线 10,508,369 个 refilter 后状态比较的输出。按预登记的构造成本门主动停止，不能把未完成运行写成查询 timeout 或性能结果。

## 决策

永久锚优先在理论调用种类不变的情况下仍显著放大了 residual 最短路的实际堆工作，已经违反性能不退化门。候选淘汰，源码恢复严格逆序；隔离构建与半截输出删除，只保留本结论。后续不得仅凭“锚出现在所有 continuation”再次移动 residual 组序，除非先给出不会增加证书构造工作的独立机制。
