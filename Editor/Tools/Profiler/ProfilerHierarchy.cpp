#include "ProfilerHierarchy.hpp"
#include "ProfilerAnalysis.hpp"

#include <algorithm>
#include <cstring>

namespace Editor
{
    namespace
    {
        constexpr double NS_PER_MS = 1.0e6;

        void Reset(ProfilerTree& tree, double frameMs)
        {
            tree.Nodes.clear();
            tree.Nodes.emplace_back();
            tree.FrameMs = frameMs;
        }

        bool SameName(const char* a, const char* b)
        {
            return a == b || (a != nullptr && b != nullptr && std::strcmp(a, b) == 0);
        }

        // The child of parent named name, made last among its children when it has none.
        uint32_t FindOrAddChild(ProfilerTree& tree, uint32_t parent, const char* name)
        {
            uint32_t last = NO_PROFILER_NODE;
            for (uint32_t child = tree.Nodes[parent].FirstChild; child != NO_PROFILER_NODE; child = tree.Nodes[child].NextSibling)
            {
                if (SameName(tree.Nodes[child].Name, name))
                    return child;
                last = child;
            }
            const uint32_t index = static_cast<uint32_t>(tree.Nodes.size());
            ProfilerNode   node;
            node.Name   = name;
            node.Parent = parent;
            tree.Nodes.push_back(node);
            if (last == NO_PROFILER_NODE)
                tree.Nodes[parent].FirstChild = index;
            else
                tree.Nodes[last].NextSibling = index;
            return index;
        }

        void AddTime(ProfilerNode& node, double totalMs, double selfMs)
        {
            node.TotalMs += totalMs;
            node.SelfMs += selfMs;
            ++node.Calls;
        }

        // The root's total: its children's.
        void TotalRoot(ProfilerTree& tree)
        {
            double total = 0.0;
            ForEachChild(tree, 0, [&](uint32_t child) { total += tree.Nodes[child].TotalMs; });
            tree.Nodes[0].TotalMs = total;
        }
    }

    void BuildHierarchy(const Profiling::ThreadRecord& thread, double frameMs, ProfilerTree& out)
    {
        Reset(out, frameMs);
        // Zones in start order with a stack of the open ones and their nodes: each zone's node is
        // its name under its parent's node, and its duration leaves the parent's self time.
        std::vector<std::pair<const Profiling::ZoneRecord*, uint32_t>> open;
        for (const Profiling::ZoneRecord* zone : SortZonesByStart(thread))
        {
            while (!open.empty() && open.back().first->Depth >= zone->Depth)
                open.pop_back();
            const uint32_t parent = open.empty() ? 0 : open.back().second;
            const uint32_t node   = FindOrAddChild(out, parent, zone->Name);
            const double   ms     = static_cast<double>(zone->DurationNs) / NS_PER_MS;
            AddTime(out.Nodes[node], ms, ms);
            if (parent != 0)
                out.Nodes[parent].SelfMs -= ms;
            open.emplace_back(zone, node);
        }
        TotalRoot(out);
    }

    void BuildInvertedHierarchy(const Profiling::ThreadRecord& thread, double frameMs, ProfilerTree& out)
    {
        Reset(out, frameMs);
        const std::vector<const Profiling::ZoneRecord*> zones = SortZonesByStart(thread);

        // Each zone's self time first, from the zones directly nested in it.
        std::vector<double>   selfMs(zones.size());
        std::vector<uint32_t> open; // indices into zones
        std::vector<uint32_t> parents(zones.size(), NO_PROFILER_NODE);
        for (uint32_t i = 0; i < zones.size(); ++i)
        {
            while (!open.empty() && zones[open.back()]->Depth >= zones[i]->Depth)
                open.pop_back();
            selfMs[i] = static_cast<double>(zones[i]->DurationNs) / NS_PER_MS;
            if (!open.empty())
            {
                parents[i] = open.back();
                selfMs[open.back()] -= selfMs[i];
            }
            open.push_back(i);
        }

        // Then each zone's self time down its name and the names of its callers, innermost first.
        for (uint32_t i = 0; i < zones.size(); ++i)
        {
            uint32_t node = FindOrAddChild(out, 0, zones[i]->Name);
            AddTime(out.Nodes[node], selfMs[i], selfMs[i]);
            for (uint32_t caller = parents[i]; caller != NO_PROFILER_NODE; caller = parents[caller])
            {
                node = FindOrAddChild(out, node, zones[caller]->Name);
                AddTime(out.Nodes[node], selfMs[i], selfMs[i]);
            }
        }
        TotalRoot(out);
    }

