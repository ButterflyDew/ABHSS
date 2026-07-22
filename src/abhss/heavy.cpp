#include "core.h"

#include <chrono>
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

void EmitHeavyProbe(const char* phase,
                    const Problem& problem,
                    double seconds = -1.0,
                    const std::vector<Row>* rows = nullptr,
                    int layer = -1)
{
    if (!gst::ProbeDiagnosticsEnabled())
        return;
    std::ostringstream out;
    out << "method=abhss_heavy phase=" << phase
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

bool AnchoredAvailable(const std::vector<Row>& anchored, int mask)
{
    return !mask || anchored[mask].ready;
}

double AnchoredValue(const Problem& p,
                     const std::vector<Row>& anchored,
                     int mask,
                     int vertex)
{
    return mask ? RowValue(anchored[mask], vertex)
                : p.group_distance[p.anchor_group][vertex];
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
        ForEachOrdinaryBranch(p, ordinary_side, [&](int vertex, double value)
        {
            use(vertex, value + p.group_distance[p.anchor_group][vertex]);
        });
        return;
    }
    if (p.popcount[ordinary_side] == 1)
    {
        const auto& singleton =
            p.group_distance[p.bit_to_group[FirstBit(ordinary_side)]];
        ForEachValue(anchored[anchor_side], [&](int vertex, double value)
        {
            use(vertex, value + singleton[vertex]);
        });
        return;
    }

    ForEachRowBranchIntersection(
        anchored[anchor_side], p.ordinary[ordinary_side],
        [&](int vertex, double a, double d) { use(vertex, a + d); });
}

void CompleteLowRow(Problem& p,
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
                    if (side && p.popcount[side] > 1 &&
                        p.ordinary[side].vertex.size() < driver_size)
                    {
                        driver = side;
                        driver_size = p.ordinary[side].vertex.size();
                    }
                auto Visit = [&](int vertex)
                {
                    if (!std::binary_search(roots.begin(), roots.end(), vertex))
                        return;
                    const double a = left ? OrdinaryValue(p, left, vertex) : 0.0;
                    const double b = right ? OrdinaryValue(p, right, vertex) : 0.0;
                    if (a < fp::kInf && b < fp::kInf)
                        p.best = std::min(
                            p.best, anchored_distance[vertex] + a + b);
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

std::vector<Row> BuildLowAnchoredRows(Problem& p, int last_size)
{
    std::vector<Row> anchored(p.subset_count);
    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> bound_cache(p.graph.n + 1);
    std::vector<int> bound_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;
    for (int size = 1; size <= last_size; ++size)
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
                return value + Bound(vertex) < p.best;
            };
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
                ForEachAnchoredSum(p, anchored, anchor_side, ordinary_side, Set);
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
            CompleteLowRow(p, mask, settled, distance, minimum);
            settled.erase(
                std::remove_if(settled.begin(), settled.end(), [&](int vertex)
                {
                    return !(distance[vertex] + Bound(vertex) < p.best);
                }),
                settled.end());

            Row& row = anchored[mask];
            row.vertex = settled;
            row.value.reserve(settled.size());
            for (int vertex : settled)
                row.value.push_back(distance[vertex]);
            row.ready = true;
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
    }
    return anchored;
}

template <class Use>
void ForEachBackwardBranchSum(const Problem& p,
                              int block,
                              const Row& backward,
                              Use&& use)
{
    if (p.popcount[block] == 1)
    {
        const auto& singleton =
            p.group_distance[p.bit_to_group[FirstBit(block)]];
        ForEachValue(backward, [&](int vertex, double value)
        {
            use(vertex, value + singleton[vertex]);
        });
        return;
    }
    ForEachRowBranchIntersection(
        backward, p.ordinary[block],
        [&](int vertex, double h, double d) { use(vertex, h + d); });
}

struct TerminalEntry
{
    int mask = 0;
    double value = fp::kInf;
    double reduced = fp::kInf;
};

