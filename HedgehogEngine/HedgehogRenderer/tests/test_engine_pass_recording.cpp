#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphCompiler.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <algorithm>
#include <cmath>

using namespace Renderer;
using namespace RGTest;

namespace
{
    RGTexture DeclareDepth(RenderGraphRuntime& graph, const char* name, uint32_t size)
    {
        return graph.CreateTexture({ name, RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(size, size),
                                     RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled });
    }

    // Declares a depth prepass and a shadow pass through the registry and binds both as outputs, so
    // neither is culled.
    void DeclareDepthAndShadow(RenderGraphRuntime& graph, const PassBuilderRegistry& registry)
    {
        PassInvocation prepass("DepthPrepass");
        prepass.SetSlot("depth", DeclareDepth(graph, "depth", 640));
        registry.Find("DepthPrepass")->Build(graph, prepass);

        PassInvocation shadow("Shadow");
        shadow.SetSlot("shadowMap", DeclareDepth(graph, "shadowMap", 1024));
        registry.Find("Shadow")->Build(graph, shadow);

        graph.BindOutput(graph.AddOutputSlot("depth", RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(640, 640)),
                         prepass.GetSlot("depth"));
        graph.BindOutput(graph.AddOutputSlot("shadowMap", RHI::Format::D32Float, RGSizePolicy::MakeAbsolute(1024, 1024)),
                         shadow.GetSlot("shadowMap"));
    }
}

TEST_CASE("Shadow cascades tile the map and fit one light matrix per cascade")
{
    for (uint32_t count = 1; count <= MAX_SHADOW_CASCADES; ++count)
    {
        CAPTURE(count);
        const ShadowCascades cascades = ComputeShadowCascades(MakeFrame(count), 2048);
        REQUIRE(cascades.Count == count);

        float coveredArea = 0.0f;
        for (uint32_t i = 0; i < count; ++i)
        {
            const ShadowCascadeViewport& tile = cascades.Viewports[i];
            CHECK(tile.X + tile.Width <= 2048.0f);
            CHECK(tile.Y + tile.Height <= 2048.0f);
            coveredArea += tile.Width * tile.Height;
            for (int e = 0; e < 16; ++e)
                CHECK(std::isfinite(cascades.ViewProj[i].GetBuffer()[e]));
        }
        CHECK(coveredArea == doctest::Approx(2048.0f * 2048.0f)); // every layout tiles the whole map
    }

    CHECK(ComputeShadowCascades(MakeFrame(0), 512).Count == 1); // clamped
    CHECK(ComputeShadowCascades(MakeFrame(9), 512).Count == MAX_SHADOW_CASCADES);
}

TEST_CASE("The depth prepass and shadow builders record into their graph targets through the frame context")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);

    TestBuffer positions(1024);
    TestBuffer indices(1024);
    const MeshDrawRange meshes[] = { { 0, 36, 0 }, { 36, 120, 24 } };
    const HX::RenderInstance instances[] = { Instance(0), Instance(1), Instance(7) }; // mesh 7 has no range

    GraphFrameData frame = MakeFrame(3);
    frame.OpaqueInstances = instances;
    frame.Meshes          = meshes;
    frame.Positions       = &positions;
    frame.Indices         = &indices;

    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    TestDevice device;
    RenderGraphRuntime graph(device, 64 * 1024);
    graph.SetFrameContext(&context);
    DeclareDepthAndShadow(graph, registry);

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));

    // One dynamic-rendering scope per pass, into the pooled depth textures at their declared sizes.
    REQUIRE(cmd.Renderings.size() == 2);
    CHECK(cmd.EndRenderingCount == 2);
    CHECK(cmd.Renderings[0].Width == 640);
    CHECK(cmd.Renderings[1].Width == 1024);
    for (const RHI::RenderingInfo& info : cmd.Renderings)
    {
        REQUIRE(info.DepthAttachment.has_value());
        CHECK(info.DepthAttachment->Texture != nullptr);
        CHECK(info.DepthAttachment->LoadOp == RHI::LoadOp::Clear);
        CHECK(info.ColorAttachments.empty());
    }

    // Prepass: the two instances with a mesh range. Shadow: the same two, once per cascade.
    CHECK(cmd.DrawnIndexCounts == std::vector<uint32_t>{ 36, 120, 36, 120, 36, 120, 36, 120 });

    // One viewProj for the prepass, one per cascade, each bound before drawing.
    CHECK(services.UploadedFirstElements.size() == 4);
    CHECK(cmd.DescriptorSetBinds == 4);
    CHECK(cmd.Viewports.size() == 4); // full-size prepass viewport + three cascade tiles
    CHECK(cmd.Viewports[0].Width == doctest::Approx(640.0f));
    CHECK(cmd.Viewports[1].Width == doctest::Approx(512.0f));

    CHECK(graph.GetFrameContext() == nullptr); // cleared by Execute: never reused next frame
}

