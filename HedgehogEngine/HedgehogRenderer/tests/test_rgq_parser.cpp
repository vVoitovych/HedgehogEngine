#include "doctest/doctest/doctest.h"

#include "RenderGraph/RenderGraphLoader.hpp"
#include "RenderGraph/RenderGraphDesc.hpp"
#include "RenderGraph/RenderPassRegistry.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"
#include "RenderGraph/IRenderPass.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    // Stub factories: RenderGraphLoader::Parse only ever calls RenderPassRegistry::IsRegistered
    // (V4), never Create — so these are never actually invoked. They exist purely to give Parse a
    // realistic set of known type names, mirroring what Renderer registers in production.
    Renderer::RenderPassRegistry MakeTestRegistry()
    {
        Renderer::RenderPassRegistry registry;
        for (const char* type : { "InitPass", "ShadowmapPass", "DepthPrePass", "ForwardPass",
                                  "GizmoPass", "GuiPass", "PresentPass" })
        {
            registry.Register(type, [](const Renderer::PassInitContext&, const Renderer::NodeDesc&)
                              -> std::unique_ptr<Renderer::IRenderPass> { return nullptr; });
        }
        return registry;
    }

    constexpr const char* kValidFrame = R"(
version: 1
stage:   frame

resources:
  - name:   shadowDepth
    kind:   texture
    format: depth_preferred
    size:   fixed
    width:  1024
    height: 1024
    usage:  [depth_stencil]

nodes:
  - type:     InitPass
    instance: init
  - type:     ShadowmapPass
    instance: shadowmap
    outputs:
      - { name: shadowDepth, usage: shadowWrite }
)";

    constexpr const char* kValidView = R"(
version: 1
stage:   view

resources:
  - name:   viewDepth
    kind:   texture
    format: depth_preferred
    size:   view
    usage:  [depth_stencil]
  - name:   viewColor
    kind:   texture
    format: rgba16
    size:   view
    usage:  [color_attachment, sampled]

nodes:
  - type:     DepthPrePass
    instance: depthPre
    outputs:
      - { name: viewDepth, usage: depthWrite }
  - type:     ForwardPass
    instance: forward
    inputs:
      - { name: viewDepth, usage: depthRead }
    outputs:
      - { name: viewColor, usage: colorWrite }
  - type:     GizmoPass
    instance: gizmo
    outputs:
      - { name: viewColor, usage: colorWrite }
)";

    constexpr const char* kValidComposition = R"(
version: 1
stage:   composition

resources:
  - name:   guiColor
    kind:   texture
    format: rgba16
    size:   swapchain
    usage:  [color_attachment, transfer_src]

nodes:
  - type:     GuiPass
    instance: gui
    inputs:
      - { name: views:*, usage: shaderRead }
    outputs:
      - { name: guiColor, usage: colorWrite }
  - type:     PresentPass
    instance: present
    inputs:
      - { name: guiColor, usage: presentSource }
)";
}

TEST_CASE("valid frame/view/composition assets parse, preserving node order")
{
    const auto registry = MakeTestRegistry();

    const auto frame = Renderer::RenderGraphLoader::Parse(kValidFrame, "<frame>", Renderer::GraphStage::Frame, registry);
    REQUIRE(frame.has_value());
    CHECK(frame->Stage == Renderer::GraphStage::Frame);
    REQUIRE(frame->Nodes.size() == 2);
    CHECK(frame->Nodes[0].Type == "InitPass");
    CHECK(frame->Nodes[1].Type == "ShadowmapPass");

    const auto view = Renderer::RenderGraphLoader::Parse(kValidView, "<view>", Renderer::GraphStage::View, registry);
    REQUIRE(view.has_value());
    REQUIRE(view->Nodes.size() == 3);
    CHECK(view->Nodes[0].Type == "DepthPrePass");
    CHECK(view->Nodes[1].Type == "ForwardPass");
    CHECK(view->Nodes[2].Type == "GizmoPass");

    const auto composition = Renderer::RenderGraphLoader::Parse(kValidComposition, "<composition>", Renderer::GraphStage::Composition, registry);
    REQUIRE(composition.has_value());
    REQUIRE(composition->Nodes.size() == 2);
    CHECK(composition->Nodes[0].Type == "GuiPass");
    CHECK(composition->Nodes[1].Type == "PresentPass");
}

