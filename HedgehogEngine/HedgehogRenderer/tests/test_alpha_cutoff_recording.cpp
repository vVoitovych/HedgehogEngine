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
using HedgehogEngine::MaterialAlphaMode;

namespace
{
    using CommandList = std::vector<std::string>;

    // A scene of three materials: 0 opaque, 1 Cutoff, 2 opaque and double-sided.
    struct AlphaScene
    {
        TestBuffer        Positions{ 1024 };
        TestBuffer        TexCoords{ 1024 };
        TestBuffer        Normals{ 1024 };
        TestBuffer        Tangents{ 1024 };
        TestBuffer        Indices{ 1024 };
        TestBuffer        Joints{ 1024 };
        TestBuffer        Weights{ 1024 };
        FakeDescriptorSet Opaque;
        FakeDescriptorSet Cutoff;
        FakeDescriptorSet DoubleSided;
        FakeDescriptorSet SceneLights;
        FakeDescriptorSet Palette;

        const RHI::IRHIDescriptorSet* Materials[3] = { &Opaque, &Cutoff, &DoubleSided };
        const MaterialDrawInfo        Infos[3]     = { { MaterialAlphaMode::Opaque, false },
                                                       { MaterialAlphaMode::Cutoff, false },
                                                       { MaterialAlphaMode::Opaque, true } };
        const MeshDrawRange           Meshes[2]    = { { 0, 36, 0 }, { 36, 120, 24 } };

        GraphFrameData MakeFrame()
        {
            GraphFrameData frame = RGTest::MakeFrame(1);
            frame.SceneLights    = &SceneLights;
            frame.Meshes         = Meshes;
            frame.MaterialSets   = Materials;
            frame.Materials      = Infos;
            frame.Positions      = &Positions;
            frame.TexCoords      = &TexCoords;
            frame.Normals        = &Normals;
            frame.Tangents       = &Tangents;
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

    // Runs a depth prepass and a forward pass over frame, as a view's graph does.
    void Record(GraphFrameData& frame, FakeServices& services, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        GraphFrameContext context{ &services, &frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(&context);

        PassInvocation prepass("DepthPrepass");
        prepass.SetSlot("depth", graph.CreateTexture({ "depth", RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(640, 640),
                                                       RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled }));
        registry.Find("DepthPrepass")->Build(graph, prepass);
        PassInvocation shadow("Shadow");
        shadow.SetSlot("shadowMap", graph.CreateTexture({ "shadowMap", RHI::Format::D32Float,
                                                          RGSizePolicy::MakeAbsolute(512, 512),
                                                          RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled }));
        registry.Find("Shadow")->Build(graph, shadow);

        PassInvocation forward("Forward");
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

    size_t Bound(const RecordingCommandList& cmd, const FakeServices& services, EnginePipeline pipeline)
    {
        return static_cast<size_t>(
            std::count(cmd.BoundPipelines.begin(), cmd.BoundPipelines.end(), &services.GetPipeline(pipeline)));
    }
}

TEST_CASE("Cutoff instances are depth-tested with the cutoff pipelines and their materials, then lit with the rest")
{
    AlphaScene               scene;
    const HX::RenderInstance opaque[]        = { Rigid(0, 0) };
    const HX::RenderInstance cutoff[]        = { Rigid(1, 1), Rigid(0, 1) };
    const HX::RenderInstance skinnedCutoff[] = { Skinned(1, 1, 4) };
    const HX::RenderInstance transparent[]   = { Rigid(0, 0), Rigid(1, 0) };

    GraphFrameData frame              = scene.MakeFrame();
    frame.OpaqueInstances             = opaque;
    frame.CutoffInstances             = cutoff;
    frame.SkinnedCutoffInstances      = skinnedCutoff;
    frame.TransparentInstances        = transparent;
    frame.SkinnedTransparentInstances = transparent;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);

    // The prepass: the opaque instance as before, then the rigid cutoff ones with positions and UVs
    // and their material (bound once for both) at set 1, then the skinned one with positions,
    // joints, weights and UVs, the palette at set 1 and its material at set 2.
    const CommandList prepass = { "begin", "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36",
                                  "pipeline", "set 0", "vertex 2", "index", "set 1", "push 64", "draw 120", "push 64", "draw 36",
                                  "pipeline", "set 0", "set 1", "vertex 4", "index", "set 2", "push 68", "draw 120" };
    CHECK(Rendering(cmd, 0) == prepass);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassCutoff) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassCutoffSkinned) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassCutoffDoubleSided) == 0);
    REQUIRE(cmd.BoundVertexBuffers.size() >= 3);
    CHECK(cmd.BoundVertexBuffers[1] == std::vector<const RHI::IRHIBuffer*>{ &scene.Positions, &scene.TexCoords });
    CHECK(cmd.BoundVertexBuffers[2]
          == std::vector<const RHI::IRHIBuffer*>{ &scene.Positions, &scene.Joints, &scene.Weights, &scene.TexCoords });
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Cutoff) == 6); // 2 each: prepass, shadow, forward

    // The forward pass draws the cutoff ones after the opaque one with the same pipeline (its shader
    // discards), and the skinned cutoff one with the skinned pipeline. Transparent ones are never drawn.
    const CommandList forward = { "begin", "pipeline", "vertex 4", "index", "set 0", "set 2", "set 3",
                                  "set 1", "push 64", "draw 36", "set 1", "push 64", "draw 120", "push 64", "draw 36",
                                  "pipeline", "vertex 6", "set 0", "set 2", "set 3", "set 4", "set 1", "push 68", "draw 120" };
    CHECK(Rendering(cmd, 2) == forward);
    CHECK(Bound(cmd, services, EnginePipeline::Forward) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::ForwardSkinned) == 1);
}

