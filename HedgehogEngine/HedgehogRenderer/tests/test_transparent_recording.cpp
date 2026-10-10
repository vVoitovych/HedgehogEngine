#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Views/ViewCulling.hpp"

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
    constexpr uint32_t TARGET_SIZE = 64;

    // A view's HDR target (as Forward and the Skybox left it), its prepass depth and the shadow
    // atlas, and the geometry and sets a frame of two Transparent materials points into: 0 one-sided,
    // 1 double-sided.
    struct TransparentScene
    {
        TestTexture Hdr{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::R16G16B16A16Float,
                                           RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
        TestTexture Depth{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::D32Float,
                                             RHI::TextureUsage::DepthStencil } };
        TestTexture Atlas{ RHI::TextureDesc{ TARGET_SIZE, TARGET_SIZE, RHI::Format::D32Float,
                                             RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled } };

        TestBuffer        Positions{ 1024 };
        TestBuffer        TexCoords{ 1024 };
        TestBuffer        Normals{ 1024 };
        TestBuffer        Tangents{ 1024 };
        TestBuffer        Indices{ 1024 };
        TestBuffer        Joints{ 1024 };
        TestBuffer        Weights{ 1024 };
        FakeDescriptorSet Glass;
        FakeDescriptorSet Leaf;
        FakeDescriptorSet SceneLights;
        FakeDescriptorSet Palette;

        const RHI::IRHIDescriptorSet* Materials[2] = { &Glass, &Leaf };
        const MaterialDrawInfo        Infos[2]     = { { MaterialAlphaMode::Transparent, false },
                                                       { MaterialAlphaMode::Transparent, true } };
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

    // A lone ForwardTransparent pass over the scene's targets, run with frame into cmd.
    void Record(GraphFrameData* frame, FakeServices& services, TransparentScene& scene, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        const GraphFrameContext context{ &services, frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(frame ? &context : nullptr);

        const RGTexture hdr   = graph.ImportTexture("hdr", RHI::Format::R16G16B16A16Float, false);
        const RGTexture depth = graph.ImportTexture("depth", RHI::Format::D32Float, true);
        const RGTexture atlas = graph.ImportTexture("shadowAtlas", RHI::Format::D32Float, true);
        graph.BindImportedTexture(hdr, &scene.Hdr);
        graph.BindImportedTexture(depth, &scene.Depth);
        graph.BindImportedTexture(atlas, &scene.Atlas);

        PassInvocation pass("ForwardTransparent");
        pass.SetSlot("color", hdr);
        pass.SetSlot("depth", depth);
        pass.SetSlot("shadowMap", atlas);
        registry.Find("ForwardTransparent")->Build(graph, pass);
        graph.BindOutput(graph.AddOutputSlot("hdr", RHI::Format::R16G16B16A16Float,
                                             RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE)),
                         pass.GetSlot("color"));
        REQUIRE(graph.Execute(cmd));
    }
}

