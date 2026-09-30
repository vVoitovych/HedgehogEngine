#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include <algorithm>
#include <string>
#include <vector>

using namespace HedgehogEngine;
using namespace ScriptingTest;
using HedgehogScripting::ScriptInstance;

namespace
{
    // What the renderer would get this frame.
    HX::RenderScene Extract(EngineWorld& world)
    {
        HX::RenderScene scene;
        HX::SceneExtractor().Extract(world.Ecs(), *world.Context.GetRenderSystem(), *world.Context.GetLightSystem(),
                                     *world.Context.GetCameraSystem(), scene);
        return scene;
    }

    template<typename Rendered>
    const Rendered* FindBySource(const std::vector<Rendered>& items, ECS::Entity entity)
    {
        const auto it = std::find_if(items.begin(), items.end(),
                                     [entity](const Rendered& item) { return item.SourceId == entity; });
        return it == items.end() ? nullptr : &*it;
    }

    std::string LastError(EngineWorld& world, ECS::Entity entity)
    {
        const ScriptInstance* instance = world.Scripts->FindInstance(entity);
        return instance != nullptr ? instance->GetLastError() : "no instance";
    }
}

TEST_CASE("Component API - a script writes every light field and the extractor sees it the same frame")
{
    EngineWorld       world;
    const ECS::Entity lamp = world.AddScripted("Lamp", "Dimmer", R"lua(
function Dimmer:OnUpdate(dt)
    local light = self.entity:getLight()
    Record(tostring(light.intensity))
    light.enable      = true
    light.lightType   = LightType.SpotLight
    light.color       = Vector3(1, 0.5, 0.25)
    light.intensity   = 2.5
    light.radius      = 12
    light.coneAngle   = 45
    light.castShadows = true
    assert(light.lightType == LightType.SpotLight)
    assert(light.color == Vector3(1, 0.5, 0.25))
    assert(light.entity == self.entity)
    Record(tostring(light))
end
)lua");
    world.Ecs().AddComponent(lamp, LightComponent{});

    world.Sim().Play();
    world.Sim().Tick(STEP); // the script writes...
    world.Context.GetLightSystem()->Update(world.Ecs());
    const HX::RenderScene scene = Extract(world); // ...and this frame's extraction sees it

    CHECK(LastError(world, lamp).empty());
    REQUIRE(world.Log.size() == 2u);
    CHECK(world.Log[0] == "1.0");
    CHECK(world.Log[1] == "LightComponent of " + std::string("Entity ") + std::to_string(lamp) + " (generation " +
                             std::to_string(world.Ecs().GetGeneration(lamp)) + ")");

    const LightComponent& light = world.Ecs().GetComponent<LightComponent>(lamp);
    CHECK(light.Enable);
    CHECK(light.LightType == LightType::SpotLight);
    CHECK(Near(light.Color, HM::Vector3(1.0f, 0.5f, 0.25f)));
    CHECK(light.Intensity == 2.5f);
    CHECK(light.Radius == 12.0f);
    CHECK(light.ConeAngle == 45.0f);
    CHECK(light.CastShadows);

    const HX::RenderLight* rendered = FindBySource(scene.Lights, lamp);
    REQUIRE(rendered != nullptr);
    CHECK(rendered->Type == HX::LightType::Spot);
    CHECK(rendered->Intensity == 2.5f);
    CHECK(rendered->Radius == 12.0f);
    CHECK(rendered->ConeAngle == 45.0f);
    CHECK(rendered->CastShadows);
    world.Sim().Stop();
}

TEST_CASE("Component API - a script writes every camera field and the extractor sees it the same frame")
{
    EngineWorld       world;
    const ECS::Entity eye = world.AddScripted("Eye", "Zoomer", R"lua(
function Zoomer:OnUpdate(dt)
    local camera = self.entity:getCamera()
    Record(tostring(camera.fov))
    camera.isEnabled      = true
    camera.projectionType = CameraProjectionType.Orthographic
    camera.fov            = 30
    camera.orthoSize      = 8
    camera.nearPlane      = 0.5
    camera.farPlane       = 250
    camera.layerMask      = 5
    camera.graphName      = "scene"
    camera.priority       = -3
    assert(camera.projectionType == CameraProjectionType.Orthographic)
    assert(camera.graphName == "scene" and camera.layerMask == 5 and camera.priority == -3)
end
)lua");
    world.Ecs().AddComponent(eye, CameraComponent{});

    world.Sim().Play();
    world.Sim().Tick(STEP);
    const HX::RenderScene scene = Extract(world);

    CHECK(LastError(world, eye).empty());
    CHECK(world.Log == std::vector<std::string>{ "60.0" });

    const HX::RenderCamera* rendered = FindBySource(scene.Cameras, eye);
    REQUIRE(rendered != nullptr);
    CHECK(rendered->ProjectionType == HX::CameraProjectionType::Orthographic);
    CHECK(rendered->Fov == 30.0f);
    CHECK(rendered->OrthoSize == 8.0f);
    CHECK(rendered->NearPlane == 0.5f);
    CHECK(rendered->FarPlane == 250.0f);
    CHECK(rendered->LayerMask == 5u);
    CHECK(rendered->GraphName == "scene");
    CHECK(rendered->Priority == -3);
    CHECK(world.Ecs().GetComponent<CameraComponent>(eye).IsEnabled);
    world.Sim().Stop();
}

