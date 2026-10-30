#include "Tools/Profiler/ProfilerHierarchy.hpp"

#include "doctest/doctest/doctest.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace Editor;

namespace
{
    constexpr uint64_t MS = 1000000; // nanoseconds

    // The node's children as "name total/self/calls", in order.
    std::vector<std::string> Describe(const ProfilerTree& tree, uint32_t node)
    {
        std::vector<std::string> out;
        ForEachChild(tree, node, [&](uint32_t child)
        {
            const ProfilerNode& n = tree.Nodes[child];
            char text[128];
            std::snprintf(text, sizeof(text), "%s %.1f/%.1f/%u", n.Name, n.TotalMs, n.SelfMs, n.Calls);
            out.push_back(text);
        });
        return out;
    }

    uint32_t Child(const ProfilerTree& tree, uint32_t node, const std::string& name)
    {
        uint32_t found = NO_PROFILER_NODE;
        ForEachChild(tree, node, [&](uint32_t child)
        {
            if (name == tree.Nodes[child].Name)
                found = child;
        });
        REQUIRE(found != NO_PROFILER_NODE);
        return found;
    }

    // Frame 10 ms: Update 4 ms calling Physics twice (1 ms each, the first holding a 0.5 ms Step),
    // then Render 5 ms calling Physics once (2 ms).
    Profiling::ThreadRecord MakeThread()
    {
        // Two names spelt by two pointers each are one name.
        static const std::string physics = "Physics";
        Profiling::ThreadRecord  thread;
        thread.Zones = {
            { "Physics", 0, 1 * MS, 2 },
            { "Step", 0, MS / 2, 3 },
            { physics.c_str(), 2 * MS, 1 * MS, 2 },
            { "Update", 0, 4 * MS, 1 },
            { "Physics", 5 * MS, 2 * MS, 2 },
            { "Render", 5 * MS, 5 * MS, 1 },
            { "Frame", 0, 10 * MS, 0 },
        };
        return thread;
    }
}

TEST_CASE("Profiler hierarchy - zones merged by call path, with self times and calls")
{
    ProfilerTree tree;
    BuildHierarchy(MakeThread(), 10.0, tree);
    CHECK(tree.FrameMs == 10.0);
    CHECK(tree.Root().TotalMs == doctest::Approx(10.0));
    CHECK(Describe(tree, 0) == std::vector<std::string>{ "Frame 10.0/1.0/1" });

    const uint32_t frame = Child(tree, 0, "Frame");
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Update 4.0/2.0/1", "Render 5.0/3.0/1" });
    // Update's two Physics calls are one node; Render's is another.
    CHECK(Describe(tree, Child(tree, frame, "Update")) == std::vector<std::string>{ "Physics 2.0/1.5/2" });
    CHECK(Describe(tree, Child(tree, frame, "Render")) == std::vector<std::string>{ "Physics 2.0/2.0/1" });
    CHECK(Describe(tree, Child(tree, Child(tree, frame, "Update"), "Physics")) == std::vector<std::string>{ "Step 0.5/0.5/1" });

    // Built again into the same tree, the same tree.
    const size_t nodes = tree.Nodes.size();
    BuildHierarchy(MakeThread(), 10.0, tree);
    CHECK(tree.Nodes.size() == nodes);

    BuildHierarchy(Profiling::ThreadRecord{}, 0.0, tree);
    CHECK(tree.Nodes.size() == 1);
    CHECK(tree.Root().FirstChild == NO_PROFILER_NODE);
}

