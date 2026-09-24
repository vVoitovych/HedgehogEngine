#include "HedgehogRenderer/Views/ViewOrdering.hpp"

#include <algorithm>
#include <set>
#include <string>

namespace Renderer
{
    namespace
    {
        // Ready views come out lowest Priority first, then lowest ViewId: the only tie-break.
        struct ReadyOrder
        {
            const std::vector<View>* Views = nullptr;
            bool operator()(size_t lhs, size_t rhs) const
            {
                const View& a = (*Views)[lhs];
                const View& b = (*Views)[rhs];
                if (a.Desc.Priority != b.Desc.Priority)
                    return a.Desc.Priority < b.Desc.Priority;
                return a.Id < b.Id;
            }
        };

        bool Contains(const std::vector<std::string>& names, const std::string& name)
        {
            return std::find(names.begin(), names.end(), name) != names.end();
        }

        // Edges[w] lists the views that read a target view w writes: w must run before each.
        std::vector<std::vector<size_t>> BuildEdges(const std::vector<View>& views)
        {
            std::vector<std::vector<size_t>> edges(views.size());
            for (size_t writer = 0; writer < views.size(); ++writer)
            {
                for (size_t reader = 0; reader < views.size(); ++reader)
                {
                    for (const std::string& target : views[writer].Desc.Targets)
                    {
                        if (Contains(views[reader].Desc.Reads, target))
                        {
                            edges[writer].push_back(reader);
                            break;
                        }
                    }
                }
            }
            return edges;
        }

        // Kahn's algorithm. Returns the order; views left out of it are in, or behind, a cycle.
        std::vector<size_t> TopologicalOrder(const std::vector<View>& views,
                                             const std::vector<std::vector<size_t>>& edges)
        {
            std::vector<size_t> incoming(views.size(), 0);
            for (const auto& readers : edges)
                for (const size_t reader : readers)
                    ++incoming[reader];

            std::set<size_t, ReadyOrder> ready(ReadyOrder{ &views });
            for (size_t i = 0; i < views.size(); ++i)
                if (incoming[i] == 0)
                    ready.insert(i);

            std::vector<size_t> order;
            order.reserve(views.size());
            while (!ready.empty())
            {
                const size_t next = *ready.begin();
                ready.erase(ready.begin());
                order.push_back(next);
                for (const size_t reader : edges[next])
                    if (--incoming[reader] == 0)
                        ready.insert(reader);
            }
            return order;
        }

        // Some cycle among the views not in `order`, as view indices. Walking predecessors from any
        // unordered view must revisit one: every unordered view has an unordered predecessor.
        std::vector<size_t> FindCycle(const std::vector<View>& views, const std::vector<std::vector<size_t>>& edges,
                                      const std::vector<size_t>& order)
        {
            std::vector<bool> ordered(views.size(), false);
            for (const size_t i : order)
                ordered[i] = true;

            std::vector<size_t> predecessor(views.size(), views.size());
            for (size_t writer = 0; writer < views.size(); ++writer)
                if (!ordered[writer])
                    for (const size_t reader : edges[writer])
                        if (!ordered[reader])
                            predecessor[reader] = writer;

            size_t start = 0;
            while (ordered[start])
                ++start;

            std::vector<size_t> path;
            std::vector<size_t> seenAt(views.size(), views.size());
            for (size_t current = start; seenAt[current] == views.size(); current = predecessor[current])
            {
                seenAt[current] = path.size();
                path.push_back(current);
            }
            const size_t loopStart = seenAt[predecessor[path.back()]];
            std::vector<size_t> cycle(path.begin() + static_cast<std::ptrdiff_t>(loopStart), path.end());
            std::reverse(cycle.begin(), cycle.end()); // writer before reader
            return cycle;
        }

        std::string Describe(const View& view)
        {
            std::string text = "view " + std::to_string(view.Id);
            if (!view.Desc.GraphName.empty())
                text += " ('" + view.Desc.GraphName + "')";
            return text;
        }

        DroppedView DropFromCycle(const std::vector<View>& views, const std::vector<size_t>& cycle)
        {
            // Lowest priority leaves, then the newest (highest ViewId).
            const size_t victim = *std::min_element(cycle.begin(), cycle.end(), [&](size_t lhs, size_t rhs)
            {
                if (views[lhs].Desc.Priority != views[rhs].Desc.Priority)
                    return views[lhs].Desc.Priority < views[rhs].Desc.Priority;
                return views[lhs].Id > views[rhs].Id;
            });

            std::string message;
            if (cycle.size() == 1)
            {
                message = Describe(views[victim]) + " reads its own target";
            }
            else
            {
                message = "render-target cycle between ";
                for (size_t i = 0; i < cycle.size(); ++i)
                    message += (i == 0 ? "" : (i + 1 == cycle.size() ? " and " : ", ")) + Describe(views[cycle[i]]);
            }
            message += "; dropping " + Describe(views[victim]) + " for this frame";
            return { views[victim].Id, ViewDropReason::Cycle, message };
        }
    }

    std::vector<View> OrderViews(std::vector<View> views, std::vector<DroppedView>& dropped)
    {
        while (true)
        {
            const std::vector<std::vector<size_t>> edges = BuildEdges(views);
            const std::vector<size_t> order = TopologicalOrder(views, edges);

            if (order.size() == views.size())
            {
                std::vector<View> ordered;
                ordered.reserve(views.size());
                for (const size_t i : order)
                    ordered.push_back(std::move(views[i]));
                return ordered;
            }

            const DroppedView drop = DropFromCycle(views, FindCycle(views, edges, order));
            dropped.push_back(drop);
            std::erase_if(views, [&](const View& view) { return view.Id == drop.Id; });
        }
    }
}
