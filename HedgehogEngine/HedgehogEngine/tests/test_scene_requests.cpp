#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Events/SceneEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "ECS/api/System.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <functional>
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

    // Calls a function in its first OnUpdate, so a request is made in the middle of the frame.
    class Requester : public ECS::System
    {
    public:
        Requester(std::function<void()> request, std::vector<std::string>& log)
            : m_Request(std::move(request))
            , m_Log(log)
        {
        }

        void OnUpdate(ECS::ECS&, float) override
        {
            m_Log.push_back("requester.update");
            if (!m_Done)
            {
                m_Done = true;
                m_Request();
            }
        }

    private:
        std::function<void()>     m_Request;
        std::vector<std::string>& m_Log;
        bool                      m_Done = false;
    };

    class LaterSystem : public ECS::System
    {
    public:
        explicit LaterSystem(std::vector<std::string>& log)
            : m_Log(log)
        {
        }
        void OnUpdate(ECS::ECS&, float) override { m_Log.push_back("later.update"); }

    private:
        std::vector<std::string>& m_Log;
    };

    class AnimationProbe : public ECS::System
    {
    public:
        explicit AnimationProbe(std::vector<std::string>& log)
            : m_Log(log)
        {
        }
        [[nodiscard]] ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Animation; }
        void OnFrame(ECS::ECS&, const ECS::FrameContext&) override { m_Log.push_back("animation"); }

    private:
        std::vector<std::string>& m_Log;
    };

    // An engine with scenes:// on a temp folder holding A (a floor and a cube), B (a floor and two
    // cubes high up) and C (a lone object), the scene events counted, A loaded.
    struct SceneWorld
    {
        TempDir                  Dir;
        TempDir                  Saves;
        EngineContext            Context;
        SceneManager&            Scenes = Context.GetSceneManager();
        std::vector<std::string> Loaded;
        int                      GameLoaded = 0;
        std::string              YamlA;

        SceneWorld()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("scenes://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));
            Context.GetSaveGames().SetSaveDirectory(Saves.Path());

            Context.GetEventBus().Subscribe<SceneLoadedEvent>([this](const SceneLoadedEvent& event)
                                                              { Loaded.push_back(event.Path); });
            Context.GetEventBus().Subscribe<GameLoadedEvent>([this](const GameLoadedEvent&) { ++GameLoaded; });

            WriteScene("A", { 5.0f }, true);
            WriteScene("B", { 8.0f, 8.0f }, true);
            WriteScene("C", {}, false);
            Dir.WriteFile("B.txt", "not a scene");
            REQUIRE(Scenes.LoadScene((Dir.Path() / "A.yaml").string()));
            YamlA = Scenes.CaptureSnapshot().Yaml;
        }

        // A floor (or a lone object) and a dynamic cube per height; written to name.yaml.
        void WriteScene(const std::string& name, const std::vector<float>& cubeHeights, bool floor)
        {
            Scenes.ResetScene();
            Scenes.SetSceneName(name);
            if (floor)
            {
                const ECS::Entity ground = Object({ 0.0f, 0.0f, 0.0f });
                Ecs().GetComponent<TransformComponent>(ground).Scale = HM::Vector3(50.0f, 50.0f, 0.5f);
                Ecs().AddComponent(ground, ColliderComponent{});
            }
            else
            {
                Object({ 1.0f, 2.0f, 3.0f });
            }
            for (size_t i = 0; i < cubeHeights.size(); ++i)
            {
                const ECS::Entity cube = Object({ static_cast<float>(i) * 5.0f, 0.0f, cubeHeights[i] });
                Ecs().AddComponent(cube, ColliderComponent{});
                Ecs().AddComponent(cube, RigidBodyComponent{});
            }
            REQUIRE(Scenes.SaveScene((Dir.Path() / (name + ".yaml")).string()));
        }

        ECS::ECS& Ecs() { return Context.GetECS(); }

        ECS::Entity Object(const HM::Vector3& position)
        {
            const ECS::Entity entity = Scenes.CreateGameObject();
            Ecs().GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            return entity;
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem& Physics() { return *Context.GetPhysicsSystem(); }
    };
}

