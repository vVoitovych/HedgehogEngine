#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/NavMeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"
#include "HedgehogEngine/api/Navigation/NavigationGeometry.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Text += line + '\n'; }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        size_t Count(const std::string& fragment) const
        {
            size_t count = 0;
            for (size_t at = m_Text.find(fragment); at != std::string::npos; at = m_Text.find(fragment, at + 1))
                ++count;
            return count;
        }

    private:
        std::string m_Text;
        int         m_Sink;
    };

    // A context with nav:// on a temp folder, and a way to bake a 20 x 20 m floor centred at x into it.
    struct NavigationWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();

        NavigationWorld()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("nav://", Dir.Path());
            Context.GetFileSystem().Register(std::move(fs));
        }

        // Bakes a floor (a static box collider, its top at z 0) into nav://<file>, then removes it.
        void BakeFloor(const std::string& file, float x)
        {
            const ECS::Entity floor     = Scenes.CreateGameObject();
            auto&             transform = Ecs.GetComponent<TransformComponent>(floor);
            transform.Position          = HM::Vector3(x, 0.0f, -0.5f);
            transform.Scale             = HM::Vector3(10.0f, 10.0f, 0.5f); // the collider's 2 m box times this
            Context.GetEventBus().Publish(TransformChangedEvent{ floor });
            Ecs.AddComponent(floor, ColliderComponent{});
            Context.UpdateContext(1.0f, FRAME);

            const HN::NavMeshBuildSettings settings = MakeNavMeshBuildSettings(NavMeshComponent{});
            const HN::NavMeshBakeResult    baked    = HN::BakeNavMesh(GatherNavigationGeometry(Ecs, Context.GetResourceCatalog()), settings);
            REQUIRE(baked.Error.empty());
            const std::vector<uint8_t> bytes = WriteNavMeshFile(settings, baked.Data);
            Dir.WriteFile(file, std::string(bytes.begin(), bytes.end()));
            Scenes.DeleteGameObject(floor);
        }

        ECS::Entity NavMesh(const std::string& path)
        {
            const ECS::Entity entity = Scenes.CreateGameObject();
            NavMeshComponent  component;
            component.NavMesh = path;
            Ecs.AddComponent(entity, component);
            return entity;
        }
    };
}

TEST_CASE("Navigation system - a baked navmesh loads and answers path queries")
{
    NavigationWorld world;
    world.BakeFloor("Level.navmesh", 0.0f);
    LogCapture        log;
    const ECS::Entity entity = world.NavMesh("nav://Level.navmesh");
    world.Context.UpdateContext(1.0f, FRAME);

    NavigationSystem& navigation = *world.Context.GetNavigationSystem();
    REQUIRE(navigation.HasNavMesh());
    CHECK(world.Ecs.GetComponent<NavMeshComponent>(entity).Loaded);
    CHECK(log.Count("[Navigation] Loaded nav://Level.navmesh.") == 1);

    std::vector<HM::Vector3> corners;
    REQUIRE(navigation.FindPath(HM::Vector3(-7.0f, -7.0f, 0.0f), HM::Vector3(7.0f, 7.0f, 0.0f), corners) == HN::NavPathStatus::Complete);
    REQUIRE(corners.size() >= 2);
    CHECK(std::abs(corners.back().x() - 7.0f) < 0.5f);
    CHECK(std::abs(corners.back().y() - 7.0f) < 0.5f);
    CHECK(navigation.SamplePosition(HM::Vector3(0.0f, 0.0f, 0.5f), 1.0f).has_value());

    // Disabling it unloads it.
    world.Ecs.GetComponent<NavMeshComponent>(entity).Enabled = false;
    world.Context.UpdateContext(1.0f, FRAME);
    CHECK_FALSE(navigation.HasNavMesh());
    CHECK_FALSE(world.Ecs.GetComponent<NavMeshComponent>(entity).Loaded);
}

