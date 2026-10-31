#pragma once

#include "Profiler/api/Profiler.hpp"

#include "HedgehogRenderer/Graph/GpuPassTiming.hpp"

#include <cstdint>
#include <span>
#include <vector>

// A recorded frame as the Profiler window's Hierarchy views show it, worked out without ImGui so
// EditorTest covers it: zones merged by call path, the same inverted, and the GPU passes with
// their commands.
namespace Editor
{
    inline constexpr uint32_t NO_PROFILER_NODE = UINT32_MAX;

    struct ProfilerNode
    {
        const char* Name        = nullptr; // points into what the tree was built from
        uint32_t    Parent      = NO_PROFILER_NODE;
        uint32_t    FirstChild  = NO_PROFILER_NODE;
        uint32_t    NextSibling = NO_PROFILER_NODE;
        double      TotalMs     = 0.0;
        double      SelfMs      = 0.0; // TotalMs minus the children's
        uint32_t    Calls       = 0;
        int32_t     Command     = -1;  // in a GPU tree, the command's index; -1 elsewhere
    };

    // Nodes[0] is the root, nameless, holding the top-level nodes as its children. Nodes are kept
    // in the order they were first met unless SortProfilerTree reorders them. Building into a tree
    // reuses its storage.
    struct ProfilerTree
    {
        std::vector<ProfilerNode> Nodes;
        double                    FrameMs = 0.0; // what Total and Self percentages are of

        [[nodiscard]] const ProfilerNode& Root() const { return Nodes.front(); }
    };

    // The thread's zones merged by call path: one node per distinct path of names (compared by
    // text), its calls counted and its times summed.
    void BuildHierarchy(const Profiling::ThreadRecord& thread, double frameMs, ProfilerTree& out);

    // One top-level node per zone name with its zones' summed self time (Total equals Self), whose
    // children are the names it was called from, theirs those they were called from, and so on, each
    // with the self time and calls of the zones reached through it.
    void BuildInvertedHierarchy(const Profiling::ThreadRecord& thread, double frameMs, ProfilerTree& out);

    // Each GPU pass (in order, Name its own) with its commands as children (Name the command's
    // kind, Command its index) under the pass of the same name; a command whose pass is not listed
    // gets a node of its own. A pass's self time is what its commands do not cover, at least 0.
    // A scope (a ScopeBegin to its ScopeEnd) is a node named by the scope holding the commands
    // recorded inside it; its time is theirs plus the gap before its end, its self time that gap.
    void BuildGpuTree(std::span<const Renderer::GpuPassTiming> passes, std::span<const Renderer::GpuCommandTiming> commands,
                      double gpuFrameMs, ProfilerTree& out);

    enum class ProfilerSortKey : uint8_t
    {
        Total,
        Self,
        Calls,
        Name,
    };

    // Orders every node's children by key: names A to Z and numbers largest first, the other way
    // round when reversed, ties in the order they were met; siblings only, so the shape is kept.
    void SortProfilerTree(ProfilerTree& tree, ProfilerSortKey key, bool reversed = false);

    // The node's children in order, for walking a tree.
    template<typename Fn>
    void ForEachChild(const ProfilerTree& tree, uint32_t node, Fn&& fn)
    {
        for (uint32_t child = tree.Nodes[node].FirstChild; child != NO_PROFILER_NODE; child = tree.Nodes[child].NextSibling)
            fn(child);
    }
}
