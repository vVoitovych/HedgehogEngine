#include "../src/Graph/Passes/SelectionOutlinePass.hpp"

#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cstring>
#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t WIDTH  = 80;
    constexpr uint32_t HEIGHT = 40;

    // The Scene view's colour output (tone mapped) and the selection's mask.
    struct OutlineTargets
    {
        TestTexture Color{ RHI::TextureDesc{ WIDTH, HEIGHT, RHI::Format::R16G16B16A16Unorm,
                                             RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
        TestTexture Mask{ RHI::TextureDesc{ WIDTH, HEIGHT, RHI::Format::R8Unorm,
                                            RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled } };
    };

    // A lone SelectionOutline pass over the targets, run with frame into cmd.
    void Record(GraphFrameData* frame, FakeServices& services, OutlineTargets& targets, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        const GraphFrameContext context{ &services, frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(frame ? &context : nullptr);

        const RGTexture color = graph.ImportTexture("color", RHI::Format::R16G16B16A16Unorm, false);
        const RGTexture mask  = graph.ImportTexture("selectionMask", RHI::Format::R8Unorm, true);
        graph.BindImportedTexture(color, &targets.Color);
        graph.BindImportedTexture(mask, &targets.Mask);

        PassInvocation pass("SelectionOutline");
        pass.SetSlot("color", color);
        pass.SetSlot("mask", mask);
        registry.Find("SelectionOutline")->Build(graph, pass);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm,
                                             RGSizePolicy::MakeAbsolute(WIDTH, HEIGHT)),
                         pass.GetSlot("color"));
        REQUIRE(graph.Execute(cmd));
    }
}

TEST_CASE("SelectionOutline - one fullscreen triangle over the loaded colour, sampling the mask")
{
    OutlineTargets           targets;
    const HX::RenderInstance overlay[] = { Instance(0, HX::EDITOR_LAYER) };
    GraphFrameData           frame     = MakeFrame(1);
    frame.OverlayInstances             = overlay;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(&frame, services, targets, cmd);

    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].Texture == &targets.Color);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Load); // blended over the view
    CHECK_FALSE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "set 0", "push 16" });
    CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ 3 }); // the fullscreen triangle
    CHECK(cmd.BoundPipelines == std::vector<const RHI::IRHIPipeline*>{
                                    &services.GetPipeline(EnginePipeline::SelectionOutline) });
    REQUIRE(services.SampledTextures.size() == 1);
    CHECK(services.SampledTextures[0] == &targets.Mask);
    CHECK(cmd.BoundSets[0] == &services.GetSampledTextureSet());

    // Its push constants: one texel of the target in UV and the outline's width.
    REQUIRE(cmd.PushedData.size() == 1);
    SelectionOutlinePushConstants pushed;
    std::memcpy(&pushed, cmd.PushedData[0].data(), sizeof(pushed));
    CHECK(pushed.TexelSize[0] == doctest::Approx(1.0f / WIDTH));
    CHECK(pushed.TexelSize[1] == doctest::Approx(1.0f / HEIGHT));
    CHECK(pushed.Width == SELECTION_OUTLINE_WIDTH);
}

TEST_CASE("SelectionOutline - nothing recorded without a selection or a frame context")
{
    OutlineTargets targets;
    FakeServices   services;

    SUBCASE("No selection")
    {
        GraphFrameData       frame = MakeFrame(1);
        RecordingCommandList cmd;
        Record(&frame, services, targets, cmd);
        CHECK(cmd.Commands.empty());
    }
    SUBCASE("No frame context")
    {
        RecordingCommandList cmd;
        Record(nullptr, services, targets, cmd);
        CHECK(cmd.Commands.empty());
    }
    CHECK(services.SampledTextures.empty());
}

TEST_CASE("SelectionOutline - its slots, and push constants for an empty target")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    REQUIRE(registry.Find("SelectionOutline") != nullptr);
    CHECK(registry.Find("SelectionOutline")->Slots == std::vector<std::string>{ "color", "mask" });

    const SelectionOutlinePushConstants empty = MakeSelectionOutlinePushConstants(0, 0);
    CHECK(empty.TexelSize[0] == 0.0f);
    CHECK(empty.TexelSize[1] == 0.0f);
}
