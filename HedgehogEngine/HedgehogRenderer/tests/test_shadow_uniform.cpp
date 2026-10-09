#include "HedgehogRenderer/Frame/SharedPhase.hpp"
#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include "TestPassServices.hpp"
#include "TestRHIDoubles.hpp"

#include "doctest/doctest/doctest.h"

#include <cmath>
#include <cstddef>
#include <cstring>

using namespace Renderer;
using namespace RGTest;

namespace
{
    // A camera at (0, -10, 2) looking at the origin with a 60 degree perspective, Z up.
    GraphFrameData MakeCameraFrame(uint32_t cascades)
    {
        GraphFrameData frame = MakeFrame(cascades);
        frame.Proj      = HM::Matrix4x4::Perspective(1.0471976f, 16.0f / 9.0f, 0.1f, 100.0f);
        frame.NearPlane = 0.1f;
        frame.FarPlane  = 100.0f;
        return frame;
    }

    HM::Vector3 Transform(const HM::Matrix4x4& matrix, const HM::Vector3& point)
    {
        const HM::Vector4 result = matrix * HM::Vector4(point, 1.0f);
        return HM::Vector3(result.x() / result.w(), result.y() / result.w(), result.z() / result.w());
    }

    bool Same(const HM::Matrix4x4& a, const HM::Matrix4x4& b)
    {
        return std::memcmp(a.GetBuffer(), b.GetBuffer(), 16 * sizeof(float)) == 0;
    }
}

TEST_CASE("Shadow uniform - its std140 offsets match Common/Shadows.glsl's block")
{
    CHECK(offsetof(ShadowUniform, ViewProj) == 0);
    CHECK(offsetof(ShadowUniform, CameraView) == 256);
    CHECK(offsetof(ShadowUniform, TileRects) == 320);
    CHECK(offsetof(ShadowUniform, SplitDepths) == 384);
    CHECK(offsetof(ShadowUniform, NormalOffsets) == 400);
    CHECK(offsetof(ShadowUniform, DepthBias) == 416);
    CHECK(offsetof(ShadowUniform, SlopeBias) == 420);
    CHECK(offsetof(ShadowUniform, CascadeBlend) == 424);
    CHECK(offsetof(ShadowUniform, TexelSize) == 428);
    CHECK(offsetof(ShadowUniform, CascadeCount) == 432);
    CHECK(offsetof(ShadowUniform, PcfRadius) == 436);
    CHECK(offsetof(ShadowUniform, LightIndex) == 440);
}

TEST_CASE("Shadow uniform - the cascades, the camera, the tiles in atlas UV and the sampling values are packed")
{
    const GraphFrameData frame    = MakeCameraFrame(4);
    const ShadowCascades cascades = ComputeShadowCascades(frame, 2048);
    const ShadowSampling sampling{ 0.001f, 0.004f, 2.0f, 3, 0.25f };

    const ShadowUniform uniform = MakeShadowUniform(cascades, frame.View, sampling, 2048, 1);
    CHECK(uniform.CascadeCount == 4);
    CHECK(Same(uniform.CameraView, frame.View));
    float previousSplit = 0.0f;
    for (uint32_t i = 0; i < 4; ++i)
    {
        CAPTURE(i);
        CHECK(Same(uniform.ViewProj[i], cascades.ViewProj[i]));
        // Four cascades are four quarter tiles.
        CHECK(uniform.TileRects[i][0] == cascades.Viewports[i].X / 2048.0f);
        CHECK(uniform.TileRects[i][1] == cascades.Viewports[i].Y / 2048.0f);
        CHECK(uniform.TileRects[i][2] == 0.5f);
        CHECK(uniform.TileRects[i][3] == 0.5f);
        // Splits grow to the far plane, and the normal offset is two of the cascade's texels.
        CHECK(uniform.SplitDepths[i] > previousSplit);
        previousSplit = uniform.SplitDepths[i];
        CHECK(uniform.NormalOffsets[i] == doctest::Approx(2.0f * cascades.WorldTexelSizes[i]));
        CHECK(cascades.WorldTexelSizes[i] > 0.0f);
    }
    CHECK(uniform.SplitDepths[3] == doctest::Approx(100.0f));
    CHECK(uniform.DepthBias == 0.001f);
    CHECK(uniform.SlopeBias == 0.004f);
    CHECK(uniform.CascadeBlend == 0.25f);
    CHECK(uniform.PcfRadius == 3);
    CHECK(uniform.TexelSize == 1.0f / 2048.0f);
    CHECK(uniform.LightIndex == 1);

    // A cascade's texels grow with it: the far ones cover more of the world.
    CHECK(cascades.WorldTexelSizes[3] > cascades.WorldTexelSizes[0]);

    const ShadowUniform unshadowed = MakeUnshadowedUniform();
    CHECK(unshadowed.CascadeCount == 0);
    CHECK(unshadowed.LightIndex == -1);
}