TEST_CASE("Without a frame context the passes declare the same graph and record nothing")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);

    TestDevice device;
    RenderGraphRuntime graph(device, 64 * 1024);
    DeclareDepthAndShadow(graph, registry);
    CHECK(graph.GetDescription().Passes.size() == 2);

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    CHECK(cmd.Renderings.empty());
    CHECK(cmd.DrawnIndexCounts.empty());
}

TEST_CASE("The forward pass packs the camera per view and the lights per frame, capping the light count")
{
    GraphFrameData frame = MakeFrame(1);
    frame.EyePosition = HM::Vector3(1.0f, 2.0f, 3.0f);
    CHECK(MakeForwardViewUniform(frame).EyePosition.y() == doctest::Approx(2.0f));

    HX::RenderLight spot;
    spot.Type      = HX::LightType::Spot;
    spot.Intensity = 4.0f;
    spot.Radius    = 9.0f;
    spot.ConeAngle = 60.0f;
    const std::vector<HX::RenderLight> lights(HedgehogEngine::MAX_LIGHTS_COUNT + 3, spot);

    const SceneLightsUniform uniform = MakeSceneLightsUniform(lights);
    CHECK(uniform.LightCount == static_cast<int32_t>(HedgehogEngine::MAX_LIGHTS_COUNT));
    CHECK(uniform.Lights[0].Data.x() == doctest::Approx(2.0f)); // LightType::Spot
    CHECK(uniform.Lights[0].Data.y() == doctest::Approx(4.0f));
    CHECK(uniform.Lights[0].Data.z() == doctest::Approx(9.0f));
    CHECK(uniform.Lights[0].Data.w() == doctest::Approx(0.5f)); // cos(60 degrees)

    // std140: every light is 64 bytes, and the count follows the array.
    static_assert(sizeof(GpuLight) == 64);
    CHECK(reinterpret_cast<const char*>(&uniform.LightCount) - reinterpret_cast<const char*>(&uniform.Lights[0])
          == static_cast<std::ptrdiff_t>(64 * HedgehogEngine::MAX_LIGHTS_COUNT));
}

TEST_CASE("The forward pass records lit draws into its colour target against the prepass depth")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);

    TestBuffer positions(1024);
    TestBuffer texCoords(1024);
    TestBuffer normals(1024);
    TestBuffer indices(1024);
    FakeDescriptorSet materialA;
    FakeDescriptorSet materialB;
    const RHI::IRHIDescriptorSet* materials[] = { &materialA, &materialB, nullptr };
    const MeshDrawRange meshes[] = { { 0, 36, 0 }, { 36, 120, 24 } };

    const auto instance = [](uint64_t mesh, uint64_t material)
    {
        HX::RenderInstance result = Instance(mesh);
        result.MaterialIndex = material;
        return result;
    };
    // Two share material A (one bind), one uses B; one has no material set and one no mesh range.
    const HX::RenderInstance instances[] = { instance(0, 0), instance(1, 0), instance(1, 1), instance(0, 2),
                                             instance(9, 0) };
    FakeDescriptorSet sceneLights;

    GraphFrameData frame = MakeFrame(1);
    frame.OpaqueInstances = instances;
    frame.SceneLights     = &sceneLights;
    frame.Meshes          = meshes;
    frame.MaterialSets    = materials;
    frame.Positions       = &positions;
    frame.TexCoords       = &texCoords;
    frame.Normals         = &normals;
    frame.Indices         = &indices;

    FakeServices      services;
    GraphFrameContext context{ &services, &frame };

    TestDevice device;
    RenderGraphRuntime graph(device, 64 * 1024);
    graph.SetFrameContext(&context);

    PassInvocation prepass("DepthPrepass");
    prepass.SetSlot("depth", DeclareDepth(graph, "depth", 640));
    registry.Find("DepthPrepass")->Build(graph, prepass);
    PassInvocation shadow("Shadow");
    shadow.SetSlot("shadowMap", DeclareDepth(graph, "shadowMap", 512));
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

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));

    // Prepass, shadow, forward: the graph orders forward last because of its reads.
    REQUIRE(cmd.Renderings.size() == 3);
    const RHI::RenderingInfo& lit = cmd.Renderings[2];
    REQUIRE(lit.ColorAttachments.size() == 1);
    CHECK(lit.ColorAttachments[0].LoadOp == RHI::LoadOp::Clear);
    REQUIRE(lit.DepthAttachment.has_value());
    CHECK(lit.DepthAttachment->LoadOp == RHI::LoadOp::Load); // reads the prepass depth
    CHECK(lit.DepthAttachment->Texture == cmd.Renderings[0].DepthAttachment->Texture);

    CHECK(services.ForwardViewUploads == 1);
    // The last three draws are the forward ones: the instances with both a mesh and a material.
    const std::vector<uint32_t> forwardDraws(cmd.DrawnIndexCounts.end() - 3, cmd.DrawnIndexCounts.end());
    CHECK(forwardDraws == std::vector<uint32_t>{ 36, 120, 120 });
    // The view's set 0 and the frame's lights at set 2 once, then set 1 only when the material
    // changes: A, then B.
    const std::vector<uint32_t> forwardSets(cmd.BoundSetIndices.end() - 4, cmd.BoundSetIndices.end());
    CHECK(forwardSets == std::vector<uint32_t>{ 0, 2, 1, 1 });
}

