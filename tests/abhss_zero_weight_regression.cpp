#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "../src/abhss/abhss.h"

namespace
{
void AddEdge(gst::Graph& graph, int u, int v, double weight)
{
    const int id = static_cast<int>(graph.edges.size());
    graph.edges.push_back({id, u, v, weight});
    graph.adj[u].push_back({v, id, weight});
    graph.adj[v].push_back({u, id, weight});
    graph.minimum_edge_weight = std::min(graph.minimum_edge_weight, weight);
    graph.m = static_cast<int>(graph.edges.size());
}

void CheckAnswer(const char* method,
                 const gst::methods::abhss::SolveResult& answer,
                 double expected)
{
    if (!answer.feasible || std::fabs(answer.best_weight - expected) > 1e-12)
        throw std::runtime_error(std::string(method) +
                                 " failed the zero-weight witness regression.");
}
}  // namespace

int main()
{
    // The useful witness is the tree 4--(1)--3--(0)--2.  Re-rooting it at
    // anchor 4 used to let vertex 2 re-parent the already-settled vertex 3 at
    // the same distance.  That formed 2<->3 and produced one extra final-key
    // heap pop.  The 4--1 leaf only weakens the component lower bound so that
    // both solvers must actually materialize and evaluate the witness.
    gst::Graph graph;
    graph.n = 4;
    graph.minimum_edge_weight = std::numeric_limits<double>::infinity();
    graph.adj.assign(graph.n + 1, {});
    AddEdge(graph, 4, 3, 1.0);
    AddEdge(graph, 3, 2, 0.0);
    AddEdge(graph, 4, 1, 0.125);

    gst::Query query;
    query.groups = {{2}, {4}};

    CheckAnswer("ABHSS-Light",
                gst::methods::abhss::SolveLightOneQuery(graph, query),
                1.0);
    CheckAnswer("ABHSS-Heavy",
                gst::methods::abhss::SolveHeavyOneQuery(graph, query),
                1.0);
    std::cout << "zero-weight witness regression passed\n";
    return 0;
}
