#ifndef ABHSS_FORWARD_H
#define ABHSS_FORWARD_H

#include "core.h"

namespace gst::methods::abhss::internal
{
// Both variants use the same forward anchored recurrence.  Light asks for the
// complete prefix and may seed it with early A1 rows; Heavy retains only the
// low prefix needed by its high-layer adjoint.
struct ForwardAnchoredPlan
{
    int last_size = 0;
    bool retain_last_layer = true;
    const char* probe_method = nullptr;
    const char* probe_phase = "forward_anchor_layer";
};

// initial_rows is either empty or a subset_count-sized table of already exact
// anchored rows.  Ownership is transferred because consumed early rows never
// participate after the ordinary phase.
std::vector<Row> BuildForwardAnchoredRows(
    Problem& problem,
    const ForwardAnchoredPlan& plan,
    std::vector<Row> initial_rows = {});

// One shared phase boundary keeps Light, Heavy and Heavy-Forward diagnostics
// and timing semantics identical around the common forward kernel.
std::vector<Row> RunForwardAnchoredStage(
    Problem& problem,
    const ForwardAnchoredPlan& plan,
    const char* start_phase,
    const char* end_phase,
    std::vector<Row> initial_rows = {});

}  // namespace gst::methods::abhss::internal

#endif  // ABHSS_FORWARD_H
