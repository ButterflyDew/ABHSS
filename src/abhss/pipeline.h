#ifndef ABHSS_PIPELINE_H
#define ABHSS_PIPELINE_H

#include "core.h"

namespace gst::methods::abhss::internal
{
// Returns true when no exponential search is needed and writes answer.
bool ResolveQueryPrelude(const Graph& graph,
                         const Query& query,
                         const char* method,
                         SolveResult& answer);

// Common phase wrappers keep timing boundaries and probe fields identical for
// Light, Heavy, and the Heavy-Forward ablation.
bool PrepareWithProbe(Problem& problem, const char* method);
void BuildOrdinaryWithProbe(Problem& problem,
                            EarlyAnchor* early,
                            const char* method);

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_PIPELINE_H
