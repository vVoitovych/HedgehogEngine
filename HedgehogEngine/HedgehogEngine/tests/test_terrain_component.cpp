#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"
#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    // Every log line while it lives.
    class LogCapture
    {
    public:
        LogCapture()
            : m_Sink(EngineLogger::Logger::Instance().AddSink([this](EngineLogger::LogLevel, const std::string& line)
                                                               { m_Lines.push_back(line); }))
        {
        }
        ~LogCapture() { EngineLogger::Logger::Instance().RemoveSink(m_Sink); }

        LogCapture(const LogCapture&)            = delete;
        LogCapture& operator=(const LogCapture&) = delete;

        size_t Count(const std::string& fragment) const
        {
            return static_cast<size_t>(std::count_if(m_Lines.begin(), m_Lines.end(), [&](const std::string& line)
                                                     { return line.find(fragment) != std::string::npos; }));
        }

    private:
        std::vector<std::string> m_Lines;
        int                      m_Sink;
    };

    // assets:// on a temp folder holding Terrain/Ground.terrain, a small hill with two layer maps.
    struct TerrainFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;
        TerrainData           Ground = MakeDefaultTerrain();

        TerrainFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            Files.Register(std::move(fs));

            Ground.Resolution      = 33;
            Ground.SplatResolution = 16;
            Ground.Heights.assign(33 * 33, 0.0f);
            Ground.Heights[16 * 33 + 16] = 12.0f;
            Ground.Splat.assign(16 * 16 * 4, 0);
            for (size_t texel = 0; texel < Ground.Splat.size(); texel += 4)
                Ground.Splat[texel + 1] = 255;
            Ground.Layers[0].BaseColor = "Textures/Grass.png";
            Ground.Layers[1].NormalMap = "Textures\\Rock_Normal.png";
            Dir.WriteFile("Terrain/Ground.terrain", WriteTerrain(Ground));
        }
    };
}

TEST_CASE("Terrain component - its defaults")
{
    const TerrainComponent terrain;
    CHECK(terrain.Enabled);
    CHECK(terrain.Terrain.empty());
    CHECK(terrain.RenderLayer == 0);
    CHECK(terrain.CastShadows);
    CHECK(terrain.PhysicsLayer == 0);
    CHECK(terrain.Friction == 0.5f);
    CHECK(terrain.TerrainIndex == -1);
}

TEST_CASE("Terrain component - a scene holding one round-trips byte for byte, without its runtime state")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    SceneManager&     scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    TerrainComponent terrain;
    terrain.Enabled         = false;
    terrain.Terrain         = "Terrain/Ground.terrain";
    terrain.RenderLayer     = 2;
    terrain.CastShadows     = false;
    terrain.PhysicsLayer    = 3;
    terrain.Friction        = 0.8f;
    terrain.TerrainIndex    = 7;
    terrain.ResolvedTerrain = "Terrain/Ground.terrain";
    ecs.AddComponent(entity, terrain);

    const auto& view = context.GetTerrainSystem()->GetEntities();
    CHECK(std::find(view.begin(), view.end(), entity) != view.end());

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    CHECK(snapshot.Yaml.find("TerrainComponent") != std::string::npos);
    CHECK(snapshot.Yaml.find("Terrain: Terrain/Ground.terrain") != std::string::npos);
    CHECK(snapshot.Yaml.find("TerrainIndex") == std::string::npos);

    REQUIRE(scenes.RestoreSnapshot(snapshot));
    REQUIRE(ecs.HasComponent<TerrainComponent>(entity));
    const TerrainComponent& restored = ecs.GetComponent<TerrainComponent>(entity);
    CHECK_FALSE(restored.Enabled);
    CHECK(restored.Terrain == "Terrain/Ground.terrain");
    CHECK(restored.RenderLayer == 2);
    CHECK_FALSE(restored.CastShadows);
    CHECK(restored.PhysicsLayer == 3);
    CHECK(restored.Friction == 0.8f);
    CHECK(restored.TerrainIndex == -1);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}

TEST_CASE("Terrain container - one file is one terrain however it is spelt, loaded dirty")
{
    TerrainFiles     files;
    TerrainContainer terrains(files.Files);

    const std::optional<size_t> first = terrains.FindOrLoad("Terrain/Ground.terrain");
    REQUIRE(first);
    CHECK(terrains.FindOrLoad("assets://Terrain/Ground.terrain") == first);
    CHECK(terrains.FindOrLoad("Terrain\\./Ground.terrain") == first);
    CHECK(terrains.GetTerrainCount() == 1);
    CHECK(terrains.GetPath(*first) == "Terrain/Ground.terrain");

    const TerrainData& ground = terrains.GetTerrain(*first);
    CHECK(ground.Resolution == 33);
    CHECK(ground.Heights[16 * 33 + 16] == doctest::Approx(12.0f).epsilon(0.001));
    CHECK(ground.Layers == files.Ground.Layers);
    CHECK(terrains.GetDirty(*first) == TerrainDirty::All);

    terrains.ClearDirty(*first, TerrainDirty::All);
    CHECK(terrains.GetDirty(*first) == TerrainDirty::None);
    terrains.MarkDirty(*first, TerrainDirty::Splat);
    CHECK(HasAny(terrains.GetDirty(*first), TerrainDirty::Splat));
    CHECK_FALSE(HasAny(terrains.GetDirty(*first), TerrainDirty::Heights));
    terrains.ClearDirty(*first, TerrainDirty::Splat);
    CHECK(terrains.GetDirty(*first) == TerrainDirty::None);

    CHECK_FALSE(terrains.FindOrLoad(""));
}