TEST_CASE("Scene requests - processed after every system's update and before Animation")
{
    SceneWorld               world;
    std::vector<std::string> log;
    world.Ecs().RegisterSystem<Requester>([&] { CHECK(world.Context.RequestSceneLoad("scenes://B.yaml")); }, log);
    world.Ecs().RegisterSystem<LaterSystem>(log);
    world.Ecs().RegisterSystem<AnimationProbe>(log);
    world.Context.GetEventBus().Subscribe<SceneLoadedEvent>(
        [&](const SceneLoadedEvent& event) { log.push_back("loaded " + event.Path); });

    REQUIRE(world.Context.Play());
    world.Frames(1);
    const uint64_t steps = world.Context.GetFixedStepClock().StepCount;

    const std::vector<std::string> expected{ "requester.update", "later.update", "loaded scenes://B.yaml",
                                             "animation" };
    CHECK(log == expected);
    CHECK(world.Scenes.GetScenePath() == "scenes://B.yaml");
    CHECK(world.Scenes.GetSceneName() == "B");

    world.Frames(3);
    CHECK(world.Context.GetFixedStepClock().StepCount > steps); // the clock is not reset
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - Stop gives back the scene and its path from before Play")
{
    SceneWorld        world;
    const std::string path = world.Scenes.GetScenePath();
    CHECK(path == "scenes://A.yaml");

    // The snapshot carries the path.
    const SceneSnapshot snapshot = world.Scenes.CaptureSnapshot();
    CHECK(snapshot.ScenePath == path);
    world.Scenes.ResetScene();
    CHECK(world.Scenes.GetScenePath().empty());
    REQUIRE(world.Scenes.RestoreSnapshot(snapshot));
    CHECK(world.Scenes.GetScenePath() == path);

    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    world.Frames(1);
    REQUIRE(world.Scenes.GetScenePath() == "scenes://B.yaml");
    REQUIRE(world.Context.Stop());

    CHECK(world.Scenes.CaptureSnapshot().Yaml == world.YamlA);
    CHECK(world.Scenes.GetScenePath() == path);
    CHECK(world.Scenes.GetSceneName() == "A");
}

TEST_CASE("Scene requests - the new scene's cubes get bodies and fall, the old bodies are gone")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    CHECK(world.Physics().GetBodyCount() == 2);
    world.Frames(5);

    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    world.Frames(2); // the bodies are made by the next frame's fixed step
    CHECK(world.Physics().GetBodyCount() == 3); // B's floor and two cubes, none of A's

    ECS::Entity cube = ECS::INVALID_ENTITY;
    for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
    {
        if (world.Ecs().IsAlive(entity) && world.Ecs().HasComponent<RigidBodyComponent>(entity))
            cube = entity;
    }
    REQUIRE(cube != ECS::INVALID_ENTITY);

    world.Frames(30);
    CHECK(world.Ecs().GetComponent<TransformComponent>(cube).Position.z() < 8.0f);
    world.Frames(120);
    CHECK(world.Ecs().GetComponent<TransformComponent>(cube).Position.z() < 3.0f);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - an old scene's sound stops and its particles leave the frame list")
{
    SceneWorld          world;
    HA::AudioEngineDesc desc;
    desc.NoDevice = true;
    REQUIRE(world.Context.GetAudioEngine().Init(desc));

    ParticleEffect effect   = MakeDefaultParticleEffect();
    effect.Emitters[0].Rate = 10.0f;
    world.Dir.WriteFile("Fire.particle", WriteParticleEffect(effect));

    const ECS::Entity    source = world.Object({ 0.0f, 0.0f, 0.0f });
    AudioSourceComponent sound;
    sound.Clip = "Audio/Tone440.wav";
    sound.Loop = true;
    world.Ecs().AddComponent(source, sound);
    ParticleSystemComponent particles;
    particles.Effect = "scenes://Fire.particle";
    world.Ecs().AddComponent(source, particles);

    REQUIRE(world.Context.Play());
    world.Frames(2);
    CHECK(world.Context.GetAudioEngine().GetActiveSoundCount() == 1);
    CHECK_FALSE(world.Context.GetParticleSystem()->GetFrameEmitters().empty());

    REQUIRE(world.Context.RequestSceneLoad("scenes://C.yaml"));
    world.Frames(2);
    CHECK(world.Context.GetAudioEngine().GetActiveSoundCount() == 0);
    CHECK(world.Context.GetParticleSystem()->GetFrameEmitters().empty());
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - refused in Edit mode, for a non-.yaml, a missing file and a parent folder")
{
    SceneWorld world;
    LogCapture log;

    CHECK_FALSE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    CHECK(log.Count("[Scene]") == 1);

    REQUIRE(world.Context.Play());
    CHECK_FALSE(world.Context.RequestSceneLoad("scenes://B.txt"));
    CHECK(log.Count("[Scene]") == 2);
    CHECK_FALSE(world.Context.RequestSceneLoad("scenes://Missing.yaml"));
    CHECK(log.Count("[Scene]") == 3);
    CHECK_FALSE(world.Context.RequestSceneLoad("scenes://../B.yaml"));
    CHECK(log.Count("[Scene]") == 4);

    world.Frames(2);
    CHECK(world.Loaded.empty());
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");
    CHECK(world.Scenes.GetSceneName() == "A");
    REQUIRE(world.Context.Stop());
    CHECK(world.Scenes.CaptureSnapshot().Yaml == world.YamlA);
}

TEST_CASE("Scene requests - two requests in one frame load the last")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    REQUIRE(world.Context.RequestSceneLoad("scenes://C.yaml"));
    world.Frames(1);

    CHECK(world.Loaded == std::vector<std::string>{ "scenes://C.yaml" });
    CHECK(world.Scenes.GetScenePath() == "scenes://C.yaml");
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - a save load requested after a scene request wins with one warning")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.GetSaveGames().RequestSave("s"));
    world.Frames(1);

    LogCapture log;
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    REQUIRE(world.Context.GetSaveGames().RequestLoad("s"));
    world.Frames(1);

    CHECK(world.Loaded.empty());
    CHECK(world.GameLoaded == 1);
    CHECK(world.Physics().GetBodyCount() == 2); // the save's world, A's
    CHECK(log.Count("[Scene] The request to load scenes://B.yaml is dropped") == 1);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - a scene request after a save load wins with one warning")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.GetSaveGames().RequestSave("s"));
    world.Frames(1);

    LogCapture log;
    REQUIRE(world.Context.GetSaveGames().RequestLoad("s"));
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    world.Frames(1);

    CHECK(world.Loaded == std::vector<std::string>{ "scenes://B.yaml" });
    CHECK(world.GameLoaded == 0);
    CHECK(log.Count("[Scene] The load of slot 's' is dropped") == 1);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - a save in the same frame sees the old world; Stop drops a pending request")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.GetSaveGames().RequestSave("s"));
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    world.Frames(1);

    CHECK(world.Loaded.size() == 1);
    const auto slots = world.Context.GetSaveGames().ListSlots();
    REQUIRE(slots.size() == 1);
    CHECK(slots[0].Metadata.ScenePath == "scenes://A.yaml");
    REQUIRE(world.Context.Stop());

    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.Pause());
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    REQUIRE(world.Context.Stop());
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");

    world.Loaded.clear();
    REQUIRE(world.Context.Play());
    world.Frames(2);
    CHECK(world.Loaded.empty());
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - a request made while Paused loads once Play resumes")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.Pause());
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    world.Frames(2);
    CHECK(world.Loaded.empty());
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");

    REQUIRE(world.Context.Resume());
    world.Frames(1);
    CHECK(world.Loaded == std::vector<std::string>{ "scenes://B.yaml" });
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - a scene that does not load keeps the running scene")
{
    SceneWorld world;
    world.Dir.WriteFile("Broken.yaml", "Version: 99\n");
    REQUIRE(world.Context.Play());
    world.Frames(2);
    REQUIRE(world.Physics().GetBodyCount() == 2);

    LogCapture log;
    REQUIRE(world.Context.RequestSceneLoad("scenes://Broken.yaml"));
    world.Frames(2);

    CHECK(world.Loaded.empty());
    CHECK(log.Count("[Scene] scenes://Broken.yaml could not be loaded; the running scene is kept.") == 1);
    CHECK(world.Scenes.GetScenePath() == "scenes://A.yaml");
    CHECK(world.Scenes.GetSceneName() == "A");
    CHECK(world.Physics().GetBodyCount() == 2);
    REQUIRE(world.Context.Stop());
    CHECK(world.Scenes.CaptureSnapshot().Yaml == world.YamlA);
}

TEST_CASE("Scene requests - paths are accepted however they are spelt")
{
    SceneWorld world;
    world.Dir.WriteFile("Upper.YAML", world.Scenes.CaptureSnapshot().Yaml);
    world.Dir.WriteFile("My..Level.yaml", world.Scenes.CaptureSnapshot().Yaml);
    REQUIRE(world.Context.Play());

    REQUIRE(world.Context.RequestSceneLoad("scenes://.\\B.yaml"));
    world.Frames(1);
    CHECK(world.Scenes.GetScenePath() == "scenes://B.yaml");

    REQUIRE(world.Context.RequestSceneLoad("scenes://Upper.YAML"));
    world.Frames(1);
    CHECK(world.Scenes.GetScenePath() == "scenes://Upper.YAML");

    REQUIRE(world.Context.RequestSceneLoad("scenes://My..Level.yaml"));
    world.Frames(1);
    CHECK(world.Scenes.GetScenePath() == "scenes://My..Level.yaml");
    CHECK(world.Loaded.size() == 3);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Scene requests - of three requests between scenes and save loads, the last wins")
{
    SceneWorld world;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.GetSaveGames().RequestSave("s"));
    world.Frames(1);

    // load, scene, load: the second load wins.
    REQUIRE(world.Context.GetSaveGames().RequestLoad("s"));
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    REQUIRE(world.Context.GetSaveGames().RequestLoad("s"));
    world.Frames(1);
    CHECK(world.Loaded.empty());
    CHECK(world.GameLoaded == 1);

    // scene, load, scene: the second scene wins.
    REQUIRE(world.Context.RequestSceneLoad("scenes://B.yaml"));
    REQUIRE(world.Context.GetSaveGames().RequestLoad("s"));
    REQUIRE(world.Context.RequestSceneLoad("scenes://C.yaml"));
    world.Frames(1);
    CHECK(world.Loaded == std::vector<std::string>{ "scenes://C.yaml" });
    CHECK(world.GameLoaded == 1);
    REQUIRE(world.Context.Stop());
}
