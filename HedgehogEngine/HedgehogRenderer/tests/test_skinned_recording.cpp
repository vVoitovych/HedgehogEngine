#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    using CommandList = std::vector<std::string>;

    // The buffers and sets a frame points into, kept alive for the frame.
    struct SkinnedScene
    {
        TestBuffer        Positions{ 1024 };
        TestBuffer        TexCoords{ 1024 };
        TestBuffer        Normals{ 1024 };
        TestBuffer        Indices{ 1024 };
        TestBuffer        Joints{ 1024 };
        TestBuffer        Weights{ 1024 };
        FakeDescriptorSet MaterialA;
        FakeDescriptorSet MaterialB;
        FakeDescriptorSet SceneLights;
        FakeDescriptorSet Palette;

        const RHI::IRHIDescriptorSet* Materials[2] = { &MaterialA, &MaterialB };
        const MeshDrawRange           Meshes[2]    = { { 0, 36, 0 }, { 36, 120, 24 } };

        // Everything a frame with skinned meshes binds, but no instances yet.
        GraphFrameData MakeFrame()
        {
            GraphFrameData frame = RGTest::MakeFrame(1);
            frame.SceneLights    = &SceneLights;
            frame.Meshes         = Meshes;
            frame.MaterialSets   = Materials;
            frame.Positions      = &Positions;
            frame.TexCoords      = &TexCoords;
            frame.Normals        = &Normals;
            frame.Indices        = &Indices;
            frame.Joints         = &Joints;
            frame.Weights        = &Weights;
            frame.JointPalette   = &Palette;
            return frame;
        }
    };

    HX::RenderInstance Rigid(uint64_t mesh, uint64_t material)
    {
        HX::RenderInstance instance = Instance(mesh);
        instance.MaterialIndex      = material;
        return instance;
    }

    HX::RenderInstance Skinned(uint64_t mesh, uint64_t material, uint32_t paletteOffset)
    {
        HX::RenderInstance instance = Rigid(mesh, material);
        instance.PaletteOffset      = paletteOffset;
        instance.JointCount         = 2;
        return instance;
    }

    // Runs a depth prepass, a shadow pass and a forward pass over frame, as a view's graph does.
    void Record(GraphFrameData& frame, FakeServices& services, RecordingCommandList& cmd, bool cullBackFaces = true)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        GraphFrameContext context{ &services, &frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(&context);

        const auto depth = [&](const char* name, uint32_t size)
        {
            return graph.CreateTexture({ name, RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(size, size),
                                         RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled });
        };
        PassInvocation prepass("DepthPrepass");
        prepass.SetSlot("depth", depth("depth", 640));
        registry.Find("DepthPrepass")->Build(graph, prepass);
        PassInvocation shadow("Shadow");
        shadow.SetSlot("shadowMap", depth("shadowMap", 512));
        registry.Find("Shadow")->Build(graph, shadow);

        PassInvocation forward("Forward");
        forward.SetParameter("cullBackFaces", cullBackFaces ? "true" : "false");
        forward.SetSlot("color", graph.CreateTexture({ "color", RHI::Format::R16G16B16A16Unorm,
                                                       RGSizePolicy::MakeAbsolute(640, 640),
                                                       RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        forward.SetSlot("depth", prepass.GetSlot("depth"));
        forward.SetSlot("shadowMap", shadow.GetSlot("shadowMap"));
        registry.Find("Forward")->Build(graph, forward);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(640, 640)),
                         forward.GetSlot("color"));

        REQUIRE(graph.Execute(cmd));
        REQUIRE(cmd.Renderings.size() == 3); // prepass, shadow, forward
    }

    // The commands of the index-th rendering, from its "begin" up to the next one.
    CommandList Rendering(const RecordingCommandList& cmd, size_t index)
    {
        std::vector<size_t> begins;
        for (size_t i = 0; i < cmd.Commands.size(); ++i)
            if (cmd.Commands[i] == "begin")
                begins.push_back(i);
        REQUIRE(index < begins.size());
        const size_t end = index + 1 < begins.size() ? begins[index + 1] : cmd.Commands.size();
        return CommandList(cmd.Commands.begin() + begins[index], cmd.Commands.begin() + end);
    }

    // What the depth prepass, shadow pass (one cascade) and forward pass recorded for these two
    // rigid instances before skinning existed: one pipeline, the shared streams, 64-byte push constants.
    const CommandList RIGID_PREPASS = { "begin", "pipeline", "set 0", "vertex 1", "index",
                                        "push 64", "draw 36", "push 64", "draw 120" };
    const CommandList RIGID_SHADOW  = RIGID_PREPASS;
    const CommandList RIGID_FORWARD = { "begin", "pipeline", "vertex 3", "index", "set 0", "set 2",
                                        "set 1", "push 64", "draw 36", "set 1", "push 64", "draw 120" };
}