void BuildTransposedTerminals(Problem& p,
                              int low_last,
                              int high_last,
                              std::vector<std::vector<int>>& terminal_vertex,
                              std::vector<std::vector<double>>& terminal_value)
{
    terminal_vertex.assign(p.subset_count, {});
    terminal_value.assign(p.subset_count, {});
    std::array<std::vector<TerminalEntry>, 64> values_by_offset;
    std::vector<size_t> cursor(p.subset_count);
    std::vector<double> subset_potential(p.subset_count);
    std::vector<int> subset_stamp(p.subset_count);
    std::vector<double> value_at_mask(p.subset_count);
    std::vector<double> reduced_at_mask(p.subset_count);
    std::vector<int> value_stamp(p.subset_count);
    std::vector<double> terminal_best(p.subset_count, fp::kInf);
    std::vector<int> touched_targets;
    int potential_epoch = 0;
    int value_epoch = 0;
    const size_t word_count = (static_cast<size_t>(p.graph.n + 1) + 63) / 64;

    for (size_t word = 0; word < word_count; ++word)
    {
        for (auto& values : values_by_offset)
            values.clear();
        const int first_vertex = std::max(1, static_cast<int>(word << 6));
        const int end_vertex =
            std::min(p.graph.n + 1, static_cast<int>((word + 1) << 6));
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] > p.half || !OrdinaryAvailable(p, mask))
                continue;
            if (p.popcount[mask] == 1)
            {
                const auto& row =
                    p.group_distance[p.bit_to_group[FirstBit(mask)]];
                for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
                    values_by_offset[vertex & 63].push_back(
                        {mask, row[vertex], 0.0});
                continue;
            }
            const Row& row = p.ordinary[mask];
            size_t& index = cursor[mask];
            while (index < row.vertex.size() && row.vertex[index] < end_vertex)
            {
                values_by_offset[row.vertex[index] & 63].push_back(
                    {mask, row.value[index], 0.0});
                ++index;
            }
        }

        for (int vertex = first_vertex; vertex < end_vertex; ++vertex)
        {
            auto& values = values_by_offset[vertex & 63];
            if (values.empty())
                continue;
            ++potential_epoch;
            subset_stamp[0] = potential_epoch;
            subset_potential[0] = 0.0;
            auto Potential = [&](auto&& self, int mask) -> double
            {
                if (subset_stamp[mask] == potential_epoch)
                    return subset_potential[mask];
                const int bit = mask & -mask;
                subset_stamp[mask] = potential_epoch;
                subset_potential[mask] =
                    self(self, mask ^ bit) +
                    p.dual.GroupAt(vertex, p.bit_to_group[FirstBit(bit)]);
                return subset_potential[mask];
            };
            const double full_potential =
                p.dual.GroupAt(vertex, p.anchor_group) +
                Potential(Potential, p.full_mask);
            const double budget = p.best - full_potential;
            if (budget < 0.0)
                continue;
            for (auto& entry : values)
                entry.reduced = entry.value - Potential(Potential, entry.mask);
            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b)
            {
                return a.reduced != b.reduced ? a.reduced < b.reduced
                                              : a.mask < b.mask;
            });

            ++value_epoch;
            long long submask_work = 0;
            for (const auto& entry : values)
            {
                value_stamp[entry.mask] = value_epoch;
                value_at_mask[entry.mask] = entry.value;
                reduced_at_mask[entry.mask] = entry.reduced;
                submask_work +=
                    (static_cast<long long>(p.subset_count) >> p.popcount[entry.mask]) - 1;
            }
            long long pair_work = 0;
            size_t right_limit = values.size();
            for (size_t left = 0; left + 1 < values.size(); ++left)
            {
                while (right_limit > left + 1 &&
                       values[left].reduced + values[right_limit - 1].reduced > budget)
                    --right_limit;
                if (right_limit <= left + 1)
                    break;
                pair_work += static_cast<long long>(right_limit - left - 1);
            }

            touched_targets.clear();
            auto Update = [&](int target, double value)
            {
                const int size = p.popcount[target];
                if (size <= low_last || size > high_last)
                    return;
                if (terminal_best[target] >= fp::kInf)
                    touched_targets.push_back(target);
                terminal_best[target] = std::min(terminal_best[target], value);
            };
            for (const auto& entry : values)
            {
                if (entry.reduced > budget)
                    break;
                Update(p.full_mask ^ entry.mask, entry.value);
            }
            if (pair_work <= submask_work)
            {
                for (size_t left = 0; left < values.size(); ++left)
                {
                    if (left + 1 == values.size() ||
                        values[left].reduced + values[left + 1].reduced > budget)
                        break;
                    for (size_t right = left + 1; right < values.size(); ++right)
                    {
                        if (values[left].reduced + values[right].reduced > budget)
                            break;
                        if (!(values[left].mask & values[right].mask))
                            Update(p.full_mask ^
                                       (values[left].mask | values[right].mask),
                                   values[left].value + values[right].value);
                    }
                }
            }
            else
            {
                for (const auto& left : values)
                {
                    const int complement = p.full_mask ^ left.mask;
                    for (int right = complement; right;
                         right = (right - 1) & complement)
                    {
                        if (right <= left.mask || value_stamp[right] != value_epoch ||
                            left.reduced + reduced_at_mask[right] > budget)
                            continue;
                        Update(p.full_mask ^ (left.mask | right),
                               left.value + value_at_mask[right]);
                    }
                }
            }

            for (int target : touched_targets)
            {
                const int included = p.anchor_bit | p.original_mask[target];
                double farthest = p.group_distance[p.anchor_group][vertex];
                for (int bits = target; bits; bits &= bits - 1)
                    farthest = std::max(
                        farthest,
                        p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][vertex]);
                const double prefix = std::max(
                    farthest,
                    std::max(p.tour.At(vertex, included, p.group_distance),
                             p.dual.GroupAt(vertex, p.anchor_group) +
                                 Potential(Potential, target)));
                if (terminal_best[target] + prefix < p.best)
                {
                    terminal_vertex[target].push_back(vertex);
                    terminal_value[target].push_back(terminal_best[target]);
                }
                terminal_best[target] = fp::kInf;
            }
        }
    }
}