TEST_CASE("ForwardTransparent - blends its instances over the HDR target in order, a double-sided one back faces first")
{
    TransparentScene         scene;
    const HX::RenderInstance rigid[]   = { Rigid(0, 0), Rigid(1, 1), Rigid(0, 0) };
    const HX::RenderInstance skinned[] = { Skinned(1, 1, 3) };

    GraphFrameData frame              = scene.MakeFrame();
    frame.TransparentInstances        = rigid;
    frame.SkinnedTransparentInstances = skinned;
    // Opaque and Cutoff instances are Forward's, never this pass's.
    frame.OpaqueInstances = rigid;
    frame.CutoffInstances = rigid;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(&frame, services, scene, cmd);

    // The HDR target is loaded (blended over, not cleared) against the read-only prepass depth.
    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Load);
    REQUIRE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.Renderings[0].DepthAttachment->Texture == &scene.Depth);

    const std::vector<std::string> expected = {
        "begin", "pipeline", "vertex 4", "index", "set 0", "set 2", "set 3",
        "set 1", "push 64", "draw 36",                                   // glass
        "set 1", "pipeline", "push 64", "draw 120", "pipeline", "push 64", "draw 120", // leaf: back, then front
        "set 1", "push 64", "draw 36",                                   // glass again, in its place
        "pipeline", "vertex 6", "index", "set 0", "set 2", "set 3", "set 4",
        "set 1", "pipeline", "push 68", "draw 120", "pipeline", "push 68", "draw 120", // skinned leaf
    };
    CHECK(cmd.Commands == expected);
    const std::vector<const RHI::IRHIPipeline*> pipelines = {
        &services.GetPipeline(EnginePipeline::ForwardTransparent),
        &services.GetPipeline(EnginePipeline::ForwardTransparentBackFaces),
        &services.GetPipeline(EnginePipeline::ForwardTransparent),
        &services.GetPipeline(EnginePipeline::ForwardTransparentSkinned),
        &services.GetPipeline(EnginePipeline::ForwardTransparentSkinnedBackFaces),
        &services.GetPipeline(EnginePipeline::ForwardTransparentSkinned),
    };
    CHECK(cmd.BoundPipelines == pipelines);
    // Lit as Forward lights: the shadow atlas beside the frame's environment at set 3.
    REQUIRE(services.LightingAtlases.size() == 1);
    CHECK(services.LightingAtlases[0] == &scene.Atlas);
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Palette) == 1);
}

TEST_CASE("ForwardTransparent - nothing recorded without transparent instances or a frame context")
{
    TransparentScene scene;
    FakeServices     services;

    SUBCASE("No transparent instances")
    {
        const HX::RenderInstance opaque[] = { Rigid(0, 0) };
        GraphFrameData           frame    = scene.MakeFrame();
        frame.OpaqueInstances             = opaque;
        RecordingCommandList cmd;
        Record(&frame, services, scene, cmd);
        CHECK(cmd.Commands.empty());
    }
    SUBCASE("Skinned ones without a joint palette")
    {
        const HX::RenderInstance skinned[] = { Skinned(1, 1, 0) };
        GraphFrameData           frame     = scene.MakeFrame();
        frame.SkinnedTransparentInstances  = skinned;
        frame.JointPalette                 = nullptr;
        RecordingCommandList cmd;
        Record(&frame, services, scene, cmd);
        CHECK(cmd.Commands.empty());
    }
    SUBCASE("No frame context")
    {
        RecordingCommandList cmd;
        Record(nullptr, services, scene, cmd);
        CHECK(cmd.Commands.empty());
    }
    CHECK(services.LightingUploads.empty());
}

TEST_CASE("ForwardTransparent - its slots match Forward's")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("ForwardTransparent") != nullptr);
    CHECK(registry.Find("ForwardTransparent")->Slots == std::vector<std::string>{ "color", "depth", "shadowMap" });
}

TEST_CASE("Transparent instances sort back to front from the eye, ties by source id")
{
    const auto at = [](uint64_t id, float x)
    {
        HX::RenderInstance instance;
        instance.SourceId    = id;
        instance.WorldBounds = HM::AABB(HM::Vector3(x - 0.5f, -0.5f, -0.5f), HM::Vector3(x + 0.5f, 0.5f, 0.5f));
        return instance;
    };
    std::vector<HX::RenderInstance> instances = { at(1, 2.0f), at(2, 10.0f), at(3, -6.0f), at(5, 4.0f), at(4, 4.0f) };
    SortBackToFront(instances, HM::Vector3(0.0f, 0.0f, 0.0f));

    std::vector<uint64_t> ids;
    for (const HX::RenderInstance& instance : instances)
        ids.push_back(instance.SourceId);
    CHECK(ids == std::vector<uint64_t>{ 2, 3, 4, 5, 1 }); // 10, 6, then 4 and 4 by id, then 2

    // From the other side the order turns around.
    SortBackToFront(instances, HM::Vector3(20.0f, 0.0f, 0.0f));
    ids.clear();
    for (const HX::RenderInstance& instance : instances)
        ids.push_back(instance.SourceId);
    CHECK(ids == std::vector<uint64_t>{ 3, 1, 4, 5, 2 });
}
