#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace HedgehogEngine;
using HedgehogScripting::ScriptSystem;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    constexpr const char* BASE_SCRIPT = R"lua(ActorScript = {}
ActorScript.__index = ActorScript
function ActorScript:new() return setmetatable({}, ActorScript) end
)lua";

    // Records every callback through the host's Record(name, event).
    constexpr const char* LOGGER_SCRIPT = R"lua(Logger = setmetatable({}, { __index = ActorScript })
Logger.__index = Logger
function Logger:new() return setmetatable(ActorScript:new(), Logger) end
function Logger:OnStart() Record("OnStart") end
function Logger:OnEnable() Record("OnEnable") end
function Logger:OnDisable() Record("OnDisable") end
function Logger:OnUpdate(dt) Record("OnUpdate") end
function Logger:OnDestroy() Record("OnDestroy") end
)lua";

    constexpr const char* FAULTY_SCRIPT = R"lua(Faulty = setmetatable({}, { __index = ActorScript })
Faulty.__index = Faulty
function Faulty:new() return setmetatable(ActorScript:new(), Faulty) end
function Faulty:OnUpdate(dt) error("always broken") end
)lua";

    // The shipped PlayerScript.lua with speed = 2.
    std::string SpinnerScript()
    {
        std::ifstream file(FS::GetEngineRootDirectory() / "Assets" / "Scripts" / "PlayerScript.lua");
        std::stringstream text;
        text << file.rdbuf();
        std::string source = text.str();
        REQUIRE(source.find("speed = 1.0") != std::string::npos);
        source.replace(source.find("speed = 1.0"), 11, "speed = 2");
        // The class is found by file stem, so the copy keeps the PlayerScript name.
        return source;
    }

    // An ECS, a scene, a Simulation and the ScriptSystem, over a temp "assets://".
    struct World
    {
        TempDir                                       Dir;
        FS::FileSystemManager                         FileSystem;
        ECS::ECS                                      Ecs;
        EventBus                                      Bus;
        EcsSerialization::ComponentSerializerRegistry Registry;
        std::shared_ptr<TransformSystem>              Transforms;
        std::shared_ptr<MeshSystem>                   Meshes;
        std::shared_ptr<RenderSystem>                 Renders;
        std::unique_ptr<SceneManager>                 Scenes;
        std::unique_ptr<Simulation>                   Sim;
        std::shared_ptr<ScriptSystem>                Scripts;
        std::vector<std::string>                      Log;

        World()
        {
            Dir.WriteFile("Scripts/Base/ActorScript.lua", BASE_SCRIPT);
            Dir.WriteFile("Scripts/Logger.lua", LOGGER_SCRIPT);
            Dir.WriteFile("Scripts/Faulty.lua", FAULTY_SCRIPT);
            Dir.WriteFile("Scripts/Spin/PlayerScript.lua", SpinnerScript());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(FileSystem.Register(std::move(fs)));

            Ecs.Init();
            Ecs.RegisterComponent<TransformComponent>();
            Ecs.RegisterComponent<ECS::HierarchyComponent>();
            Ecs.RegisterComponent<MeshComponent>();
            Ecs.RegisterComponent<RenderComponent>();
            Ecs.RegisterComponent<ScriptComponent>();
            Transforms = Ecs.RegisterSystem<TransformSystem>();
            Meshes     = Ecs.RegisterSystem<MeshSystem>();
            Renders    = Ecs.RegisterSystem<RenderSystem>();
            SetSignature<TransformSystem, TransformComponent>();
            SetSignature<MeshSystem, MeshComponent>();
            SetSignature<RenderSystem, RenderComponent>();

            Registry.RegisterReflected<TransformComponent>("TransformComponent");
            Registry.RegisterVisitable<ScriptComponent>("ScriptComponent");

            Scenes  = std::make_unique<SceneManager>(Ecs, Bus, FileSystem, Registry, *Transforms, *Meshes, *Renders);
            Sim     = std::make_unique<Simulation>(Ecs, *Scenes);
            Scripts = ScriptSystem::Register(Ecs, Bus, FileSystem);

            Scripts->GetVM().GetState().set_function("Record", [this](const std::string& event) { Log.push_back(event); });
        }

        template<typename System, typename Component>
        void SetSignature()
        {
            ECS::Signature signature;
            signature.set(Ecs.GetComponentType<Component>());
            Ecs.SetSystemSignature<System>(signature);
        }

        ECS::Entity AddScripted(const std::string& scriptPath)
        {
            const ECS::Entity entity = Scenes->CreateGameObject();
            ScriptComponent   script;
            script.ScriptPath = scriptPath;
            Ecs.AddComponent(entity, script);
            return entity;
        }

        ScriptComponent& Script(ECS::Entity entity) { return Ecs.GetComponent<ScriptComponent>(entity); }

        std::vector<std::string> TakeLog()
        {
            std::vector<std::string> taken;
            taken.swap(Log);
            return taken;
        }
    };

    using Events = std::vector<std::string>;
}