TEST_CASE("Terrain container - an edited terrain saves what the file format reads back")
{
    TerrainFiles     files;
    TerrainContainer terrains(files.Files);
    const size_t     index = *terrains.FindOrLoad("Terrain/Ground.terrain");

    TerrainData& ground          = terrains.GetTerrain(index);
    ground.Heights[0]            = 20.0f;
    ground.Layers[3].BaseColor   = "Textures/Sand.png";
    terrains.MarkDirty(index, TerrainDirty::Heights | TerrainDirty::Layers);
    REQUIRE(terrains.Save(index));

    const std::optional<std::string> text = files.Files.ReadTextFile("assets://Terrain/Ground.terrain");
    REQUIRE(text);
    const TerrainReadResult saved = ParseTerrain(*text);
    REQUIRE(saved.Data);
    CHECK(saved.Data->Heights[0] == doctest::Approx(20.0f).epsilon(0.001));
    CHECK(saved.Data->Layers[3].BaseColor == "Textures/Sand.png");
    CHECK(*text == WriteTerrain(ground));
}

TEST_CASE("Terrain container - a file that does not load is logged once and not retried")
{
    TerrainFiles files;
    files.Dir.WriteFile("Terrain/Broken.terrain", "Version: 1\nSize: 10\n");
    TerrainContainer terrains(files.Files);

    LogCapture log;
    CHECK_FALSE(terrains.FindOrLoad("Terrain/Missing.terrain"));
    CHECK_FALSE(terrains.FindOrLoad("assets://Terrain/Missing.terrain"));
    CHECK(log.Count("[Terrain] assets://Terrain/Missing.terrain cannot be read") == 1);

    CHECK_FALSE(terrains.FindOrLoad("Terrain/Broken.terrain"));
    CHECK_FALSE(terrains.FindOrLoad("Terrain/Broken.terrain"));
    CHECK(log.Count("[Terrain] assets://Terrain/Broken.terrain:") == 1);
    CHECK(terrains.GetTerrainCount() == 0);

    // A good file after failures still gets index 0.
    CHECK(terrains.FindOrLoad("Terrain/Ground.terrain") == size_t{ 0 });
}

TEST_CASE("Terrain system - resolves each terrain's index once, and again when its path changes")
{
    TerrainFiles     files;
    TerrainContainer terrains(files.Files);
    EngineContext    context;
    ECS::ECS&        ecs     = context.GetECS();
    SceneManager&    scenes  = context.GetSceneManager();
    const ECS::Entity first  = scenes.CreateGameObject();
    const ECS::Entity second = scenes.CreateGameObject();
    const ECS::Entity none   = scenes.CreateGameObject();

    TerrainComponent terrain;
    terrain.Terrain = "Terrain/Ground.terrain";
    ecs.AddComponent(first, terrain);
    terrain.Terrain = "assets://Terrain/Ground.terrain";
    ecs.AddComponent(second, terrain);
    ecs.AddComponent(none, TerrainComponent{});

    TerrainSystem& system = *context.GetTerrainSystem();
    system.Update(ecs, terrains);
    CHECK(ecs.GetComponent<TerrainComponent>(first).TerrainIndex == 0);
    CHECK(ecs.GetComponent<TerrainComponent>(second).TerrainIndex == 0);
    CHECK(ecs.GetComponent<TerrainComponent>(none).TerrainIndex == -1);

    LogCapture log;
    ecs.GetComponent<TerrainComponent>(second).Terrain = "Terrain/Missing.terrain";
    system.Update(ecs, terrains);
    system.Update(ecs, terrains);
    CHECK(ecs.GetComponent<TerrainComponent>(second).TerrainIndex == -1);
    CHECK(log.Count("[Terrain]") == 1);
    CHECK(ecs.GetComponent<TerrainComponent>(first).TerrainIndex == 0);
}

TEST_CASE("Terrain dependencies - a scene's terrain brings its file and its layer maps")
{
    TerrainFiles files;
    files.Dir.WriteFile("Textures/Grass.png", "");
    files.Dir.WriteFile("Textures/Rock_Normal.png", "");
    files.Dir.WriteFile("Scenes/Hills.yaml", R"(Version: 1
Scene name: Hills
Scene:
  - Entity: 0
    Name: Root
    Parent: 0
    Children:
      - Entity: 1
        Name: Ground
        Parent: 0
        TerrainComponent:
          Terrain: Terrain\Ground.terrain
        Children: []
)");

    EcsSerialization::AssetDependencyCollector collector;
    RegisterEngineAssetDependencies(collector);
    const EcsSerialization::AssetDependencies result = collector.CollectScene("assets://Scenes/Hills.yaml", files.Files);
    const std::vector<std::string> expected = {
        "assets://Scenes/Hills.yaml",
        "assets://Terrain/Ground.terrain",
        "assets://Textures/Grass.png",
        "assets://Textures/Rock_Normal.png",
    };
    CHECK(result.Assets == expected);
    CHECK(result.Warnings.empty());
}
