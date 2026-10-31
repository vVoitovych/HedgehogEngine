#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Graph/TimedCommandList.hpp"

#include "TestRHIDoubles.hpp"

#include "Profiler/api/Profiler.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <cstring>

using namespace Renderer;
using namespace RGTest;

namespace
{
    struct OpaquePassData
    {
        RGTexture ColorTarget{};
    };

    RGTextureDesc MakeRGDesc()
    {
        RGTextureDesc desc;
        desc.Name   = "SceneColor";
        desc.Format = RHI::Format::R8G8B8A8Unorm;
        desc.Size   = RGSizePolicy::MakeAbsolute(16, 16);
        desc.Usage  = RHI::TextureUsage::ColorAttachment;
        return desc;
    }

    // Two side-effect passes: one draws twice, the other once with instances.
    void DeclarePasses(RenderGraphRuntime& runtime)
    {
        const RGTexture first = runtime.CreateTexture(MakeRGDesc());
        runtime.AddPass<OpaquePassData>(
            "Opaque",
            [first](RGPassBuilder& pass, OpaquePassData& data)
            {
                data.ColorTarget = pass.ColorTarget(first);
                pass.SetSideEffect();
            },
            [](OpaquePassData&, RHI::IRHICommandList& cmd)
            {
                cmd.DrawIndexed(36);
                cmd.DrawIndexed(12);
            });
        const RGTexture second = runtime.CreateTexture(MakeRGDesc());
        runtime.AddPass<OpaquePassData>(
            "Lines",
            [second](RGPassBuilder& pass, OpaquePassData& data)
            {
                data.ColorTarget = pass.ColorTarget(second);
                pass.SetSideEffect();
            },
            [](OpaquePassData&, RHI::IRHICommandList& cmd) { cmd.Draw(6, 4); });
    }

    size_t CountTimestamps(const RecordingCommandList& cmd)
    {
        return static_cast<size_t>(std::count_if(cmd.Commands.begin(), cmd.Commands.end(),
            [](const std::string& command) { return command.rfind("timestamp", 0) == 0; }));
    }
}

TEST_CASE("Timed command list - every call reaches the target, each timed command followed by its timestamp")
{
    TestQueryPool        pool(16);
    RecordingCommandList target;
    TimedCommandList     timed;
    timed.Start(target, pool, 2, 10);
    timed.SetPass("Forward");

    const TestBuffer   buffer(64);
    TestBuffer         destination(64);
    TestTexture        texture(RHI::TextureDesc{});
    timed.BindIndexBuffer(buffer, RHI::IndexType::Uint32, 0);
    timed.DrawIndexed(36, 2, 0, 0, 0);
    timed.Draw(3, 1, 0, 0);
    timed.CopyBufferToBuffer(buffer, destination, 0, 0, 48);
    timed.GenerateMipmaps(texture);
    // A backend finds its native command buffer through the wrapper.
    CHECK(&timed.GetRecordingList() == &target);
    timed.Finish();

    CHECK(target.Commands == std::vector<std::string>{ "reset queries 2 10", "timestamp 2", "index", "draw 36",
                                                       "timestamp 3", "timestamp 4", "timestamp 5", "mipmaps",
                                                       "timestamp 6" });
    CHECK(target.DrawnVertexCounts == std::vector<uint32_t>{ 3 });
    REQUIRE(target.MipmapGenerations.size() == 1);
    CHECK(target.MipmapGenerations[0] == &texture);

    const std::span<const GpuCommandTiming> commands = timed.GetCommands();
    REQUIRE(commands.size() == 4);
    CHECK(commands[0].Kind == GpuCommandKind::DrawIndexed);
    CHECK(commands[0].Count == 36);
    CHECK(commands[0].Instances == 2);
    CHECK(commands[1].Kind == GpuCommandKind::Draw);
    CHECK(commands[1].Count == 3);
    CHECK(commands[2].Kind == GpuCommandKind::CopyBuffer);
    CHECK(commands[2].Count == 48);
    CHECK(commands[3].Kind == GpuCommandKind::Mipmaps);
    for (const GpuCommandTiming& command : commands)
        CHECK(std::strcmp(command.Pass, "Forward") == 0);
    CHECK_FALSE(timed.IsTruncated());
}