    void BuildGpuTree(std::span<const Renderer::GpuPassTiming> passes, std::span<const Renderer::GpuCommandTiming> commands,
                      double gpuFrameMs, ProfilerTree& out)
    {
        Reset(out, gpuFrameMs);
        for (const Renderer::GpuPassTiming& pass : passes)
        {
            ProfilerNode& node = out.Nodes[FindOrAddChild(out, 0, pass.Name.c_str())];
            AddTime(node, pass.GpuMilliseconds, pass.GpuMilliseconds);
        }
        for (size_t i = 0; i < commands.size(); ++i)
        {
            const Renderer::GpuCommandTiming& command = commands[i];
            const uint32_t pass = FindOrAddChild(out, 0, command.Pass);
            if (out.Nodes[pass].Calls == 0)
                ++out.Nodes[pass].Calls; // a pass known only from its commands
            out.Nodes[pass].SelfMs -= command.GpuMilliseconds;
            // Each command is a node of its own, so it is added rather than found by name.
            const uint32_t index = static_cast<uint32_t>(out.Nodes.size());
            ProfilerNode   node;
            node.Name    = Renderer::GetGpuCommandKindName(command.Kind);
            node.Parent  = pass;
            node.TotalMs = command.GpuMilliseconds;
            node.SelfMs  = command.GpuMilliseconds;
            node.Calls   = 1;
            node.Command = static_cast<int32_t>(i);
            out.Nodes.push_back(node);
            uint32_t* link = &out.Nodes[pass].FirstChild;
            while (*link != NO_PROFILER_NODE)
                link = &out.Nodes[*link].NextSibling;
            *link = index;
        }
        // A pass known only from its commands takes their time as its own.
        ForEachChild(out, 0, [&](uint32_t pass)
        {
            ProfilerNode& node = out.Nodes[pass];
            if (node.TotalMs == 0.0 && node.SelfMs < 0.0)
            {
                node.TotalMs = -node.SelfMs;
                node.SelfMs  = 0.0;
            }
        });
        TotalRoot(out);
    }

    void SortProfilerTree(ProfilerTree& tree, ProfilerSortKey key, bool reversed)
    {
        // order < 0 when x comes first: names A to Z, numbers largest first.
        const auto before = [&](uint32_t a, uint32_t b)
        {
            const ProfilerNode& x = tree.Nodes[a];
            const ProfilerNode& y = tree.Nodes[b];
            const auto larger = [](auto u, auto v) { return u > v ? -1 : u < v ? 1 : 0; };
            int order = 0;
            switch (key)
            {
            case ProfilerSortKey::Total: order = larger(x.TotalMs, y.TotalMs); break;
            case ProfilerSortKey::Self:  order = larger(x.SelfMs, y.SelfMs); break;
            case ProfilerSortKey::Calls: order = larger(x.Calls, y.Calls); break;
            case ProfilerSortKey::Name:  order = std::strcmp(x.Name ? x.Name : "", y.Name ? y.Name : ""); break;
            }
            if (reversed)
                order = -order;
            return order != 0 ? order < 0 : a < b; // ties keep the order they were met in
        };

        std::vector<uint32_t> children;
        for (uint32_t parent = 0; parent < tree.Nodes.size(); ++parent)
        {
            children.clear();
            ForEachChild(tree, parent, [&](uint32_t child) { children.push_back(child); });
            if (children.size() < 2)
                continue;
            std::sort(children.begin(), children.end(), before);
            tree.Nodes[parent].FirstChild = children.front();
            for (size_t i = 0; i + 1 < children.size(); ++i)
                tree.Nodes[children[i]].NextSibling = children[i + 1];
            tree.Nodes[children.back()].NextSibling = NO_PROFILER_NODE;
        }
    }
}