namespace
{
    struct WrittenTarget
    {
        RGTexture Target{};
    };

    // What an application's UI callback saw.
    struct UiProbe
    {
        int               Calls  = 0;
        RHI::IRHITexture* Target = nullptr;
    };

    void RecordProbe(void* user, RHI::IRHICommandList&, RHI::IRHITexture& target)
    {
        UiProbe& probe = *static_cast<UiProbe*>(user);
        ++probe.Calls;
        probe.Target = &target;
    }

    RGTexture DeclareColor(RenderGraphRuntime& graph, const char* name, RHI::Format format)
    {
        return graph.CreateTexture({ name, format, RGSizePolicy::MakeAbsolute(64, 64),
                                     RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled });
    }
}

TEST_CASE("The Ui pass runs the application's callback into its target, after the views it samples")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    TestDevice         device;
    RenderGraphRuntime graph(device, 64 * 1024);

    // An earlier view writes the target the UI shows.
    RGTexture sceneWritten;
    graph.AddPass<WrittenTarget>("SceneView",
        [&](RGPassBuilder& pass, WrittenTarget& data)
        {
            sceneWritten = data.Target = pass.ColorTarget(DeclareColor(graph, "scene", RHI::Format::R16G16B16A16Unorm));
        },
        [](WrittenTarget&, RHI::IRHICommandList&) {});

    UiProbe         probe;
    GraphFrameData  frame      = MakeFrame(1);
    const RGTexture sampled[]  = { sceneWritten };
    frame.Ui                   = { &RecordProbe, &probe };
    frame.UiSampledTargets     = sampled;
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };
    graph.SetFrameContext(&context);

    PassInvocation ui("Ui");
    ui.SetSlot("target", DeclareColor(graph, "main", RHI::Format::B8G8R8A8Srgb));
    registry.Find("Ui")->Build(graph, ui);
    graph.BindOutput(graph.AddOutputSlot("main", RHI::Format::B8G8R8A8Srgb, RGSizePolicy::MakeAbsolute(64, 64)),
                     ui.GetSlot("target"));

    // The sampled target orders its writer first and is made readable before the UI runs.
    const CompileResult compiled = GraphCompiler{}.Compile(graph.GetDescription());
    REQUIRE(compiled.Success);
    REQUIRE(compiled.Graph.Passes.size() == 2);
    CHECK(compiled.Graph.Passes[0].Name == "SceneView");
    CHECK(compiled.Graph.Passes[1].Name == "Ui");
    const auto& barriers    = compiled.Graph.Passes[1].TextureBarriers;
    const auto  toShaderRead = [&](const RGTextureBarrier& barrier)
    {
        return barrier.Id == sceneWritten.Id && barrier.After == RHI::ResourceState::ShaderResource;
    };
    CHECK(std::any_of(barriers.begin(), barriers.end(), toShaderRead));

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    CHECK(probe.Calls == 1);
    CHECK(probe.Target != nullptr);
    CHECK(graph.GetLastExecutedPassCount() == 2);
}