TEST_CASE("Timed command list - past its capacity commands are still recorded, only not timed")
{
    TestQueryPool        pool(4);
    RecordingCommandList target;
    TimedCommandList     timed;
    timed.Start(target, pool, 0, 3);
    for (uint32_t i = 1; i <= 4; ++i)
        timed.DrawIndexed(i, 1, 0, 0, 0);

    CHECK(target.DrawnIndexCounts == std::vector<uint32_t>{ 1, 2, 3, 4 });
    CHECK(CountTimestamps(target) == 3);
    CHECK(timed.GetCommands().size() == 2);
    CHECK(timed.IsTruncated());

    // A new capture starts empty.
    RecordingCommandList next;
    timed.Start(next, pool, 0, 3);
    CHECK(timed.GetCommands().empty());
    CHECK_FALSE(timed.IsTruncated());
}

TEST_CASE("Timed command list - scopes reach the target and are timed at their start and end")
{
    TestQueryPool        pool(16);
    RecordingCommandList target;
    TimedCommandList     timed;
    timed.Start(target, pool, 0, 16);
    timed.SetPass("Shadow");

    timed.BeginScope("Rigid");
    timed.BeginScope("Cascade 0");
    timed.DrawIndexed(36, 1, 0, 0, 0);
    timed.EndScope();
    timed.EndScope();
    timed.EndScope(); // no scope open: ignored
    timed.Finish();

    CHECK(target.Scopes == std::vector<std::string>{ "begin Rigid", "begin Cascade 0", "end", "end" });
    CHECK(CountTimestamps(target) == 6); // the start, two scope starts, the draw, two scope ends

    const std::span<const GpuCommandTiming> commands = timed.GetCommands();
    REQUIRE(commands.size() == 5);
    CHECK(commands[0].Kind == GpuCommandKind::ScopeBegin);
    CHECK(std::strcmp(commands[0].Scope, "Rigid") == 0);
    CHECK(std::strcmp(commands[1].Scope, "Cascade 0") == 0);
    CHECK(commands[2].Kind == GpuCommandKind::DrawIndexed);
    CHECK(commands[3].Kind == GpuCommandKind::ScopeEnd);
    CHECK(commands[4].Kind == GpuCommandKind::ScopeEnd);
    CHECK(IsGpuScopeMarker(commands[0].Kind));
    CHECK_FALSE(IsGpuScopeMarker(commands[2].Kind));
    for (const GpuCommandTiming& command : commands)
        CHECK(std::strcmp(command.Pass, "Shadow") == 0);
}

TEST_CASE("With command timestamps set, Execute times each pass's barriers and draws under the pass's name")
{
    TestDevice         device;
    RenderGraphRuntime runtime(device, 64 * 1024);
    TestQueryPool      pool(64);
    runtime.SetCommandTimestamps(&pool, 8, 32);

    DeclarePasses(runtime);
    RecordingCommandList cmd;
    REQUIRE(runtime.Execute(cmd));

    // The draws as recorded, then one timestamp per timed command plus the first.
    CHECK(cmd.DrawnIndexCounts == std::vector<uint32_t>{ 36, 12 });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 6 });
    REQUIRE_FALSE(cmd.Commands.empty());
    CHECK(cmd.Commands[0] == "reset queries 8 32");
    CHECK(cmd.Commands[1] == "timestamp 8");

    const std::span<const GpuCommandTiming> commands = runtime.GetLastTimedCommands();
    CHECK(CountTimestamps(cmd) == commands.size() + 1);
    std::vector<std::string> draws;
    for (const GpuCommandTiming& command : commands)
    {
        if (command.Kind == GpuCommandKind::Barrier)
        {
            CHECK(command.Count == static_cast<uint32_t>(1));
            continue;
        }
        draws.push_back(std::string(command.Pass) + " " + GetGpuCommandKindName(command.Kind) + " "
                        + std::to_string(command.Count) + "x" + std::to_string(command.Instances));
    }
    CHECK(draws == std::vector<std::string>{ "Opaque DrawIndexed 36x1", "Opaque DrawIndexed 12x1", "Lines Draw 6x4" });
    // The pass names are interned, so they outlive the frame's graph.
    CHECK(commands.front().Pass == Profiling::InternName("Opaque"));
    CHECK_FALSE(runtime.IsLastCommandTimingTruncated());

    // Off again: the passes record straight into the command list, and nothing is timed.
    runtime.SetCommandTimestamps(nullptr, 0, 0);
    DeclarePasses(runtime);
    RecordingCommandList untimed;
    REQUIRE(runtime.Execute(untimed));
    CHECK(untimed.Commands == std::vector<std::string>{ "draw 36", "draw 12" });
    CHECK(runtime.GetLastTimedCommands().empty());
}