TEST_CASE("Component API - a new meshPath goes through MeshSystem; a missing file is reverted")
{
    EngineWorld world;
    world.Dir.WriteFile("Models/Test/box.obj", "v 0 0 0\n");
    const ECS::Entity body = world.AddScripted("Body", "Reshaper", R"lua(
function Reshaper:OnStart()
    local mesh = self.entity:getMesh()
    Record(mesh.meshPath)
    mesh.meshPath = "Models/Test/box.obj"
    Record(mesh.meshPath)
    mesh.meshPath = "Models/Test/missing.obj"
    Record(mesh.meshPath)
end
)lua");
    MeshComponent mesh;
    mesh.MeshPath = MeshSystem::sDefaultMeshPath;
    world.Ecs().AddComponent(body, mesh);
    world.Context.GetMeshSystem()->Update(world.Ecs(), body, world.ScriptFiles);
    world.Context.GetMeshSystem()->MeshContainerUpdated();

    LogCapture capture;
    world.Sim().Play();
    world.Sim().Tick(STEP);

    const std::vector<std::string> expected{ MeshSystem::sDefaultMeshPath, "Models/Test/box.obj", "Models/Test/box.obj" };
    CHECK(world.Log == expected);
    const MeshComponent& result = world.Ecs().GetComponent<MeshComponent>(body);
    REQUIRE(result.MeshIndex.has_value());
    const std::vector<std::string>& meshes = world.Context.GetMeshSystem()->GetMeshes();
    CHECK(meshes.at(*result.MeshIndex) == "Models/Test/box.obj");
    CHECK(world.Context.GetMeshSystem()->ShouldUpdateMeshContainer()); // the catalog loads it this frame
    CHECK(capture.CountLines("Wrong file path:  Models/Test/missing.obj") == 1);
    world.Sim().Stop();
}

TEST_CASE("Component API - get returns nil when absent; has and add")
{
    EngineWorld       world;
    const ECS::Entity bare = world.AddScripted("Bare", "Builder", R"lua(
function Builder:OnStart()
    local e = self.entity
    Record(tostring(e:getLight() == nil) .. " " .. tostring(e:getCamera() == nil) .. " " .. tostring(e:getMesh() == nil))
    Record(tostring(e:hasLight()) .. " " .. tostring(e:hasCamera()) .. " " .. tostring(e:hasMesh()))
    local light = e:addLight()
    light.intensity = 3
    assert(e:addLight().intensity == 3, "adding again returns the existing light")
    e:addCamera().fov = 50
    Record(e:addMesh().meshPath)
    Record(tostring(e:hasLight()) .. " " .. tostring(e:hasCamera()) .. " " .. tostring(e:hasMesh()))
end
)lua");

    world.Sim().Play();
    world.Sim().Tick(STEP);

    CHECK(LastError(world, bare).empty());
    const std::vector<std::string> expected{ "true true true", "false false false", MeshSystem::sDefaultMeshPath,
                                             "true true true" };
    CHECK(world.Log == expected);
    CHECK(world.Ecs().GetComponent<LightComponent>(bare).Intensity == 3.0f);
    CHECK(world.Ecs().GetComponent<CameraComponent>(bare).Fov == 50.0f);
    CHECK(world.Ecs().GetComponent<MeshComponent>(bare).MeshIndex.has_value());

    // Stop restores the scene as it was: the added components go.
    world.Sim().Stop();
    CHECK_FALSE(world.Ecs().HasComponent<LightComponent>(bare));
}

TEST_CASE("Component API - a proxy for a removed component raises an error naming the component and entity")
{
    EngineWorld       world;
    const ECS::Entity lamp = world.AddScripted("Lamp", "Holder", R"lua(
function Holder:OnStart()
    light = self.entity:getLight()
end
function Holder:OnUpdate(dt)
    local ok, message = pcall(function() return light.intensity end)
    Record(tostring(ok))
    Record(tostring(message))
    local _ = light.intensity -- unprotected: faults this instance
end
)lua");
    world.Ecs().AddComponent(lamp, LightComponent{});

    world.Sim().Play();
    world.Sim().Tick(STEP);
    REQUIRE(world.Log.size() == 2u);
    CHECK(world.Log[0] == "true");
    world.Log.clear();

    world.Ecs().RemoveComponent<LightComponent>(lamp);
    LogCapture capture;
    world.Sim().Tick(STEP);

    const std::string expected = "LightComponent of Entity " + std::to_string(lamp) + " (generation " +
                                 std::to_string(world.Ecs().GetGeneration(lamp)) + ") no longer exists";
    REQUIRE(world.Log.size() == 2u);
    CHECK(world.Log[0] == "false");
    CHECK(world.Log[1].find(expected) != std::string::npos);
    CHECK(LastError(world, lamp).find(expected) != std::string::npos);
    CHECK(LastError(world, lamp).find("stack traceback") != std::string::npos);
    world.Sim().Stop();
}

TEST_CASE("Component API - an enum value out of range is a script error")
{
    EngineWorld       world;
    const ECS::Entity lamp = world.AddScripted("Lamp", "Breaker", R"lua(
function Breaker:OnStart()
    local light = self.entity:addLight()
    local ok, message = pcall(function() light.lightType = 7 end)
    Record(tostring(ok))
    Record(tostring(message))
    Record(tostring(light.lightType == LightType.DirectionLight))
    ok = pcall(function() self.entity:addCamera().projectionType = -1 end)
    Record(tostring(ok))
end
)lua");

    LogCapture capture; // the rejected writes are logged by sol2 in Debug
    world.Sim().Play();
    world.Sim().Tick(STEP);

    REQUIRE(world.Log.size() == 4u);
    CHECK(world.Log[0] == "false");
    CHECK(world.Log[1].find("7 is not a valid enum value") != std::string::npos);
    CHECK(world.Log[2] == "true");
    CHECK(world.Log[3] == "false");
    CHECK(world.Ecs().GetComponent<LightComponent>(lamp).LightType == LightType::DirectionLight);
    world.Sim().Stop();
}
