#include "core.h"

#include <chrono>
#include <numeric>
#include <sstream>
#include <stdexcept>

#include "../common/probe_diagnostics.h"
#include "../common/query_feasibility.h"

namespace gst::methods::abhss
{
namespace
{
using namespace internal;

using ProbeClock = std::chrono::steady_clock;

const char* LightProbeMethod()
{
#if defined(ABHSS_ABLATE_NO_EARLY)
    return "abhss_light_no_early";
#elif defined(ABHSS_ABLATE_NO_WITNESS)
    return "abhss_light_no_witness";
#else
    return "abhss_light";
#endif
}

std::uint64_t RowScalarCount(const std::vector<Row>& rows)
{
    std::uint64_t count = 0;
    for (const Row& row : rows)
        count += row.value.size();
    return count;
}

std::uint64_t ReadyRowCount(const std::vector<Row>& rows)
{
    std::uint64_t count = 0;
    for (const Row& row : rows)
        count += row.ready ? 1 : 0;
    return count;
}

double ProbeSecondsSince(const ProbeClock::time_point& start)
{
    return std::chrono::duration<double>(ProbeClock::now() - start).count();
}

void EmitAbhssProbe(const char* method,
                    const char* phase,
                    const Problem& problem,
                    double seconds = -1.0,
                    const std::vector<Row>* rows = nullptr,
                    int layer = -1)
{
    if (!gst::ProbeDiagnosticsEnabled())
        return;
    std::ostringstream out;
    out << "method=" << method << " phase=" << phase
        << " g=" << problem.g << " best=" << problem.best;
    if (seconds >= 0.0)
        out << " seconds=" << seconds;
    if (layer >= 0)
        out << " layer=" << layer;
    if (rows != nullptr)
        out << " rows=" << ReadyRowCount(*rows)
            << " scalars=" << RowScalarCount(*rows);
    gst::EmitProbeDiagnostic(out.str());
}

double AnchoredValue(const Problem& p,
                     const std::vector<Row>& anchored,
                     int mask,
                     int vertex)
{
    if (!mask)
        return p.group_distance[p.anchor_group][vertex];
    return RowValue(anchored[mask], vertex);
}

bool AnchoredAvailable(const std::vector<Row>& anchored, int mask)
{
    return !mask || anchored[mask].ready;
}

template <class Use>
void ForEachAnchoredSum(const Problem& p,
                        const std::vector<Row>& anchored,
                        int anchor_side,
                        int ordinary_side,
                        Use&& use)
{
    if (!anchor_side)
    {
        const auto& anchor = p.group_distance[p.anchor_group];
        ForEachOrdinaryBranch(p, ordinary_side, [&](int vertex, double value)
        {
            if (anchor.IsExact(vertex))
                use(vertex, value + anchor[vertex]);
        });
        return;
    }
    if (p.popcount[ordinary_side] == 1)
    {
        const auto& singleton =
            p.group_distance[p.bit_to_group[FirstBit(ordinary_side)]];
        ForEachValue(anchored[anchor_side], [&](int vertex, double value)
        {
            if (singleton.IsExact(vertex))
                use(vertex, value + singleton[vertex]);
        });
        return;
    }

    ForEachRowBranchIntersection(
        anchored[anchor_side], p.ordinary[ordinary_side],
        [&](int vertex, double a, double d) { use(vertex, a + d); });
}

void CompleteRoots(Problem& p,
                   int mask,
                   const std::vector<int>& roots,
                   const std::vector<double>& anchored_distance,
                   double anchored_minimum)
{
    const int remaining = p.full_mask ^ mask;
    for (int left = remaining;; left = (left - 1) & remaining)
    {
        const int right = remaining ^ left;
        if (left <= right && p.popcount[left] <= p.half &&
            p.popcount[right] <= p.half &&
            (!left || OrdinaryAvailable(p, left)) &&
            (!right || OrdinaryAvailable(p, right)))
        {
            const double left_minimum =
                !left || p.popcount[left] == 1 ? 0.0 : p.ordinary_minimum[left];
            const double right_minimum =
                !right || p.popcount[right] == 1 ? 0.0 : p.ordinary_minimum[right];
            if (anchored_minimum + left_minimum + right_minimum < p.best)
            {
                int driver = 0;
                size_t driver_size = roots.size();
                for (int side : {left, right})
                {
                    if (!side)
                        continue;
                    const size_t size = p.popcount[side] == 1
                                            ? p.group_distance[
                                                  p.bit_to_group[FirstBit(side)]].ExactSize(
                                                  p.graph.n)
                                            : p.ordinary[side].vertex.size();
                    if (size < driver_size)
                    {
                        driver = side;
                        driver_size = size;
                    }
                }

                auto Visit = [&](int vertex)
                {
                    if (!std::binary_search(roots.begin(), roots.end(), vertex))
                        return;
                    const double a = left ? OrdinaryValue(p, left, vertex) : 0.0;
                    const double b = right ? OrdinaryValue(p, right, vertex) : 0.0;
                    if (a >= fp::kInf || b >= fp::kInf)
                        return;
                    if (left && p.popcount[left] == 1 &&
                        !p.group_distance[p.bit_to_group[FirstBit(left)]].IsExact(vertex))
                        return;
                    if (right && p.popcount[right] == 1 &&
                        !p.group_distance[p.bit_to_group[FirstBit(right)]].IsExact(vertex))
                        return;
                    p.best = std::min(p.best, anchored_distance[vertex] + a + b);
                };
                if (driver)
                    ForEachOrdinaryValue(p, driver, [&](int vertex, double)
                    {
                        Visit(vertex);
                    });
                else
                    for (int vertex : roots)
                        Visit(vertex);
            }
        }
        if (!left)
            break;
    }
}

void BuildAnchoredRows(Problem& p, EarlyAnchor& early)
{
    std::vector<Row> anchored(p.subset_count);
    for (int mask = 1; mask < p.subset_count; ++mask)
        if (early.row[mask].ready)
            anchored[mask] = std::move(early.row[mask]);

    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> bound_cache(p.graph.n + 1);
    std::vector<int> bound_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;

    // A(0) 只在 g=2/3 时直接参与最终结算；更大 g 不物化这一整行。
    if (p.half < 2)
    {
        std::vector<int> roots;
        p.group_distance[p.anchor_group].ForEachExact(
            p.graph.n, [&](int vertex, double value)
        {
            roots.push_back(vertex);
            distance[vertex] = value;
        });
        CompleteRoots(p, 0, roots, distance, 0.0);
        for (int vertex : roots)
            distance[vertex] = fp::kInf;
    }

    for (int size = 1; size <= p.half - 1; ++size)
    {
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size)
                continue;
            touched.clear();
            settled.clear();
            double minimum = fp::kInf;
            const int remaining_nonanchor = p.full_mask ^ mask;
            const int remaining_original =
                p.nonanchor_original_mask ^ p.original_mask[mask];
            ++stamp;
            auto Bound = [&](int vertex)
            {
                if (bound_stamp[vertex] != stamp)
                {
                    bound_stamp[vertex] = stamp;
                    bound_cache[vertex] =
                        FutureBound(p, vertex, remaining_original);
                }
                return bound_cache[vertex];
            };
            auto CanImprove = [&](int vertex, double value)
            {
                if (!(value + FarthestRemaining(p, vertex, remaining_original) < p.best))
                    return false;
                return value + Bound(vertex) < p.best;
            };

            // 提前生成的 A1 已是 cone 内的精确闭包；此处只按
            // 更紧 incumbent 重新筛选，并执行正式 completion。
            if (anchored[mask].ready)
            {
                Row precomputed = std::move(anchored[mask]);
                ForEachValue(precomputed, [&](int vertex, double value)
                {
                    if (!CanImprove(vertex, value))
                        return;
                    distance[vertex] = value;
                    touched.push_back(vertex);
                    settled.push_back(vertex);
                    minimum = std::min(minimum, value);
                    double star = value;
                    for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                        star += p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][vertex];
                    p.best = std::min(p.best, star);
                });
                CompleteRoots(p, mask, settled, distance, minimum);
            }
            else
            {
                auto Set = [&](int vertex, double value)
                {
                    if (value >= distance[vertex] || !CanImprove(vertex, value))
                        return;
                    if (distance[vertex] >= fp::kInf)
                        touched.push_back(vertex);
                    distance[vertex] = value;
                };
                for (int ordinary_side = mask;
                     ordinary_side;
                     ordinary_side = (ordinary_side - 1) & mask)
                {
                    const int anchor_side = mask ^ ordinary_side;
                    if (!OrdinaryAvailable(p, ordinary_side) ||
                        !AnchoredAvailable(anchored, anchor_side))
                        continue;
                    ForEachAnchoredSum(
                        p, anchored, anchor_side, ordinary_side, Set);
                }
                if (touched.empty())
                    continue;

                std::priority_queue<QueueNode,
                                    std::vector<QueueNode>,
                                    std::greater<QueueNode>> queue;
                for (int vertex : touched)
                    queue.push({distance[vertex] + Bound(vertex),
                                distance[vertex],
                                vertex});
                while (!queue.empty())
                {
                    const QueueNode node = queue.top();
                    queue.pop();
                    if (node.distance != distance[node.vertex] || !(node.key < p.best))
                        continue;
                    settled.push_back(node.vertex);
                    minimum = std::min(minimum, node.distance);
                    double star = node.distance;
                    for (int bits = remaining_nonanchor; bits; bits &= bits - 1)
                        star += p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][node.vertex];
                    p.best = std::min(p.best, star);
                    for (const auto& edge : p.graph.adj[node.vertex])
                    {
                        const double next = node.distance + edge.w;
                        if (next >= distance[edge.to] || !CanImprove(edge.to, next))
                            continue;
                        if (distance[edge.to] >= fp::kInf)
                            touched.push_back(edge.to);
                        distance[edge.to] = next;
                        queue.push({next + Bound(edge.to),
                                    next,
                                    edge.to});
                    }
                }
                std::sort(settled.begin(), settled.end());
                settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
                CompleteRoots(p, mask, settled, distance, minimum);
            }