TEST_CASE("Shadow cascades - every point of the camera's frustum lies inside the cascade its depth picks")
{
    for (uint32_t count = 1; count <= MAX_SHADOW_CASCADES; ++count)
    {
        CAPTURE(count);
        GraphFrameData frame       = MakeCameraFrame(count);
        frame.ShadowLightDirection = HM::Vector3(0.3f, -0.4f, 0.85f); // towards the sun
        const ShadowCascades cascades      = ComputeShadowCascades(frame, 2048);
        const HM::Matrix4x4  inverseCamera = (frame.Proj * frame.View).Inverse();

        for (float depth : { 0.5f, 3.0f, 9.0f, 20.0f, 45.0f, 80.0f, 99.0f })
        {
            for (float x : { -0.95f, 0.0f, 0.95f })
            {
                for (float y : { -0.95f, 0.0f, 0.95f })
                {
                    CAPTURE(depth);
                    CAPTURE(x);
                    CAPTURE(y);
                    // The NDC depth of that view distance under the [0, 1] perspective.
                    const float       ndcZ  = (100.0f / 99.9f) * (1.0f - 0.1f / depth);
                    const HM::Vector3 world = Transform(inverseCamera, HM::Vector3(x, y, ndcZ));
                    const float       viewDepth = -Transform(frame.View, world).z();
                    REQUIRE(viewDepth == doctest::Approx(depth).epsilon(0.01));

                    uint32_t cascade = 0;
                    while (cascade < cascades.Count && viewDepth > cascades.SplitDepths[cascade])
                        ++cascade;
                    REQUIRE(cascade < cascades.Count);

                    const HM::Vector3 light = Transform(cascades.ViewProj[cascade], world);
                    CHECK(std::abs(light.x()) <= 1.0f);
                    CHECK(std::abs(light.y()) <= 1.0f);
                    CHECK(light.z() >= 0.0f);
                    CHECK(light.z() <= 1.0f);

                    // A caster a few metres towards the sun still lands in the depth range, nearer the light.
                    const HM::Vector3 caster = Transform(cascades.ViewProj[cascade], world + *frame.ShadowLightDirection * 2.0f);
                    CHECK(caster.z() >= 0.0f);
                    CHECK(caster.z() < light.z());
                }
            }
        }
    }
}

TEST_CASE("Shadow uniform - the shadowed light is the first caster, when it is directional and visible")
{
    HX::RenderLight sun;
    sun.Type        = HX::LightType::Directional;
    sun.CastShadows = true;
    HX::RenderLight lamp;
    lamp.Type = HX::LightType::Point;

    HX::RenderLight noneCast[2] = { lamp, lamp };
    CHECK(FindShadowedLight(noneCast) == -1);
    HX::RenderLight sunSecond[3] = { lamp, sun, sun };
    CHECK(FindShadowedLight(sunSecond) == 1);
    HX::RenderLight castingLamp = lamp;
    castingLamp.CastShadows     = true;
    HX::RenderLight lampFirst[2] = { castingLamp, sun };
    CHECK(FindShadowedLight(lampFirst) == -1); // the sun's cascades follow the first caster's direction

    std::vector<HX::RenderLight> many(HedgehogEngine::MAX_LIGHTS_COUNT + 1, lamp);
    many.back() = sun; // past the lights the shader sees
    CHECK(FindShadowedLight(many) == -1);
}