TEST_CASE("Navigation system - a rewritten file is read again within a poll interval")
{
    NavigationWorld world;
    world.BakeFloor("Level.navmesh", 0.0f);
    world.BakeFloor("Moved.navmesh", 100.0f);
    world.NavMesh("nav://Level.navmesh");

    NavigationSystem& navigation = *world.Context.GetNavigationSystem();
    const auto        start      = std::chrono::steady_clock::now();
    navigation.Update(world.Ecs, start);
    REQUIRE(navigation.HasNavMesh());
    const HM::Vector3 moved(100.0f, 0.0f, 0.5f);
    CHECK_FALSE(navigation.SamplePosition(moved, 1.0f).has_value());

    const std::filesystem::path level = world.Dir.Path() / "Level.navmesh";
    std::filesystem::copy_file(world.Dir.Path() / "Moved.navmesh", level, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::last_write_time(level, std::filesystem::last_write_time(level) + std::chrono::seconds(5));

    navigation.Update(world.Ecs, start + std::chrono::milliseconds(500)); // not polled yet
    CHECK_FALSE(navigation.SamplePosition(moved, 1.0f).has_value());
    navigation.Update(world.Ecs, start + NavigationSystem::POLL_INTERVAL);
    CHECK(navigation.SamplePosition(moved, 1.0f).has_value());
}

TEST_CASE("Navigation system - a missing or broken file is logged once")
{
    NavigationWorld world;
    world.Dir.WriteFile("Broken.navmesh", "not a navmesh");
    LogCapture        log;
    const ECS::Entity entity = world.NavMesh("nav://Missing.navmesh");

    NavigationSystem& navigation = *world.Context.GetNavigationSystem();
    const auto        start      = std::chrono::steady_clock::now();
    for (int second = 0; second < 5; ++second)
        navigation.Update(world.Ecs, start + std::chrono::seconds(second));
    CHECK_FALSE(navigation.HasNavMesh());
    CHECK(log.Count("[Navigation] nav://Missing.navmesh: the file does not exist.") == 1);

    world.Ecs.GetComponent<NavMeshComponent>(entity).NavMesh = "nav://Broken.navmesh";
    for (int second = 5; second < 10; ++second)
        navigation.Update(world.Ecs, start + std::chrono::seconds(second));
    CHECK_FALSE(navigation.HasNavMesh());
    CHECK(log.Count("[Navigation] nav://Broken.navmesh: it is not a navmesh file (no HNAV magic).") == 1);
}

TEST_CASE("Navigation system - the first enabled component by entity id is the one loaded")
{
    NavigationWorld world;
    world.BakeFloor("Level.navmesh", 0.0f);
    const ECS::Entity first  = world.NavMesh("nav://Level.navmesh");
    const ECS::Entity second = world.NavMesh("nav://Missing.navmesh");
    world.Context.UpdateContext(1.0f, FRAME);
    CHECK(world.Context.GetNavigationSystem()->GetNavMeshKey() == "nav://Level.navmesh");
    CHECK(world.Ecs.GetComponent<NavMeshComponent>(first).Loaded);
    CHECK_FALSE(world.Ecs.GetComponent<NavMeshComponent>(second).Loaded);
}

TEST_CASE("Navigation system - the component round-trips through a snapshot without its runtime state")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    SceneManager&     scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    NavMeshComponent component;
    component.Enabled             = false;
    component.NavMesh             = "Navigation/Level.navmesh";
    component.AgentRadius         = 0.6f;
    component.AgentHeight         = 2.0f;
    component.MaxClimb            = 0.5f;
    component.MaxSlope            = 30.0f;
    component.CellSize            = 0.2f;
    component.CellHeight          = 0.1f;
    component.IncludeRenderMeshes = true;
    component.Loaded              = true;
    ecs.AddComponent(entity, component);

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    CHECK(snapshot.Yaml.find("NavMesh: Navigation/Level.navmesh") != std::string::npos);
    CHECK(snapshot.Yaml.find("Loaded") == std::string::npos);
    REQUIRE(scenes.RestoreSnapshot(snapshot));
    const NavMeshComponent& restored = ecs.GetComponent<NavMeshComponent>(entity);
    CHECK_FALSE(restored.Enabled);
    CHECK(restored.AgentRadius == 0.6f);
    CHECK(restored.MaxSlope == 30.0f);
    CHECK(restored.IncludeRenderMeshes);
    CHECK_FALSE(restored.Loaded);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}
