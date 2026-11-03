#include "HedgehogRenderer/Graph/EnginePassTypes.hpp"
#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/RenderGraphRuntime.hpp"
#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogExtract/api/CameraMath.hpp"

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

    struct TerrainScene
    {
        TestBuffer        Positions{ 1024 };
        TestBuffer        TexCoords{ 1024 };
        TestBuffer        Normals{ 1024 };
        TestBuffer        Tangents{ 1024 };
        TestBuffer        Indices{ 1024 };
        FakeDescriptorSet Material;
        FakeDescriptorSet SceneLights;
        FakeDescriptorSet Hills;
        FakeDescriptorSet Dunes;

        const RHI::IRHIDescriptorSet* Materials[1]   = { &Material };
        // Terrain 1 has no set (past the registry's budget).
        const RHI::IRHIDescriptorSet* TerrainSets[3] = { &Hills, nullptr, &Dunes };
        const MeshDrawRange           Meshes[2]      = { { 0, 36, 0 }, { 36, 6144, 24 } };

        GraphFrameData MakeFrame()
        {
            GraphFrameData frame = RGTest::MakeFrame(1);
            frame.SceneLights    = &SceneLights;
            frame.Meshes         = Meshes;
            frame.MaterialSets   = Materials;
            frame.TerrainSets    = TerrainSets;
            frame.Positions      = &Positions;
            frame.TexCoords      = &TexCoords;
            frame.Normals        = &Normals;
            frame.Tangents       = &Tangents;
            frame.Indices        = &Indices;
            return frame;
        }
    };

    HX::RenderInstance Terrain(int32_t terrain)
    {
        HX::RenderInstance instance = Instance(1);
        instance.MaterialIndex      = UINT64_MAX;
        instance.TerrainIndex       = terrain;
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

    HM::Matrix4x4 ViewProj()
    {
        HX::RenderCamera camera;
        camera.WorldMatrix = HM::Matrix4x4::GetIdentity();
        camera.Fov         = 90.0f;
        camera.NearPlane   = 0.1f;
        camera.FarPlane    = 100.0f;
        return HX::MakeProjection(camera, 1.0f) * HX::MakeViewMatrix(camera);
    }

    HX::RenderInstance At(uint64_t sourceId, int32_t terrain, bool casts = true)
    {
        HX::RenderInstance instance;
        instance.WorldBounds  = HM::AABB(HM::Vector3(-1.0f, -1.0f, -6.0f), HM::Vector3(1.0f, 1.0f, -4.0f));
        instance.SourceId     = sourceId;
        instance.TerrainIndex = terrain;
        instance.CastsShadows = casts;
        if (terrain >= 0)
            instance.MaterialIndex = UINT64_MAX;
        return instance;
    }
}

TEST_CASE("Terrain tiles are a view's terrain instances, never opaque ones, and cast unless told not to")
{
    const HX::RenderInstance instances[] = { At(1, -1), At(2, 0), At(3, 2, false), At(4, -1, false) };

    ViewInstances view;
    CullViewInstances(instances, 0xFFFFFFFFu, ViewProj(), view);
    REQUIRE(view.Opaque.size() == 2);
    CHECK(view.Opaque[0].SourceId == 1);
    CHECK(view.Opaque[1].SourceId == 4);
    REQUIRE(view.Terrain.size() == 2);
    CHECK(view.Terrain[0].SourceId == 2);
    CHECK(view.Terrain[1].SourceId == 3);

    std::vector<HX::RenderInstance> casters;
    CollectSceneInstances(instances, casters);
    REQUIRE(casters.size() == 2);
    CHECK(casters[0].SourceId == 1);
    CHECK(casters[1].SourceId == 2);

    // Re-culling empties the list.
    CullViewInstances({}, 0xFFFFFFFFu, ViewProj(), view);
    CHECK(view.Terrain.empty());
}

TEST_CASE("Terrain tiles are depth-tested as opaque geometry and lit with the terrain pipeline and their own set")
{
    TerrainScene             scene;
    const HX::RenderInstance opaque[]   = { Instance(0) };
    const HX::RenderInstance terrains[] = { Terrain(0), Terrain(0), Terrain(1), Terrain(2) };

    GraphFrameData frame   = scene.MakeFrame();
    frame.OpaqueInstances  = opaque;
    frame.TerrainInstances = terrains;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);

    // The prepass: the opaque instance, then every terrain tile with the same pipeline and set 0.
    const CommandList prepass = { "begin", "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36",
                                  "vertex 1", "index", "push 64", "draw 6144", "push 64", "draw 6144", "push 64", "draw 6144",
                                  "push 64", "draw 6144" };
    CHECK(Rendering(cmd, 0) == prepass);

    // The forward pass: the opaque instance with its material, then the terrain pipeline with sets 0,
    // 2 and 3 again and each terrain's set when it changes; the tile without a set is skipped.
    const CommandList forward = { "begin", "pipeline", "vertex 4", "index", "set 0", "set 2", "set 3",
                                  "set 1", "push 64", "draw 36",
                                  "pipeline", "set 0", "set 2", "set 3",
                                  "set 1", "push 64", "draw 6144", "push 64", "draw 6144",
                                  "set 1", "push 64", "draw 6144" };
    CHECK(Rendering(cmd, 2) == forward);
    CHECK(std::count(cmd.BoundPipelines.begin(), cmd.BoundPipelines.end(),
                     &services.GetPipeline(EnginePipeline::ForwardTerrain)) == 1);
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Hills) == 1);
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Dunes) == 1);
    CHECK(std::count(cmd.BoundSets.begin(), cmd.BoundSets.end(), &scene.Material) == 1);
}

TEST_CASE("Without terrain tiles the passes record what they did before")
{
    TerrainScene             scene;
    const HX::RenderInstance opaque[] = { Instance(0) };
    GraphFrameData           frame    = scene.MakeFrame();
    frame.OpaqueInstances             = opaque;

    FakeServices         services;
    RecordingCommandList cmd;
    Record(frame, services, cmd);
    CHECK(Rendering(cmd, 0) == CommandList{ "begin", "pipeline", "set 0", "vertex 1", "index", "push 64", "draw 36" });
    CHECK(Rendering(cmd, 2) == CommandList{ "begin", "pipeline", "vertex 4", "index", "set 0", "set 2", "set 3", "set 1",
                                            "push 64", "draw 36" });
    CHECK(std::count(cmd.BoundPipelines.begin(), cmd.BoundPipelines.end(),
                     &services.GetPipeline(EnginePipeline::ForwardTerrain)) == 0);
}