void SolveHighAdjoint(Problem& p,
                      const std::vector<Row>& anchored,
                      int low_last,
                      int high_last)
{
    std::vector<std::vector<int>> terminal_vertex;
    std::vector<std::vector<double>> terminal_value;
    BuildTransposedTerminals(
        p, low_last, high_last, terminal_vertex, terminal_value);

    std::vector<Row> backward(p.subset_count);
    std::vector<double> distance(p.graph.n + 1, fp::kInf);
    std::vector<double> prefix_cache(p.graph.n + 1);
    std::vector<int> prefix_stamp(p.graph.n + 1);
    int stamp = 0;
    std::vector<int> touched;
    std::vector<int> settled;

    auto EvaluateBoundary = [&](int successor)
    {
        for (int mask = 0; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] > low_last || (mask & ~successor) ||
                (mask && !anchored[mask].ready))
                continue;
            const int block = successor ^ mask;
            if (!block)
                continue;
            ForEachBackwardBranchSum(
                p, block, backward[successor], [&](int vertex, double value)
            {
                const double anchor = AnchoredValue(p, anchored, mask, vertex);
                if (anchor < fp::kInf)
                    p.best = std::min(p.best, anchor + value);
            });
        }
    };

    for (int size = high_last; size > low_last; --size)
    {
        for (int mask = 1; mask < p.subset_count; ++mask)
        {
            if (p.popcount[mask] != size)
                continue;
            touched.clear();
            settled.clear();
            ++stamp;
            const int included = p.anchor_bit | p.original_mask[mask];
            auto Prefix = [&](int vertex)
            {
                if (prefix_stamp[vertex] == stamp)
                    return prefix_cache[vertex];
                prefix_stamp[vertex] = stamp;
                double farthest = p.group_distance[p.anchor_group][vertex];
                for (int bits = mask; bits; bits &= bits - 1)
                    farthest = std::max(
                        farthest,
                        p.group_distance[
                            p.bit_to_group[FirstBit(bits & -bits)]][vertex]);
                prefix_cache[vertex] = std::max(
                    farthest,
                    std::max(p.tour.At(vertex, included, p.group_distance),
                             p.dual.At(vertex, included)));
                return prefix_cache[vertex];
            };
            auto Set = [&](int vertex, double value)
            {
                if (value >= distance[vertex] || !(value + Prefix(vertex) < p.best))
                    return;
                if (distance[vertex] >= fp::kInf)
                    touched.push_back(vertex);
                distance[vertex] = value;
            };
            for (size_t i = 0; i < terminal_vertex[mask].size(); ++i)
                Set(terminal_vertex[mask][i], terminal_value[mask][i]);
            const int outside = p.full_mask ^ mask;
            for (int block = outside; block; block = (block - 1) & outside)
            {
                const int successor = mask | block;
                if (p.popcount[successor] > high_last || !backward[successor].ready)
                    continue;
                ForEachBackwardBranchSum(
                    p, block, backward[successor], [&](int vertex, double value)
                {
                    Set(vertex, value);
                });
            }

            std::priority_queue<QueueNode,
                                std::vector<QueueNode>,
                                std::greater<QueueNode>> queue;
            for (int vertex : touched)
                queue.push({distance[vertex] + Prefix(vertex),
                            distance[vertex],
                            vertex});
            while (!queue.empty())
            {
                const QueueNode node = queue.top();
                queue.pop();
                if (node.distance != distance[node.vertex] || !(node.key < p.best))
                    continue;
                settled.push_back(node.vertex);
                for (const auto& edge : p.graph.adj[node.vertex])
                {
                    const double next = node.distance + edge.w;
                    if (next >= distance[edge.to] || !(next + Prefix(edge.to) < p.best))
                        continue;
                    if (distance[edge.to] >= fp::kInf)
                        touched.push_back(edge.to);
                    distance[edge.to] = next;
                    queue.push({next + Prefix(edge.to), next, edge.to});
                }
            }
            std::sort(settled.begin(), settled.end());
            settled.erase(std::unique(settled.begin(), settled.end()), settled.end());
            Row& row = backward[mask];
            row.vertex = settled;
            row.value.reserve(settled.size());
            for (int vertex : settled)
                row.value.push_back(distance[vertex]);
            row.ready = true;
            EvaluateBoundary(mask);
            for (int vertex : touched)
                distance[vertex] = fp::kInf;
        }
        EmitHeavyProbe("adjoint_layer", p, -1.0, &backward, size);
    }
}
}  // namespace