TEST_CASE("A double-sided material's instances switch to the pipelines without back-face culling, and back")
{
    AlphaScene               scene;
    const HX::RenderInstance opaque[]  = { Rigid(0, 0), Rigid(1, 2), Rigid(0, 0) };
    const HX::RenderInstance skinned[] = { Skinned(0, 2, 0) };

    GraphFrameData frame   = scene.MakeFrame();
    frame.OpaqueInstances  = opaque;
    frame.SkinnedInstances = skinned;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);

    // The prepass binds the double-sided twin for the middle instance and the single-sided one after.
    const CommandList prepass = { "begin", "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36",
                                  "pipeline", "push 64", "draw 120", "pipeline", "push 64", "draw 36",
                                  "pipeline", "set 0", "set 1", "vertex 3", "index", "pipeline", "push 68", "draw 36" };
    CHECK(Rendering(cmd, 0) == prepass);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepass) == 2);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassDoubleSided) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassSkinned) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassSkinnedDoubleSided) == 1);

    // The forward pass likewise, its sets left bound across the switch.
    CHECK(Bound(cmd, services, EnginePipeline::Forward) == 2);
    CHECK(Bound(cmd, services, EnginePipeline::ForwardDoubleSided) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::ForwardSkinned) == 1);
    CHECK(Bound(cmd, services, EnginePipeline::ForwardSkinnedDoubleSided) == 1);
    const CommandList forward = Rendering(cmd, 2);
    CHECK(std::count(forward.begin(), forward.end(), "set 0") == 2); // once per layout, not per switch
}

TEST_CASE("Without draw info every instance is drawn single-sided, as before")
{
    AlphaScene               scene;
    const HX::RenderInstance opaque[] = { Rigid(0, 2), Rigid(1, 2) };

    GraphFrameData frame  = scene.MakeFrame();
    frame.Materials       = {};
    frame.OpaqueInstances = opaque;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);
    CHECK(Bound(cmd, services, EnginePipeline::DepthPrepassDoubleSided) == 0);
    CHECK(Bound(cmd, services, EnginePipeline::ForwardDoubleSided) == 0);
}
