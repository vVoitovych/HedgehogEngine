#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t MASK_SIZE = 64;

    // The view's mask target and the geometry a frame with a selection points into.
    struct MaskScene
    {
        TestTexture Mask{ RHI::TextureDesc{ MASK_SIZE, MASK_SIZE, RHI::Format::R8Unorm,
                                            RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };

        TestBuffer        Positions{ 1024 };
        TestBuffer        Indices{ 1024 };
        TestBuffer        Joints{ 1024 };
        TestBuffer        Weights{ 1024 };
        FakeDescriptorSet Palette;

        const MeshDrawRange Meshes[2] = { { 0, 36, 0 }, { 36, 120, 24 } };

        GraphFrameData MakeFrame()
        {
            GraphFrameData frame = RGTest::MakeFrame(1);
            frame.Meshes         = Meshes;
            frame.Positions      = &Positions;
            frame.Indices        = &Indices;
            frame.Joints         = &Joints;
            frame.Weights        = &Weights;
            frame.JointPalette   = &Palette;
            return frame;
        }
    };

    HX::RenderInstance Selected(uint64_t mesh, uint32_t joints = 0, uint32_t paletteOffset = 0)
    {
        HX::RenderInstance instance = Instance(mesh, HX::EDITOR_LAYER);
        instance.JointCount         = joints;
        instance.PaletteOffset      = paletteOffset;
        return instance;
    }

    // A lone SelectionMask pass over the scene's mask, run with frame into cmd.
    void Record(GraphFrameData* frame, FakeServices& services, MaskScene& scene, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        const GraphFrameContext context{ &services, frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(frame ? &context : nullptr);

        const RGTexture mask = graph.ImportTexture("selectionMask", RHI::Format::R8Unorm, false);
        graph.BindImportedTexture(mask, &scene.Mask);
        PassInvocation pass("SelectionMask");
        pass.SetSlot("mask", mask);
        registry.Find("SelectionMask")->Build(graph, pass);
        graph.BindOutput(graph.AddOutputSlot("selectionMask", RHI::Format::R8Unorm,
                                             RGSizePolicy::MakeAbsolute(MASK_SIZE, MASK_SIZE)),
                         pass.GetSlot("mask"));
        REQUIRE(graph.Execute(cmd));
    }
}

TEST_CASE("SelectionMask - clears the mask and draws the selection into it, rigid then skinned")
{
    MaskScene                scene;
    const HX::RenderInstance overlay[] = { Selected(0), Selected(1, 2, 5), Selected(1) };

    GraphFrameData frame   = scene.MakeFrame();
    frame.OverlayInstances = overlay;
    // The rest of the view is never drawn into the mask.
    const HX::RenderInstance scenery[] = { Instance(0), Instance(1) };
    frame.OpaqueInstances              = scenery;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(&frame, services, scene, cmd);

    // The mask alone, cleared to 0: no depth attachment, so nothing hides the selection.
    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Texture == &scene.Mask);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Clear);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Clear.Color.R == 0.0f);
    CHECK_FALSE(cmd.Renderings[0].DepthAttachment.has_value());

    const std::vector<std::string> expected = {
        "begin",
        "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36", "push 64", "draw 120", // rigid
        "pipeline", "set 0", "set 1", "vertex 3", "index", "push 68", "draw 120",              // skinned
    };
    CHECK(cmd.Commands == expected);
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{
                                    &services.GetPipeline(EnginePipeline::SelectionMask),
                                    &services.GetPipeline(EnginePipeline::SelectionMaskSkinned) });
    CHECK(cmd.PushedPaletteOffsets == std::vector<uint32_t>{ 5 });
    CHECK(cmd.BoundSets.back() == &scene.Palette);
}

TEST_CASE("SelectionMask - nothing recorded without a selection, without geometry or without a frame context")
{
    MaskScene    scene;
    FakeServices services;

    SUBCASE("No selection")
    {
        GraphFrameData       frame = scene.MakeFrame();
        RecordingCommandList cmd;
        Record(&frame, services, scene, cmd);
        CHECK(cmd.Commands.empty());
    }
    SUBCASE("No geometry uploaded")
    {
        const HX::RenderInstance overlay[] = { Selected(0) };
        GraphFrameData           frame     = scene.MakeFrame();
        frame.OverlayInstances             = overlay;
        frame.Positions                    = nullptr;
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
}

TEST_CASE("SelectionMask - a skinned selection without a joint palette draws only its rigid part")
{
    MaskScene                scene;
    const HX::RenderInstance overlay[] = { Selected(0), Selected(1, 2, 0) };
    GraphFrameData           frame     = scene.MakeFrame();
    frame.OverlayInstances             = overlay;
    frame.JointPalette                 = nullptr;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(&frame, services, scene, cmd);
    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36" });
}

TEST_CASE("SelectionMask - its one slot is the mask")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("SelectionMask") != nullptr);
    CHECK(registry.Find("SelectionMask")->Slots == std::vector<std::string>{ "mask" });
}