            settled.erase(
                std::remove_if(settled.begin(), settled.end(), [&](int vertex)
                {
                    return !(distance[vertex] + Bound(vertex) < p.best);
                }),
                settled.end());
            if (size < p.half - 1)
            {
                Row& row = anchored[mask];
                row.vertex = settled;
                row.value.reserve(settled.size());
                for (int vertex : settled)
                    row.value.push_back(distance[vertex]);
                row.ready = true;
            }
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
        EmitAbhssProbe(
            LightProbeMethod(), "anchored_layer", p, -1.0, &anchored, size);
    }
}
}  // namespace

SolveResult SolveLightOneQuery(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
        return {0.0, true};
    if (g > 16)
        throw std::runtime_error("ABHSSLight supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
        return {};
    if (g == 1)
        return {0.0, true};

    Problem problem(graph, query, true);
    const auto prepare_start = ProbeClock::now();
    EmitAbhssProbe(LightProbeMethod(), "prepare_start", problem);
    if (PrepareProblem(problem))
    {
        EmitAbhssProbe(LightProbeMethod(),
                       "prepare_closed",
                       problem,
                       ProbeSecondsSince(prepare_start));
        return {problem.best, true};
    }
    EmitAbhssProbe(LightProbeMethod(),
                   "prepare_end",
                   problem,
                   ProbeSecondsSince(prepare_start));
#if defined(ABHSS_ABLATE_NO_WITNESS)
    // Preserve all initial feasible upper bounds, but disable the recurring
    // subset DP over the materialized witness tree.
    problem.witness_tree = {};
#endif
    EarlyAnchor early;
#if defined(ABHSS_ABLATE_NO_EARLY)
    // BuildAnchoredRows expects a row table even when no row was produced by
    // the early anchor pass.  Ordinary rows receive no early-future oracle.
    early.row.assign(problem.subset_count, {});
    const auto ordinary_start = ProbeClock::now();
    EmitAbhssProbe(LightProbeMethod(), "ordinary_start", problem);
    BuildOrdinaryRows(problem, nullptr);
    EmitAbhssProbe(LightProbeMethod(),
                   "ordinary_end",
                   problem,
                   ProbeSecondsSince(ordinary_start),
                   &problem.ordinary);
#else
    const auto early_start = ProbeClock::now();
    EmitAbhssProbe(LightProbeMethod(), "early_anchor_start", problem);
    BuildEarlyAnchorRows(problem, early);
    EmitAbhssProbe(LightProbeMethod(),
                   "early_anchor_end",
                   problem,
                   ProbeSecondsSince(early_start),
                   &early.row);
    const auto ordinary_start = ProbeClock::now();
    EmitAbhssProbe(LightProbeMethod(), "ordinary_start", problem);
    BuildOrdinaryRows(problem, &early);
    EmitAbhssProbe(LightProbeMethod(),
                   "ordinary_end",
                   problem,
                   ProbeSecondsSince(ordinary_start),
                   &problem.ordinary);
#endif
    const auto anchored_start = ProbeClock::now();
    EmitAbhssProbe(LightProbeMethod(), "anchored_start", problem);
    BuildAnchoredRows(problem, early);
    EmitAbhssProbe(LightProbeMethod(),
                   "anchored_end",
                   problem,
                   ProbeSecondsSince(anchored_start));
    return {problem.best, problem.best < fp::kInf / 4};
}

SolveResult SolveHeavyForwardOneQuery(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
        return {0.0, true};
    if (g > 16)
        throw std::runtime_error("ABHSSHeavyForward supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
        return {};
    if (g == 1)
        return {0.0, true};

    // This is an exact, one-factor ablation of Heavy: PrepareProblem builds
    // the same eager directed-cut potentials and dual-primal witness, and the
    // ordinary rows are identical.  Only the high-level transpose/adjoint is
    // replaced by the full forward anchored lattice used by Light.
    Problem problem(graph, query, false);
    const auto prepare_start = ProbeClock::now();
    EmitAbhssProbe("abhss_heavy_forward", "prepare_start", problem);
    if (PrepareProblem(problem))
    {
        EmitAbhssProbe("abhss_heavy_forward",
                       "prepare_closed",
                       problem,
                       ProbeSecondsSince(prepare_start));
        return {problem.best, true};
    }
    EmitAbhssProbe("abhss_heavy_forward",
                   "prepare_end",
                   problem,
                   ProbeSecondsSince(prepare_start));
    const auto ordinary_start = ProbeClock::now();
    EmitAbhssProbe("abhss_heavy_forward", "ordinary_start", problem);
    BuildOrdinaryRows(problem, nullptr);
    EmitAbhssProbe("abhss_heavy_forward",
                   "ordinary_end",
                   problem,
                   ProbeSecondsSince(ordinary_start),
                   &problem.ordinary);
    EarlyAnchor empty;
    empty.row.assign(problem.subset_count, {});
    const auto anchored_start = ProbeClock::now();
    EmitAbhssProbe("abhss_heavy_forward", "anchored_start", problem);
    BuildAnchoredRows(problem, empty);
    EmitAbhssProbe("abhss_heavy_forward",
                   "anchored_end",
                   problem,
                   ProbeSecondsSince(anchored_start));
    return {problem.best, problem.best < fp::kInf / 4};
}

}  // namespace gst::methods::abhss
