#pragma once
#include "forward.h"

namespace abhss {

// 从辅助半层 high_last 向 low_last 递减构造 H，并与已生成的低层 A 结算。
void SolveHighAdjoint(Problem& problem, const std::vector<Row>& anchored, int low_last, int high_last);

} // namespace abhss
