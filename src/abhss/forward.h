#pragma once
#include "core.h"

namespace abhss {

// 接管提前生成的 A1，生成到 last_size；只有后续 H 需要时才保留最后一层。
std::vector<Row> BuildForwardAnchoredRows(Problem& problem, int last_size, bool retain_last_layer, std::vector<Row> initial_rows);

} // namespace abhss
