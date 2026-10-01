#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"

#include <string>

using HedgehogEngine::CameraComponent;
using HedgehogEngine::CameraProjectionType;
using HedgehogEngine::CameraTargetMode;
using HedgehogEngine::LightComponent;
using HedgehogEngine::LightType;
using HedgehogEngine::MeshComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }
}

TEST_CASE("Component bindings - a script reads and writes every Light field, straight into the ECS")
{
    EngineWorld world;
    world.WriteScript("Lamp.lua", Script("Lamp", R"lua(
    local light = self.entity:addLight()
    print("defaults " .. tostring(light.enabled) .. " " .. light.type .. " " .. light.intensity)
    light.enabled = false
    light.type = LightType.Spot
    light.color = Vector3(1, 0.5, 0.25)
    light.intensity = 2.5
    light.radius = 12
    light.coneAngle = 45
    print("type is spot " .. tostring(light.type == LightType.Spot))
)lua"));
    const ECS::Entity lamp = world.AddScripted("Scripts/Lamp.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("defaults true 0 1.0").size() == 1);
    CHECK(log.Lines("type is spot true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());

    REQUIRE(world.Ecs().HasComponent<LightComponent>(lamp));
    const auto& light = world.Ecs().GetComponent<LightComponent>(lamp);
    CHECK_FALSE(light.Enable);
    CHECK(light.LightType == LightType::SpotLight);
    CHECK(light.Color.y() == doctest::Approx(0.5f));
    CHECK(light.Intensity == doctest::Approx(2.5f));
    CHECK(light.Radius == doctest::Approx(12.0f));
    CHECK(light.ConeAngle == doctest::Approx(45.0f));

    // Added during Play, so Stop's restore takes it away again.
    REQUIRE(world.Stop());
    CHECK_FALSE(world.Ecs().HasComponent<LightComponent>(lamp));
}

TEST_CASE("Component bindings - castShadows goes through LightSystem, so only one light casts")
{
    EngineWorld world;
    world.WriteScript("Sun.lua", Script("Sun", "    self.entity:getLight().castShadows = true"));
    const ECS::Entity first  = world.AddScripted("Scripts/Sun.lua", false);
    const ECS::Entity second = world.AddScripted("Scripts/Sun.lua", false);
    world.Ecs().AddComponent(first, LightComponent{});
    world.Ecs().AddComponent(second, LightComponent{});
    world.Ecs().GetComponent<HedgehogEngine::ScriptComponent>(first).Enable = true;

    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Ecs().GetComponent<LightComponent>(first).CastShadows);
    CHECK_FALSE(world.Ecs().GetComponent<LightComponent>(second).CastShadows);

    world.Ecs().GetComponent<HedgehogEngine::ScriptComponent>(second).Enable = true;
    world.Frame(STEP);
    CHECK_FALSE(world.Ecs().GetComponent<LightComponent>(first).CastShadows);
    CHECK(world.Ecs().GetComponent<LightComponent>(second).CastShadows);
    REQUIRE(world.Stop());
}

TEST_CASE("Component bindings - getLight is nil without a light, and addLight returns the existing one")
{
    EngineWorld world;
    world.WriteScript("Probe.lua", Script("Probe", R"lua(
    local e = self.entity
    print("before " .. tostring(e:getLight() == nil) .. " " .. tostring(e:hasLight()))
    local a = e:addLight()
    a.intensity = 2
    local b = e:addLight()
    print("after " .. tostring(e:hasLight()) .. " " .. b.intensity .. " " .. e:getLight().intensity)
    print("text " .. tostring(b))
)lua"));
    const ECS::Entity probe = world.AddScripted("Scripts/Probe.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("before true false").size() == 1);
    CHECK(log.Lines("after true 2.0 2.0").size() == 1);
    CHECK(log.Lines("text Light of Entity " + std::to_string(probe)).size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Component bindings - a handle whose component was removed names the component and the entity")
{
    EngineWorld world;
    world.WriteScript("Holder.lua", Script("Holder", "    light = self.entity:getLight()", "    light.intensity = 3"));
    const ECS::Entity holder = world.AddScripted("Scripts/Holder.lua");
    world.Ecs().AddComponent(holder, LightComponent{});

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(world.Ecs().GetComponent<LightComponent>(holder).Intensity == doctest::Approx(3.0f));
    CHECK(log.Lines("[ERROR]").empty());

    world.Ecs().RemoveComponent<LightComponent>(holder);
    world.Frame(STEP);
    const auto errors = log.Lines("[ERROR][Script]");
    REQUIRE(errors.size() == 1);
    CHECK(log.Text().find("Entity " + std::to_string(holder) + " (generation") != std::string::npos);
    CHECK(log.Text().find("has no LightComponent") != std::string::npos);
    REQUIRE(world.Stop());
}

TEST_CASE("Component bindings - a script reads and writes every Camera field")
{
    EngineWorld world;
    world.WriteScript("Cam.lua", Script("Cam", R"lua(
    local cam = self.entity:addCamera()
    print("defaults " .. tostring(cam.enabled) .. " " .. cam.projection .. " " .. cam.fov .. " " .. cam.graphName)
    cam.enabled = false
    cam.projection = CameraProjectionType.Orthographic
    cam.fov = 75
    cam.orthoSize = 4
    cam.near = 0.5
    cam.far = 250
    cam.layerMask = 0xFFFFFFFF
    cam.targetMode = CameraTargetMode.Texture
    cam.targetName = "minimap"
    cam.graphName = "scene"
    cam.priority = -3
    print("mask " .. string.format("%x", cam.layerMask))
)lua"));
    const ECS::Entity cam = world.AddScripted("Scripts/Cam.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("defaults true 0 60.0 game").size() == 1);
    CHECK(log.Lines("mask ffffffff").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());

    const auto& camera = world.Ecs().GetComponent<CameraComponent>(cam);
    CHECK_FALSE(camera.IsEnabled);
    CHECK(camera.ProjectionType == CameraProjectionType::Orthographic);
    CHECK(camera.Fov == doctest::Approx(75.0f));
    CHECK(camera.OrthoSize == doctest::Approx(4.0f));
    CHECK(camera.NearPlane == doctest::Approx(0.5f));
    CHECK(camera.FarPlane == doctest::Approx(250.0f));
    CHECK(camera.LayerMask == 0xFFFFFFFFu);
    CHECK(camera.TargetMode == CameraTargetMode::Texture);
    CHECK(camera.TargetName == "minimap");
    CHECK(camera.GraphName == "scene");
    CHECK(camera.Priority == -3);
    REQUIRE(world.Stop());
}

TEST_CASE("Component bindings - an enum value or layer mask out of range is a script error")
{
    EngineWorld world;
    world.WriteScript("Bad.lua", Script("Bad", R"lua(
    local cam = self.entity:addCamera()
    local ok1 = pcall(function() cam.projection = 7 end)
    local ok2 = pcall(function() cam.layerMask = -1 end)
    local ok3 = pcall(function() self.entity:addLight().type = 3 end)
    print("accepted " .. tostring(ok1) .. " " .. tostring(ok2) .. " " .. tostring(ok3))
)lua"));
    const ECS::Entity bad = world.AddScripted("Scripts/Bad.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("accepted false false false").size() == 1);
    CHECK(world.Ecs().GetComponent<CameraComponent>(bad).ProjectionType == CameraProjectionType::Perspective);
    CHECK(world.Ecs().GetComponent<LightComponent>(bad).LightType == LightType::DirectionLight);
    REQUIRE(world.Stop());
}

TEST_CASE("Component bindings - setting mesh.path loads an existing mesh, and a missing file is an error")
{
    EngineWorld world;
    world.WriteScript("Shape.lua", Script("Shape", R"lua(
    local mesh = self.entity:addMesh()
    print("default " .. mesh.path)
    mesh.path = "assets://Models/viking_room.obj"
    print("loaded " .. mesh.path)
    local ok = pcall(function() mesh.path = "Models/missing.obj" end)
    print("missing accepted " .. tostring(ok) .. ", still " .. mesh.path)
)lua"));
    const ECS::Entity shape = world.AddScripted("Scripts/Shape.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("default " + HedgehogEngine::MeshSystem::sDefaultMeshPath).size() == 1);
    CHECK(log.Lines("loaded Models/viking_room.obj").size() == 1);
    CHECK(log.Lines("missing accepted false, still Models/viking_room.obj").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());

    const auto& mesh = world.Ecs().GetComponent<MeshComponent>(shape);
    CHECK(mesh.MeshPath == "Models/viking_room.obj");
    REQUIRE(mesh.MeshIndex.has_value());
    CHECK(mesh.CachedMeshPath == "Models/viking_room.obj");
    REQUIRE(world.Stop());
}