TEST_CASE("The Shadow pass and every forward pass read the same cascades from the shared phase")
{
    PassBuilderRegistry registry;
    RegisterEnginePassTypes(registry);

    HX::RenderLight lights[2];
    lights[1].Type        = HX::LightType::Directional;
    lights[1].CastShadows = true;

    TestBuffer        positions(1024);
    TestBuffer        texCoords(1024);
    TestBuffer        normals(1024);
    TestBuffer        tangents(1024);
    TestBuffer        indices(1024);
    const MeshDrawRange meshes[] = { { 0, 36, 0 } };
    const HX::RenderInstance casters[] = { Instance(0) };

    GraphFrameData shadowView  = MakeCameraFrame(3);
    shadowView.OpaqueInstances = casters;
    shadowView.Meshes          = meshes;
    shadowView.Positions       = &positions;
    shadowView.Indices         = &indices;

    SharedPhaseSettings settings;
    settings.ShadowAtlasSize = 1024;
    settings.Sampling        = { 0.002f, 0.01f, 1.5f, 2, 0.2f };

    TestDevice         device;
    RenderGraphRuntime graph(device, 256 * 1024);
    FakeServices       services;
    SharedPhase        shared(registry);
    const SharedPhaseOutputs outputs = shared.Declare(graph, services, &shadowView, lights, settings);
    REQUIRE(outputs.Shadow != nullptr);
    CHECK(outputs.Shadow->CascadeCount == 3);
    CHECK(outputs.Shadow->LightIndex == 1);
    CHECK(outputs.Shadow->PcfRadius == 2);
    CHECK(outputs.Shadow->DepthBias == 0.002f);

    // A view's forward pass, reading the atlas the shared phase declared.
    FakeDescriptorSet sceneLights;
    GraphFrameData    view = MakeCameraFrame(3);
    view.SceneLights       = outputs.SceneLights;
    view.Shadow            = outputs.Shadow;
    view.Meshes            = meshes;
    view.Positions         = &positions;
    view.TexCoords         = &texCoords;
    view.Normals           = &normals;
    view.Tangents          = &tangents;
    view.Indices           = &indices;
    GraphFrameContext viewContext{ &services, &view };
    graph.SetFrameContext(&viewContext);

    const RGSizePolicy size = RGSizePolicy::MakeAbsolute(64, 64);
    PassInvocation     prepass("DepthPrepass");
    prepass.SetSlot("depth", graph.CreateTexture({ "depth", RHI::Format::D32Float, size,
                                                   RHI::TextureUsage::DepthStencil | RHI::TextureUsage::Sampled }));
    registry.Find("DepthPrepass")->Build(graph, prepass);
    PassInvocation forward("Forward");
    forward.SetSlot("color", graph.CreateTexture({ "color", RHI::Format::R16G16B16A16Float, size,
                                                   RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled }));
    forward.SetSlot("depth", prepass.GetSlot("depth"));
    forward.SetSlot("shadowMap", outputs.Imports.at(SHADOW_ATLAS_IMPORT));
    registry.Find("Forward")->Build(graph, forward);
    graph.BindOutput(graph.AddOutputSlot("color", RHI::Format::R16G16B16A16Float, size), forward.GetSlot("color"));

    RecordingCommandList cmd;
    REQUIRE(graph.Execute(cmd));

    // The Shadow pass rendered each cascade with the matrices the uniform carries...
    REQUIRE(services.UploadedFirstElements.size() >= 3);
    for (uint32_t i = 0; i < 3; ++i)
        CHECK(services.UploadedFirstElements[i] == outputs.Shadow->ViewProj[i].GetBuffer()[0]);
    // ...and the forward pass bound exactly that uniform, beside the atlas the Shadow pass wrote.
    REQUIRE(services.LightingUploads.size() == 1);
    CHECK(std::memcmp(&services.LightingUploads[0], outputs.Shadow, sizeof(ShadowUniform)) == 0);
    const RHI::IRHITexture* atlas = nullptr;
    for (const RHI::RenderingInfo& rendering : cmd.Renderings)
        if (rendering.DepthAttachment && rendering.Width == 1024)
            atlas = rendering.DepthAttachment->Texture;
    REQUIRE(atlas != nullptr);
    CHECK(services.LightingAtlases[0] == atlas);

    // Without a shadow view there is no atlas, and no shadow uniform.
    RenderGraphRuntime other(device, 256 * 1024);
    SharedPhase        none(registry);
    CHECK(none.Declare(other, services, nullptr, lights, settings).Shadow == nullptr);
}