TEST_CASE("ScriptSystem - OnStart, OnEnable, then OnUpdate every played frame")
{
    World world;
    world.AddScripted("Scripts/Logger.lua");

    world.Sim->Play();
    CHECK(world.TakeLog().empty()); // made on Play, started on the first Update
    for (int i = 0; i < 3; ++i)
        world.Sim->Tick(STEP);

    CHECK(world.TakeLog() == Events{ "OnStart", "OnEnable", "OnUpdate", "OnUpdate", "OnUpdate" });
}

TEST_CASE("ScriptSystem - nothing runs in Edit or while paused")
{
    World world;
    world.AddScripted("Scripts/Logger.lua");

    world.Sim->Tick(STEP);
    CHECK(world.TakeLog().empty());
    CHECK(world.Scripts->GetInstanceCount() == 0u);

    world.Sim->Play();
    world.Sim->Tick(STEP);
    (void)world.TakeLog();

    world.Sim->Pause();
    for (int i = 0; i < 5; ++i)
        world.Sim->Tick(STEP);
    CHECK(world.TakeLog().empty());

    world.Sim->Resume();
    world.Sim->Tick(STEP);
    CHECK(world.TakeLog() == Events{ "OnUpdate" });
}

TEST_CASE("ScriptSystem - toggling Enable gives OnDisable, then OnEnable")
{
    World world;
    const ECS::Entity entity = world.AddScripted("Scripts/Logger.lua");
    world.Sim->Play();
    world.Sim->Tick(STEP);
    (void)world.TakeLog();

    world.Script(entity).NewEnable = false;
    world.Sim->Tick(STEP);
    world.Sim->Tick(STEP); // disabled: no OnUpdate
    CHECK(world.TakeLog() == Events{ "OnDisable" });
    CHECK_FALSE(world.Script(entity).Enable);

    world.Script(entity).NewEnable = true;
    world.Sim->Tick(STEP);
    CHECK(world.TakeLog() == Events{ "OnEnable", "OnUpdate" }); // OnStart only ever once
}

TEST_CASE("ScriptSystem - OnDestroy on component removal, entity destruction and Stop")
{
    World world;
    const ECS::Entity removed   = world.AddScripted("Scripts/Logger.lua");
    const ECS::Entity destroyed = world.AddScripted("Scripts/Logger.lua");
    const ECS::Entity stopped   = world.AddScripted("Scripts/Logger.lua");
    world.Sim->Play();
    world.Sim->Tick(STEP);
    (void)world.TakeLog();
    REQUIRE(world.Scripts->GetInstanceCount() == 3u);

    world.Ecs.RemoveComponent<ScriptComponent>(removed);
    CHECK(world.TakeLog() == Events{ "OnDestroy" });

    world.Scenes->DeleteGameObject(destroyed);
    CHECK(world.TakeLog() == Events{ "OnDestroy" });

    world.Sim->Stop();
    CHECK(world.TakeLog() == Events{ "OnDestroy" });
    CHECK(world.Scripts->GetInstanceCount() == 0u);
    (void)stopped;
}

