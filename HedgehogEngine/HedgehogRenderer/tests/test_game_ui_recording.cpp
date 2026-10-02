#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"

#include "../src/Graph/Passes/GameUiPass.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <string>
#include <vector>

using namespace Renderer;
using namespace RGTest;

namespace
{
    constexpr uint32_t TARGET_SIZE = 64;

    // Builds a lone GameUi pass over a TARGET_SIZE colour target and runs it with frame into cmd.
    void RecordGameUi(const GraphFrameData& frame, RecordingCommandList& cmd)
    {
        PassBuilderRegistry registry;
        RegisterEnginePassTypes(registry);
        FakeServices      services;
        GraphFrameContext context{ &services, &frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(&context);

        PassInvocation ui("GameUi");
        ui.SetSlot("color", graph.CreateTexture({ "color", RHI::Format::R16G16B16A16Unorm,
                                                  RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE),
                                                  RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
        registry.Find("GameUi")->Build(graph, ui);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm,
                                             RGSizePolicy::MakeAbsolute(TARGET_SIZE, TARGET_SIZE)),
                         ui.GetSlot("color"));

        REQUIRE(graph.Execute(cmd));
    }

    std::vector<std::string> CommandsOf(const GraphFrameData& frame)
    {
        RecordingCommandList cmd;
        RecordGameUi(frame, cmd);
        return cmd.Commands;
    }

    HX::UiDrawCommand Command(uint32_t texture, uint32_t firstIndex, uint32_t indexCount, HX::UiRect scissor,
                              uint32_t vertexOffset = 0)
    {
        return { texture, scissor, vertexOffset, firstIndex, indexCount };
    }
}

TEST_CASE("GameUi - a frame without UI records nothing")
{
    TestBuffer        vertices(1024);
    TestBuffer        indices(1024);
    FakeDescriptorSet solid;

    GraphFrameData empty;
    CHECK(CommandsOf(empty).empty());

    // Commands but no geometry or texture yet, as before the first upload: nothing either.
    const HX::UiDrawCommand commands[] = { Command(HX::UI_NO_TEXTURE, 0, 6, { 0.0f, 0.0f, 64.0f, 64.0f }) };
    GraphFrameData          noGeometry;
    noGeometry.UiCommands     = commands;
    noGeometry.UiSolidTexture = &solid;
    CHECK(CommandsOf(noGeometry).empty());

    // Geometry and a texture, but no commands.
    GraphFrameData noCommands;
    noCommands.UiVertices     = &vertices;
    noCommands.UiIndices      = &indices;
    noCommands.UiSolidTexture = &solid;
    CHECK(CommandsOf(noCommands).empty());
}

TEST_CASE("GameUi - each command is one indexed draw with its scissor and texture, over the view's colour")
{
    TestBuffer        vertices(1024);
    TestBuffer        indices(1024);
    FakeDescriptorSet solid;
    FakeDescriptorSet image;

    const HX::UiDrawCommand commands[] = {
        Command(HX::UI_NO_TEXTURE, 0, 6, { 0.0f, 0.0f, 32.0f, 32.0f }),
        Command(0, 6, 12, { 8.5f, 4.25f, 10.0f, 10.0f }, 4),
        Command(1, 18, 6, { 0.0f, 0.0f, 64.0f, 64.0f }),   // a texture that did not load: the solid fill
        Command(0, 24, 6, { 100.0f, 0.0f, 10.0f, 10.0f }), // outside the target: skipped
    };
    const RHI::IRHIDescriptorSet* textures[] = { &image, nullptr };

    GraphFrameData frame;
    frame.UiCommands     = commands;
    frame.UiVertices     = &vertices;
    frame.UiIndices      = &indices;
    frame.UiTextureSets  = textures;
    frame.UiSolidTexture = &solid;
    frame.UiTargetSize   = HM::Vector2(64.0f, 64.0f);

    RecordingCommandList cmd;
    RecordGameUi(frame, cmd);
    CHECK(cmd.Commands == std::vector<std::string>{ "begin", "pipeline", "vertex 1", "index", "push 16",
                                                    "set 0", "draw 6", "set 0", "draw 12", "set 0", "draw 6" });
    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Load);
    CHECK_FALSE(cmd.Renderings[0].DepthAttachment.has_value());
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ &solid, &image, &solid });

    // Scissors round outward to whole pixels.
    REQUIRE(cmd.Scissors.size() == 3);
    CHECK(cmd.Scissors[1].X == 8);
    CHECK(cmd.Scissors[1].Y == 4);
    CHECK(cmd.Scissors[1].Width == 11);
    CHECK(cmd.Scissors[1].Height == 11);
}

