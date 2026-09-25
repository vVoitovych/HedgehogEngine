#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "TestRHIDoubles.hpp"

#include "RHI/api/IRHIDescriptor.hpp"
#include "RHI/api/IRHIPipeline.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>

using namespace Renderer;
using namespace RGTest;

namespace
{
    class FakePipeline final : public RHI::IRHIPipeline
    {
    };

    class FakeDescriptorSet final : public RHI::IRHIDescriptorSet
    {
    public:
        void WriteUniformBuffer(uint32_t, const RHI::IRHIBuffer&, size_t, size_t) override {}
        void WriteStorageBuffer(uint32_t, const RHI::IRHIBuffer&, size_t, size_t) override {}
        void WriteTexture(uint32_t, const RHI::IRHITexture&, const RHI::IRHISampler&) override {}
        void Flush() override {}
    };

    // Stands in for GraphPassServices: fixed pipelines, and a record of every viewProj uploaded.
    class FakeServices final : public IGraphPassServices
    {
    public:
        const RHI::IRHIPipeline& GetPipeline(EnginePipeline pipeline) const override
        {
            switch (pipeline)
            {
                case EnginePipeline::DepthPrepass: return m_Depth;
                case EnginePipeline::Shadow:       return m_Shadow;
                default:                           return m_Forward;
            }
        }
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override
        {
            UploadedFirstElements.push_back(viewProj.GetBuffer()[0]);
            return m_Set;
        }
        const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) override
        {
            ForwardLightCounts.push_back(uniform.LightCount);
            return m_Set;
        }

        std::vector<float>   UploadedFirstElements;
        std::vector<int32_t> ForwardLightCounts;

    private:
        FakePipeline      m_Depth;
        FakePipeline      m_Shadow;
        FakePipeline      m_Forward;
        FakeDescriptorSet m_Set;
    };

    HX::RenderInstance Instance(uint64_t meshIndex)
    {
        HX::RenderInstance instance;
        instance.MeshIndex = meshIndex;
        return instance;
    }

    GraphFrameData MakeFrame(uint32_t cascades)
    {
        GraphFrameData frame;
        frame.View                     = HM::Matrix4x4::LookAt(HM::Vector3(0.0f, -10.0f, 2.0f), HM::Vector3(0.0f, 0.0f, 0.0f),
                                                               HM::Vector3(0.0f, 0.0f, 1.0f));
        frame.Proj                     = HM::Matrix4x4::Perspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        frame.NearPlane                = 0.1f;
        frame.FarPlane                 = 100.0f;
        frame.ShadowCascadeCount       = cascades;
        frame.ShadowCascadeSplitLambda = 0.75f;
        frame.ShadowLightDirection     = HM::Vector3(0.3f, 0.2f, -1.0f);
        return frame;
    }

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

TEST_CASE("The forward pass packs camera and lights into the shader layout, capping the light count")
{
    GraphFrameData frame = MakeFrame(1);
    frame.EyePosition = HM::Vector3(1.0f, 2.0f, 3.0f);

    HX::RenderLight spot;
    spot.Type      = HX::LightType::Spot;
    spot.Intensity = 4.0f;
    spot.Radius    = 9.0f;
    spot.ConeAngle = 60.0f;
    const std::vector<HX::RenderLight> lights(HedgehogEngine::MAX_LIGHTS_COUNT + 3, spot);
    frame.Lights = lights;

    const ForwardViewUniform uniform = MakeForwardViewUniform(frame);
    CHECK(uniform.LightCount == static_cast<int32_t>(HedgehogEngine::MAX_LIGHTS_COUNT));
    CHECK(uniform.EyePosition.y() == doctest::Approx(2.0f));
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
    const HX::RenderLight lights[2] = {};

    GraphFrameData frame = MakeFrame(1);
    frame.OpaqueInstances = instances;
    frame.Lights          = lights;
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

    CHECK(services.ForwardLightCounts == std::vector<int32_t>{ 2 });
    // The last three draws are the forward ones: the instances with both a mesh and a material.
    const std::vector<uint32_t> forwardDraws(cmd.DrawnIndexCounts.end() - 3, cmd.DrawnIndexCounts.end());
    CHECK(forwardDraws == std::vector<uint32_t>{ 36, 120, 120 });
    // Set 0 once, then set 1 only when the material changes: A, then B.
    const std::vector<uint32_t> forwardSets(cmd.BoundSetIndices.end() - 3, cmd.BoundSetIndices.end());
    CHECK(forwardSets == std::vector<uint32_t>{ 0, 1, 1 });
}