TEST_CASE("V1: version/stage")
{
    const auto registry = MakeTestRegistry();

    SUBCASE("missing version")
    {
        const char* text = "stage: frame\nnodes:\n  - {type: InitPass, instance: init}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("unsupported version")
    {
        const char* text = "version: 2\nstage: frame\nnodes:\n  - {type: InitPass, instance: init}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("missing stage")
    {
        const char* text = "version: 1\nnodes:\n  - {type: InitPass, instance: init}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("unknown stage")
    {
        const char* text = "version: 1\nstage: bogus\nnodes:\n  - {type: InitPass, instance: init}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
}

TEST_CASE("V2: stage/slot mismatch")
{
    const auto registry = MakeTestRegistry();
    // kValidFrame declares stage: frame; ask for it as a view asset.
    CHECK(!Renderer::RenderGraphLoader::Parse(kValidFrame, "<t>", Renderer::GraphStage::View, registry).has_value());
}

TEST_CASE("V3: nodes / InitPass / PresentPass invariants")
{
    const auto registry = MakeTestRegistry();

    SUBCASE("missing nodes")
    {
        const char* text = "version: 1\nstage: frame\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("empty nodes")
    {
        const char* text = "version: 1\nstage: frame\nnodes: []\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("node missing type")
    {
        const char* text = "version: 1\nstage: frame\nnodes:\n  - {instance: init}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("node missing instance")
    {
        const char* text = "version: 1\nstage: frame\nnodes:\n  - {type: InitPass}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("frame without InitPass")
    {
        const char* text = "version: 1\nstage: frame\nnodes:\n  - {type: ShadowmapPass, instance: sm}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("composition without PresentPass")
    {
        const char* text = "version: 1\nstage: composition\nnodes:\n  - {type: GuiPass, instance: gui}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Composition, registry).has_value());
    }
    SUBCASE("PresentPass with no inputs")
    {
        const char* text = "version: 1\nstage: composition\nnodes:\n  - {type: PresentPass, instance: present}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Composition, registry).has_value());
    }
}

TEST_CASE("V4: registered type / duplicate instance")
{
    const auto registry = MakeTestRegistry();

    SUBCASE("unregistered type")
    {
        const char* text = "version: 1\nstage: frame\nnodes:\n  - {type: NotAPass, instance: x}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("duplicate instance")
    {
        const char* text =
            "version: 1\nstage: frame\nnodes:\n"
            "  - {type: InitPass, instance: dup}\n"
            "  - {type: ShadowmapPass, instance: dup}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
}

TEST_CASE("V5: resource format/size/usage")
{
    const auto registry = MakeTestRegistry();

    SUBCASE("unknown format")
    {
        const char* text =
            "version: 1\nstage: view\n"
            "resources:\n  - {name: viewColor, kind: texture, format: bogus, size: view, usage: [sampled]}\n"
            "nodes:\n  - {type: ForwardPass, instance: f, outputs: [{name: viewColor, usage: colorWrite}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
    }
    SUBCASE("unknown size")
    {
        const char* text =
            "version: 1\nstage: view\n"
            "resources:\n  - {name: viewColor, kind: texture, format: rgba16, size: bogus, usage: [sampled]}\n"
            "nodes:\n  - {type: ForwardPass, instance: f, outputs: [{name: viewColor, usage: colorWrite}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
    }
    SUBCASE("fixed without width/height")
    {
        const char* text =
            "version: 1\nstage: frame\n"
            "resources:\n  - {name: shadowDepth, kind: texture, format: depth_preferred, size: fixed, usage: [depth_stencil]}\n"
            "nodes:\n"
            "  - {type: InitPass, instance: init}\n"
            "  - {type: ShadowmapPass, instance: sm, outputs: [{name: shadowDepth, usage: shadowWrite}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("size: view illegal in a frame asset")
    {
        const char* text =
            "version: 1\nstage: frame\n"
            "resources:\n  - {name: shadowDepth, kind: texture, format: depth_preferred, size: view, usage: [depth_stencil]}\n"
            "nodes:\n"
            "  - {type: InitPass, instance: init}\n"
            "  - {type: ShadowmapPass, instance: sm, outputs: [{name: shadowDepth, usage: shadowWrite}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value());
    }
    SUBCASE("unknown usage flag")
    {
        const char* text =
            "version: 1\nstage: view\n"
            "resources:\n  - {name: viewColor, kind: texture, format: rgba16, size: view, usage: [bogus_flag]}\n"
            "nodes:\n  - {type: ForwardPass, instance: f, outputs: [{name: viewColor, usage: colorWrite}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
    }
}

TEST_CASE("V6: name reference resolution")
{
    const auto registry = MakeTestRegistry();

    SUBCASE("input references undeclared resource")
    {
        const char* text =
            "version: 1\nstage: view\n"
            "nodes:\n  - {type: ForwardPass, instance: f, inputs: [{name: notDeclared, usage: depthRead}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
    }
    SUBCASE("malformed shared: reference")
    {
        const char* text =
            "version: 1\nstage: view\n"
            "nodes:\n  - {type: ForwardPass, instance: f, inputs: [{name: 'shared:', usage: shadowRead}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
    }
    SUBCASE("malformed views: reference")
    {
        const char* text =
            "version: 1\nstage: composition\n"
            "nodes:\n"
            "  - {type: PresentPass, instance: present, inputs: [{name: 'views:', usage: presentSource}]}\n";
        CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Composition, registry).has_value());
    }
    SUBCASE("views:* accepted")
    {
        const auto composition = Renderer::RenderGraphLoader::Parse(kValidComposition, "<t>", Renderer::GraphStage::Composition, registry);
        REQUIRE(composition.has_value());
        REQUIRE(composition->Nodes[0].Inputs.size() == 1);
        CHECK(composition->Nodes[0].Inputs[0].Name == "views:*");
    }
}

TEST_CASE("V7: unknown slot usage")
{
    const auto registry = MakeTestRegistry();
    const char* text =
        "version: 1\nstage: view\n"
        "resources:\n  - {name: viewColor, kind: texture, format: rgba16, size: view, usage: [sampled]}\n"
        "nodes:\n  - {type: ForwardPass, instance: f, outputs: [{name: viewColor, usage: bogusUsage}]}\n";
    CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry).has_value());
}

TEST_CASE("V8: unreferenced resource warns but still parses")
{
    const auto registry = MakeTestRegistry();
    const char* text =
        "version: 1\nstage: view\n"
        "resources:\n"
        "  - {name: viewColor, kind: texture, format: rgba16, size: view, usage: [sampled]}\n"
        "  - {name: unused, kind: texture, format: rgba16, size: view, usage: [sampled]}\n"
        "nodes:\n  - {type: ForwardPass, instance: f, outputs: [{name: viewColor, usage: colorWrite}]}\n";
    const auto desc = Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::View, registry);
    REQUIRE(desc.has_value());
    CHECK(desc->Resources.size() == 2);
}

TEST_CASE("malformed YAML does not throw and returns nullopt")
{
    const auto registry = MakeTestRegistry();
    const char* text = "version: 1\nstage: [frame\n"; // unbalanced bracket
    CHECK_NOTHROW(CHECK(!Renderer::RenderGraphLoader::Parse(text, "<t>", Renderer::GraphStage::Frame, registry).has_value()));
}

TEST_CASE("round-trip: the five shipped .rgq assets parse clean")
{
    const auto registry = MakeTestRegistry();

    // Assumes the test binary runs with the repository root as its working directory — true for
    // Scripts\RunTests.bat, this suite's only supported invocation.
    const std::filesystem::path graphsDir =
        std::filesystem::path("HedgehogEngine") / "HedgehogRenderer" / "assets" / "Graphs";

    struct Asset { const char* file; Renderer::GraphStage stage; };
    const Asset assets[] = {
        { "frame_default.rgq",      Renderer::GraphStage::Frame },
        { "scene_view.rgq",         Renderer::GraphStage::View },
        { "game_view.rgq",          Renderer::GraphStage::View },
        { "composition_editor.rgq", Renderer::GraphStage::Composition },
        { "present_direct.rgq",     Renderer::GraphStage::Composition },
    };

    for (const auto& asset : assets)
    {
        const std::filesystem::path path = graphsDir / asset.file;
        std::ifstream in(path, std::ios::binary);
        REQUIRE_MESSAGE(in.good(), "could not open " << path.string());

        std::ostringstream buffer;
        buffer << in.rdbuf();

        const auto desc = Renderer::RenderGraphLoader::Parse(buffer.str(), path.string(), asset.stage, registry);
        CHECK_MESSAGE(desc.has_value(), "failed to parse " << path.string());
    }
}