TEST_CASE("GameUi - pixels of the UI's target map onto clip space, and scissors follow a differently sized target")
{
    const GameUiPushConstants constants = MakeGameUiPushConstants(HM::Vector2(800.0f, 600.0f));
    CHECK(0.0f * constants.Scale[0] + constants.Offset[0] == doctest::Approx(-1.0f));
    CHECK(800.0f * constants.Scale[0] + constants.Offset[0] == doctest::Approx(1.0f));
    CHECK(0.0f * constants.Scale[1] + constants.Offset[1] == doctest::Approx(-1.0f)); // y down, as Vulkan's clip space
    CHECK(600.0f * constants.Scale[1] + constants.Offset[1] == doctest::Approx(1.0f));

    // The UI was laid out for 800x600 and draws into a 400x300 target: everything halves.
    const RHI::Scissor half = MakeGameUiScissor({ 100.0f, 50.0f, 200.0f, 100.0f }, HM::Vector2(800.0f, 600.0f), 400, 300);
    CHECK(half.X == 50);
    CHECK(half.Y == 25);
    CHECK(half.Width == 100);
    CHECK(half.Height == 50);

    // Clamped to the target, and empty when wholly outside it.
    const RHI::Scissor clamped = MakeGameUiScissor({ -10.0f, 590.0f, 50.0f, 50.0f }, HM::Vector2(800.0f, 600.0f), 800, 600);
    CHECK(clamped.X == 0);
    CHECK(clamped.Y == 590);
    CHECK(clamped.Width == 40);
    CHECK(clamped.Height == 10);
    CHECK(MakeGameUiScissor({ 900.0f, 0.0f, 10.0f, 10.0f }, HM::Vector2(800.0f, 600.0f), 800, 600).Width == 0);

    // No UI target size: the colour target's own.
    const RHI::Scissor own = MakeGameUiScissor({ 10.0f, 10.0f, 20.0f, 20.0f }, HM::Vector2(0.0f, 0.0f), 64, 64);
    CHECK(own.X == 10);
    CHECK(own.Width == 20);
}

TEST_CASE("GameUi - text samples its font's atlas, and a font without one draws nothing")
{
    TestBuffer        vertices(1024);
    TestBuffer        indices(1024);
    FakeDescriptorSet solid;
    FakeDescriptorSet font;

    const HX::UiRect        whole      = { 0.0f, 0.0f, 64.0f, 64.0f };
    const HX::UiDrawCommand commands[] = {
        Command(HX::UI_FONT_TEXTURE | 0u, 0, 12, whole),
        Command(HX::UI_FONT_TEXTURE | 1u, 12, 6, whole), // a font past the registry's budget
        Command(HX::UI_FONT_TEXTURE | 5u, 18, 6, whole), // not in the frame at all
    };
    const RHI::IRHIDescriptorSet* fonts[] = { &font, nullptr };

    GraphFrameData frame;
    frame.UiCommands     = commands;
    frame.UiVertices     = &vertices;
    frame.UiIndices      = &indices;
    frame.UiFontSets     = fonts;
    frame.UiSolidTexture = &solid;

    RecordingCommandList cmd;
    RecordGameUi(frame, cmd);
    CHECK(cmd.DrawnIndexCounts == std::vector<uint32_t>{ 12 });
    CHECK(cmd.BoundSets == std::vector<const RHI::IRHIDescriptorSet*>{ &font });
}