TEST_CASE("A view with only rigid instances records exactly the commands it did before skinning")
{
    SkinnedScene             scene;
    const HX::RenderInstance rigid[] = { Rigid(0, 0), Rigid(1, 1) };

    // The frame has a palette and the skinning streams, as any frame with an animated mesh does,
    // but this view sees no skinned instance.
    GraphFrameData frame  = scene.MakeFrame();
    frame.OpaqueInstances = rigid;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);
    CHECK(Rendering(cmd, 0) == RIGID_PREPASS);
    CHECK(Rendering(cmd, 1) == RIGID_SHADOW);
    CHECK(Rendering(cmd, 2) == RIGID_FORWARD);
    CHECK(std::find(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Palette) == cmd.BoundSets.end());
    CHECK(cmd.PushedPaletteOffsets.empty());
}

TEST_CASE("Skinned instances draw after the rigid ones with the skinned pipelines, the palette and their offsets")
{
    SkinnedScene             scene;
    const HX::RenderInstance rigid[]   = { Rigid(0, 0), Rigid(1, 1) };
    const HX::RenderInstance skinned[] = { Skinned(1, 0, 0), Skinned(0, 0, 2) };

    GraphFrameData frame   = scene.MakeFrame();
    frame.OpaqueInstances  = rigid;
    frame.SkinnedInstances = skinned;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);

    // The rigid draws are unchanged; the skinned ones follow with every set bound again.
    CommandList prepass = RIGID_PREPASS;
    prepass.insert(prepass.end(), { "pipeline", "set 0", "set 1", "vertex 3", "index",
                                    "push 68", "draw 120", "push 68", "draw 36" });
    CHECK(Rendering(cmd, 0) == prepass);

    // The shadow pass binds the palette once, then each cascade's viewProj again.
    CommandList shadow = RIGID_SHADOW;
    shadow.insert(shadow.end(), { "pipeline", "set 1", "set 0", "vertex 3", "index",
                                  "push 68", "draw 120", "push 68", "draw 36" });
    CHECK(Rendering(cmd, 1) == shadow);

    CommandList forward = RIGID_FORWARD;
    forward.insert(forward.end(), { "pipeline", "vertex 5", "set 0", "set 2", "set 3",
                                    "set 1", "push 68", "draw 120", "push 68", "draw 36" });
    CHECK(Rendering(cmd, 2) == forward);

    const auto bound = [&](EnginePipeline pipeline)
    {
        return std::count(cmd.BoundPipelines.begin(), cmd.BoundPipelines.end(), &services.GetPipeline(pipeline));
    };
    CHECK(bound(EnginePipeline::DepthPrepassSkinned) == 1);
    CHECK(bound(EnginePipeline::ShadowSkinned) == 1);
    CHECK(bound(EnginePipeline::ForwardSkinned) == 1);
    CHECK(bound(EnginePipeline::ForwardSkinnedDoubleSided) == 0);
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Palette) == 3);
    CHECK(cmd.PushedPaletteOffsets == std::vector<uint32_t>{ 0, 2, 0, 2, 0, 2 });
}

TEST_CASE("A double-sided forward pass draws skinned instances with the double-sided skinned pipeline")
{
    SkinnedScene             scene;
    const HX::RenderInstance skinned[] = { Skinned(0, 0, 0) };

    GraphFrameData frame   = scene.MakeFrame();
    frame.SkinnedInstances = skinned;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd, /*cullBackFaces*/ false);
    const auto bound = [&](EnginePipeline pipeline)
    {
        return std::count(cmd.BoundPipelines.begin(), cmd.BoundPipelines.end(), &services.GetPipeline(pipeline));
    };
    CHECK(bound(EnginePipeline::ForwardSkinnedDoubleSided) == 1);
    CHECK(bound(EnginePipeline::ForwardSkinned) == 0);
}

TEST_CASE("Without a joint palette or the skinning streams, skinned instances are not drawn")
{
    SkinnedScene             scene;
    const HX::RenderInstance rigid[]   = { Rigid(0, 0), Rigid(1, 1) };
    const HX::RenderInstance skinned[] = { Skinned(1, 0, 0) };

    GraphFrameData noPalette   = scene.MakeFrame();
    noPalette.OpaqueInstances  = rigid;
    noPalette.SkinnedInstances = skinned;
    noPalette.JointPalette     = nullptr;
    GraphFrameData noStreams   = noPalette;
    noStreams.JointPalette     = &scene.Palette;
    noStreams.Joints           = nullptr;

    for (GraphFrameData* frame : { &noPalette, &noStreams })
    {
        FakeServices         services;
        RecordingCommandList cmd;
        Record(*frame, services, cmd);
        CHECK(Rendering(cmd, 0) == RIGID_PREPASS);
        CHECK(Rendering(cmd, 1) == RIGID_SHADOW);
        CHECK(Rendering(cmd, 2) == RIGID_FORWARD);
    }
}