TEST_CASE("Without a UI callback the Ui pass clears its target")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);
    TestDevice         device;
    RenderGraphRuntime graph(device, 64 * 1024);

    GraphFrameData    frame = MakeFrame(1); // no Ui
    FakeServices      services;
    GraphFrameContext context{ &services, &frame };
    graph.SetFrameContext(&context);

    PassInvocation ui("Ui");
    ui.SetSlot("target", DeclareColor(graph, "main", RHI::Format::B8G8R8A8Srgb));
    registry.Find("Ui")->Build(graph, ui);
    graph.BindOutput(graph.AddOutputSlot("main", RHI::Format::B8G8R8A8Srgb, RGSizePolicy::MakeAbsolute(64, 64)),
                     ui.GetSlot("target"));

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));
    REQUIRE(cmd.Renderings.size() == 1);
    REQUIRE(cmd.Renderings[0].ColorAttachments.size() == 1);
    CHECK(cmd.Renderings[0].ColorAttachments[0].LoadOp == RHI::LoadOp::Clear);
}

TEST_CASE("The Gizmo pass draws each overlay instance's bounds over the view, and nothing without any")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);

    for (const bool selected : { true, false })
    {
        CAPTURE(selected);
        HX::RenderInstance overlay[2] = { Instance(0, HX::EDITOR_LAYER), Instance(1, HX::EDITOR_LAYER) };
        overlay[0].WorldBounds = HM::AABB(HM::Vector3(1.0f, 2.0f, 3.0f), HM::Vector3(3.0f, 3.0f, 7.0f));

        GraphFrameData frame = MakeFrame(1);
        if (selected)
            frame.OverlayInstances = overlay;
        FakeServices      services;
        GraphFrameContext context{ &services, &frame };

        TestDevice         device;
        RenderGraphRuntime graph(device, 64 * 1024);
        graph.SetFrameContext(&context);

        PassInvocation prepass("DepthPrepass");
        prepass.SetSlot("depth", DeclareDepth(graph, "depth", 64));
        registry.Find("DepthPrepass")->Build(graph, prepass);

        PassInvocation gizmo("Gizmo");
        gizmo.SetSlot("color", DeclareColor(graph, "color", RHI::Format::R16G16B16A16Unorm));
        gizmo.SetSlot("depth", prepass.GetSlot("depth"));
        registry.Find("Gizmo")->Build(graph, gizmo);
        graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Unorm, RGSizePolicy::MakeAbsolute(64, 64)),
                         gizmo.GetSlot("color"));

        RecordingCommandList cmd;
        REQUIRE(graph.Execute(cmd));
        if (!selected)
        {
            CHECK(cmd.Renderings.size() == 1); // the prepass only
            CHECK(cmd.DrawnVertexCounts.empty());
            continue;
        }

        REQUIRE(cmd.Renderings.size() == 2);
        const RHI::RenderingInfo& info = cmd.Renderings[1];
        REQUIRE(info.ColorAttachments.size() == 1);
        CHECK(info.ColorAttachments[0].LoadOp == RHI::LoadOp::Load); // over the lit scene
        REQUIRE(info.DepthAttachment.has_value());
        CHECK(info.DepthAttachment->LoadOp == RHI::LoadOp::Load);
        CHECK(info.DepthAttachment->Texture == cmd.Renderings[0].DepthAttachment->Texture);
        CHECK(cmd.DrawnVertexCounts == std::vector<uint32_t>{ GIZMO_BOX_LINE_VERTICES, GIZMO_BOX_LINE_VERTICES });
    }

    // The unit cube's corners land on the box's.
    const HM::AABB      box(HM::Vector3(1.0f, 2.0f, 3.0f), HM::Vector3(3.0f, 3.0f, 7.0f));
    const HM::Matrix4x4 model = MakeGizmoBoxMatrix(box);
    const HM::Vector4   low   = model * HM::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    const HM::Vector4   high  = model * HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    CHECK(HM::Vector3(low.x(), low.y(), low.z()) == box.GetMin());
    CHECK(HM::Vector3(high.x(), high.y(), high.z()) == box.GetMax());
}