TEST_CASE("ScriptSystem - a script added during Play starts on the next Update")
{
    World world;
    world.Sim->Play();
    world.Sim->Tick(STEP);
    CHECK(world.TakeLog().empty());

    world.AddScripted("Scripts/Logger.lua");
    world.Sim->Tick(STEP);
    CHECK(world.TakeLog() == Events{ "OnStart", "OnEnable", "OnUpdate" });
}

TEST_CASE("ScriptSystem - PlayerScript with speed 2 turns its entity by 2*dt per Update")
{
    World world;
    const ECS::Entity spinner = world.AddScripted("Scripts\\Spin\\PlayerScript.lua"); // Default.yaml's separator
    int changedEvents = 0;
    world.Bus.Subscribe<TransformChangedEvent>([&](const TransformChangedEvent& e) { changedEvents += (e.entity == spinner); });

    world.Sim->Play();
    for (int i = 0; i < 3; ++i)
        world.Sim->Tick(STEP);

    CHECK(world.Ecs.GetComponent<TransformComponent>(spinner).Rotation.z() == doctest::Approx(3.0 * 2.0 * STEP));
    CHECK(changedEvents == 3);

    // Stop puts the rotation back.
    world.Sim->Stop();
    CHECK(world.Ecs.GetComponent<TransformComponent>(spinner).Rotation.z() == 0.0f);
}

TEST_CASE("ScriptSystem - component Params override the script's globals")
{
    World world;
    const ECS::Entity spinner = world.AddScripted("Scripts/Spin/PlayerScript.lua");
    world.Script(spinner).Params["speed"] = { ParamType::Number, 5.0f, false };

    world.Sim->Play();
    world.Sim->Tick(STEP);
    CHECK(world.Ecs.GetComponent<TransformComponent>(spinner).Rotation.z() == doctest::Approx(5.0 * STEP));

    // A dirty param reaches the running instance before its next OnUpdate.
    world.Script(spinner).Params["clockWise"] = { ParamType::Boolean, false, true };
    world.Sim->Tick(STEP);
    CHECK(world.Ecs.GetComponent<TransformComponent>(spinner).Rotation.z() == doctest::Approx(0.0).epsilon(1e-4));
}

TEST_CASE("ScriptSystem - a faulted instance does not stop the others")
{
    World world;
    const ECS::Entity faulty = world.AddScripted("Scripts/Faulty.lua");
    world.AddScripted("Scripts/Logger.lua");

    world.Sim->Play();
    for (int i = 0; i < 3; ++i)
        world.Sim->Tick(STEP);

    REQUIRE(world.Scripts->FindInstance(faulty) != nullptr);
    CHECK(world.Scripts->FindInstance(faulty)->IsFaulted());
    CHECK(world.TakeLog() == Events{ "OnStart", "OnEnable", "OnUpdate", "OnUpdate", "OnUpdate" });
}

TEST_CASE("ScriptSystem - Play, Stop and Play again runs fresh instances")
{
    World world;
    world.AddScripted("Scripts/Logger.lua");

    world.Sim->Play();
    world.Sim->Tick(STEP);
    world.Sim->Stop();
    (void)world.TakeLog();

    world.Sim->Play();
    world.Sim->Tick(STEP);
    CHECK(world.TakeLog() == Events{ "OnStart", "OnEnable", "OnUpdate" });
}

TEST_CASE("ScriptSystem - DescribeScript lists a script's parameters without an instance")
{
    World world;
    const auto params = world.Scripts->DescribeScript("Scripts/Spin/PlayerScript.lua");

    REQUIRE(params.size() == 2u);
    REQUIRE(params.count("speed") == 1u);
    CHECK(params.at("speed").type == ParamType::Number);
    CHECK(std::get<float>(params.at("speed").value) == 2.0f);
    REQUIRE(params.count("clockWise") == 1u);
    CHECK(params.at("clockWise").type == ParamType::Boolean);
    CHECK(std::get<bool>(params.at("clockWise").value));
    CHECK(world.Scripts->GetInstanceCount() == 0u);
}