SolveResult SolveHeavyOneQuery(const Graph& graph, const Query& query)
{
    const int g = static_cast<int>(query.groups.size());
    if (!g)
        return {0.0, true};
    if (g > 16)
        throw std::runtime_error("ABHSSHeavy supports group count <= 16.");
    if (!IsQueryFeasible(graph, query))
        return {};
    if (g == 1)
        return {0.0, true};

    Problem problem(graph, query, false);
    const auto prepare_start = ProbeClock::now();
    EmitHeavyProbe("prepare_start", problem);
    if (PrepareProblem(problem))
    {
        EmitHeavyProbe(
            "prepare_closed", problem, ProbeSecondsSince(prepare_start));
        return {problem.best, true};
    }
    EmitHeavyProbe("prepare_end", problem, ProbeSecondsSince(prepare_start));

    const auto ordinary_start = ProbeClock::now();
    EmitHeavyProbe("ordinary_start", problem);
    BuildOrdinaryRows(problem, nullptr);
    EmitHeavyProbe("ordinary_end",
                   problem,
                   ProbeSecondsSince(ordinary_start),
                   &problem.ordinary);
    const int high_last = problem.half - 1;
    const int low_last = std::max(0, high_last / 2);
    if (problem.half < 2)
    {
        std::vector<double> anchor_distance(graph.n + 1, fp::kInf);
        std::vector<int> roots(graph.n);
        for (int vertex = 1; vertex <= graph.n; ++vertex)
        {
            roots[vertex - 1] = vertex;
            anchor_distance[vertex] =
                problem.group_distance[problem.anchor_group][vertex];
        }
        CompleteLowRow(problem, 0, roots, anchor_distance, 0.0);
    }
    const auto low_anchor_start = ProbeClock::now();
    EmitHeavyProbe("low_anchor_start", problem);
    std::vector<Row> anchored = BuildLowAnchoredRows(problem, low_last);
    EmitHeavyProbe("low_anchor_end",
                   problem,
                   ProbeSecondsSince(low_anchor_start),
                   &anchored);
    const auto adjoint_start = ProbeClock::now();
    EmitHeavyProbe("adjoint_start", problem);
    SolveHighAdjoint(problem, anchored, low_last, high_last);
    EmitHeavyProbe(
        "adjoint_end", problem, ProbeSecondsSince(adjoint_start));
    return {problem.best, problem.best < fp::kInf / 4};
}

}  // namespace gst::methods::abhss