TEST_CASE("Profiler hierarchy - inverted: self time by name, expanding into its callers")
{
    ProfilerTree tree;
    BuildInvertedHierarchy(MakeThread(), 10.0, tree);
    // Physics' self: 0.5 + 1 + 2 ms over three calls; the roots in the order met.
    CHECK(Describe(tree, 0) == std::vector<std::string>{ "Frame 1.0/1.0/1", "Update 2.0/2.0/1", "Physics 3.5/3.5/3",
                                                         "Step 0.5/0.5/1", "Render 3.0/3.0/1" });
    const uint32_t physics = Child(tree, 0, "Physics");
    CHECK(Describe(tree, physics) == std::vector<std::string>{ "Update 1.5/1.5/2", "Render 2.0/2.0/1" });
    CHECK(Describe(tree, Child(tree, physics, "Update")) == std::vector<std::string>{ "Frame 1.5/1.5/2" });
    CHECK(Describe(tree, Child(tree, 0, "Step")) == std::vector<std::string>{ "Physics 0.5/0.5/1" });
    // The roots' self times are the frame.
    CHECK(tree.Root().TotalMs == doctest::Approx(10.0));
}

TEST_CASE("Profiler hierarchy - GPU passes with their commands, a pass known only from its commands included")
{
    using Renderer::GpuCommandKind;
    const std::vector<Renderer::GpuPassTiming>    passes   = { { "Shadow", 1.0 }, { "Forward", 2.0 } };
    const std::vector<Renderer::GpuCommandTiming> commands = {
        { "Shadow", GpuCommandKind::Barrier, 1, 1, 0.1 },
        { "Shadow", GpuCommandKind::DrawIndexed, 36, 1, 0.6 },
        { "Forward", GpuCommandKind::DrawIndexed, 12, 1, 1.5 },
        { "Upload", GpuCommandKind::CopyBuffer, 64, 1, 0.3 },
    };

    ProfilerTree tree;
    BuildGpuTree(passes, commands, 4.0, tree);
    CHECK(tree.FrameMs == 4.0);
    CHECK(Describe(tree, 0) == std::vector<std::string>{ "Shadow 1.0/0.3/1", "Forward 2.0/0.5/1", "Upload 0.3/0.0/1" });
    const uint32_t shadow = Child(tree, 0, "Shadow");
    CHECK(Describe(tree, shadow) == std::vector<std::string>{ "Barrier 0.1/0.1/1", "DrawIndexed 0.6/0.6/1" });
    CHECK(tree.Nodes[tree.Nodes[shadow].FirstChild].Command == 0);
    CHECK(tree.Nodes[Child(tree, 0, "Upload")].TotalMs == doctest::Approx(0.3));
    CHECK(tree.Nodes[shadow].Command == -1);
    CHECK(tree.Root().TotalMs == doctest::Approx(3.3));

    BuildGpuTree(passes, {}, 4.0, tree);
    CHECK(Describe(tree, 0) == std::vector<std::string>{ "Shadow 1.0/1.0/1", "Forward 2.0/2.0/1" });
}

TEST_CASE("Profiler hierarchy - sorting orders every node's children only")
{
    ProfilerTree tree;
    BuildHierarchy(MakeThread(), 10.0, tree);
    const uint32_t frame = Child(tree, 0, "Frame");

    SortProfilerTree(tree, ProfilerSortKey::Total);
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Render 5.0/3.0/1", "Update 4.0/2.0/1" });
    SortProfilerTree(tree, ProfilerSortKey::Total, true);
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Update 4.0/2.0/1", "Render 5.0/3.0/1" });
    SortProfilerTree(tree, ProfilerSortKey::Self);
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Render 5.0/3.0/1", "Update 4.0/2.0/1" });
    SortProfilerTree(tree, ProfilerSortKey::Name);
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Render 5.0/3.0/1", "Update 4.0/2.0/1" });
    SortProfilerTree(tree, ProfilerSortKey::Name, true);
    CHECK(Describe(tree, frame) == std::vector<std::string>{ "Update 4.0/2.0/1", "Render 5.0/3.0/1" });

    BuildInvertedHierarchy(MakeThread(), 10.0, tree);
    SortProfilerTree(tree, ProfilerSortKey::Calls);
    CHECK(tree.Nodes[tree.Root().FirstChild].Name == std::string("Physics"));
    // A sort keeps the shape: Physics' callers are still its children.
    CHECK(Describe(tree, Child(tree, 0, "Physics")).size() == 2);
}
