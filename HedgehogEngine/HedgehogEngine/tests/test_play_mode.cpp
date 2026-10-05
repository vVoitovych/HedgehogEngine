#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include "FileSystem/tests/test_helpers.hpp"

#include <optional>
#include <string>
#include <vector>

using HedgehogEngine::EngineContext;
using HedgehogEngine::PlayState;
using HedgehogEngine::SaveGameManager;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Records every play-mode event it receives, into its own counters and a shared log.
    class RecordingSystem : public ECS::System
    {
    public:
        RecordingSystem(std::string name, std::vector<std::string>& log)
            : m_Name(std::move(name))
            , m_Log(log)
        {
        }

        void OnPlayStart(ECS::ECS&) override  { m_Log.push_back(m_Name + ".start"); }
        void OnPlayPause(ECS::ECS&) override  { m_Log.push_back(m_Name + ".pause"); }
        void OnPlayResume(ECS::ECS&) override { m_Log.push_back(m_Name + ".resume"); }

        void OnPlayStop(ECS::ECS& ecs) override
        {
            m_Log.push_back(m_Name + ".stop");
            if (Watched)
                PositionAtStop = ecs.GetComponent<TransformComponent>(*Watched).Position.x();
        }

        void OnFixedUpdate(ECS::ECS&, float fixedDeltaTime) override
        {
            FixedDeltaTimes.push_back(fixedDeltaTime);
        }

        void OnUpdate(ECS::ECS&, float deltaTime) override { DeltaTimes.push_back(deltaTime); }

        std::vector<float>         FixedDeltaTimes;
        std::vector<float>         DeltaTimes;
        std::optional<ECS::Entity> Watched;
        float                      PositionAtStop = 0.0f;

    private:
        std::string               m_Name;
        std::vector<std::string>& m_Log;
    };

    // The ECS holds one system per type, so a second recorder needs its own type.
    class SecondRecordingSystem : public RecordingSystem
    {
    public:
        using RecordingSystem::RecordingSystem;
    };

    // Asks for a save in its first OnUpdate and for a load of that save in its second.
    class SaveRequester : public ECS::System
    {
    public:
        SaveRequester(SaveGameManager& saves, std::vector<std::string>& log)
            : m_Saves(saves)
            , m_Log(log)
        {
        }

        void OnUpdate(ECS::ECS&, float) override
        {
            m_Log.push_back("requester.update");
            ++m_Updates;
            if (m_Updates == 1)
                CHECK(m_Saves.RequestSave("slot"));
            else if (m_Updates == 2)
                CHECK(m_Saves.RequestLoad("slot"));
        }

    private:
        SaveGameManager&          m_Saves;
        std::vector<std::string>& m_Log;
        int                       m_Updates = 0;
    };

    // Registered after SaveRequester: whether the slot exists yet when its own update runs.
    class LaterSystem : public ECS::System
    {
    public:
        LaterSystem(SaveGameManager& saves, std::vector<std::string>& log)
            : m_Saves(saves)
            , m_Log(log)
        {
        }

        void OnUpdate(ECS::ECS&, float) override
        {
            m_Log.push_back(m_Saves.SlotExists("slot") ? "later.update(saved)" : "later.update");
        }

    private:
        SaveGameManager&          m_Saves;
        std::vector<std::string>& m_Log;
    };

    // Marks where the Animation phase runs in the frame.
    class AnimationProbe : public ECS::System
    {
    public:
        explicit AnimationProbe(std::vector<std::string>& log)
            : m_Log(log)
        {
        }

        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Animation; }
        void OnFrame(ECS::ECS&, const ECS::FrameContext&) override { m_Log.push_back("animation"); }

    private:
        std::vector<std::string>& m_Log;
    };
}

TEST_CASE("Play mode - no event reaches a system in Edit or while paused")
{
    EngineContext            context;
    std::vector<std::string> log;
    auto system = context.GetECS().RegisterSystem<RecordingSystem>("A", log);

    CHECK(context.GetPlayState() == PlayState::Edit);
    context.UpdatePlayMode(1.0f);
    CHECK(system->FixedDeltaTimes.empty());
    CHECK(system->DeltaTimes.empty());
    CHECK(log.empty());

    REQUIRE(context.Play());
    REQUIRE(context.Pause());
    CHECK(context.GetPlayState() == PlayState::Paused);
    context.UpdatePlayMode(1.0f);
    CHECK(system->FixedDeltaTimes.empty());
    CHECK(system->DeltaTimes.empty());

    REQUIRE(context.Resume());
    CHECK(context.GetPlayState() == PlayState::Playing);
    context.UpdatePlayMode(STEP);
    CHECK(system->FixedDeltaTimes.size() == 1);
    CHECK(system->DeltaTimes.size() == 1);

    REQUIRE(context.Stop());
    CHECK(context.GetPlayState() == PlayState::Edit);
    context.UpdatePlayMode(STEP);
    CHECK(system->DeltaTimes.size() == 1);
}

TEST_CASE("Play mode - a 1/30 s frame is two fixed steps of 1/60 s, then one update of 1/30 s")
{
    EngineContext            context;
    std::vector<std::string> log;
    auto system = context.GetECS().RegisterSystem<RecordingSystem>("A", log);

    REQUIRE(context.Play());
    context.UpdatePlayMode(1.0f / 30.0f);

    REQUIRE(system->FixedDeltaTimes.size() == 2);
    CHECK(system->FixedDeltaTimes[0] == STEP);
    CHECK(system->FixedDeltaTimes[1] == STEP);
    REQUIRE(system->DeltaTimes.size() == 1);
    CHECK(system->DeltaTimes[0] == doctest::Approx(1.0f / 30.0f));
}

TEST_CASE("Play mode - TimeScale 0.5 halves both the steps and the frame time")
{
    EngineContext            context;
    std::vector<std::string> log;
    auto system = context.GetECS().RegisterSystem<RecordingSystem>("A", log);

    context.GetFixedStepClock().TimeScale = 0.5f;
    REQUIRE(context.Play());
    context.UpdatePlayMode(1.0f / 30.0f);

    CHECK(system->FixedDeltaTimes.size() == 1);
    REQUIRE(system->DeltaTimes.size() == 1);
    CHECK(system->DeltaTimes[0] == doctest::Approx(1.0f / 60.0f));
}

TEST_CASE("Play mode - each transition sends its event once; a call in the wrong state sends nothing")
{
    EngineContext            context;
    std::vector<std::string> log;
    (void)context.GetECS().RegisterSystem<RecordingSystem>("A", log);

    CHECK_FALSE(context.Pause());
    CHECK_FALSE(context.Resume());
    CHECK_FALSE(context.Stop());

    CHECK(context.Play());
    CHECK_FALSE(context.Play());
    CHECK_FALSE(context.Resume());
    CHECK(context.Pause());
    CHECK_FALSE(context.Pause());
    CHECK_FALSE(context.Play());
    CHECK(context.Resume());
    CHECK(context.Stop());
    CHECK_FALSE(context.Stop());

    const std::vector<std::string> expected{ "A.start", "A.pause", "A.resume", "A.stop" };
    CHECK(log == expected);
}

TEST_CASE("Play mode - systems start in registration order and stop in reverse")
{
    EngineContext            context;
    std::vector<std::string> log;
    (void)context.GetECS().RegisterSystem<RecordingSystem>("A", log);
    (void)context.GetECS().RegisterSystem<SecondRecordingSystem>("B", log);

    REQUIRE(context.Play());
    REQUIRE(context.Stop());

    const std::vector<std::string> expected{ "A.start", "B.start", "B.stop", "A.stop" };
    CHECK(log == expected);
}

TEST_CASE("Play mode - Stop restores the scene as it was on Play, after OnPlayStop")
{
    EngineContext            context;
    auto&                    scenes = context.GetSceneManager();
    ECS::ECS&                ecs    = context.GetECS();
    std::vector<std::string> log;
    auto system = ecs.RegisterSystem<RecordingSystem>("A", log);

    const ECS::Entity kept   = scenes.CreateGameObject();
    const ECS::Entity doomed = scenes.CreateGameObject(kept);
    system->Watched          = kept;

    REQUIRE(context.Play());
    const std::string atPlay = scenes.CaptureSnapshot().Yaml;

    ecs.GetComponent<TransformComponent>(kept).Position = HM::Vector3(5.0f, 6.0f, 7.0f);
    scenes.DeleteGameObject(doomed);
    const ECS::Entity spawned = scenes.CreateGameObject();
    context.UpdatePlayMode(STEP);
    REQUIRE(context.Stop());

    // OnPlayStop saw the played scene; the restore came after it.
    CHECK(system->PositionAtStop == 5.0f);
    CHECK(scenes.CaptureSnapshot().Yaml == atPlay);
    CHECK(ecs.GetComponent<TransformComponent>(kept).Position.x() == 0.0f);
    CHECK(ecs.IsAlive(kept));
    CHECK(ecs.IsAlive(doomed));
    if (spawned != doomed)
        CHECK_FALSE(ecs.IsAlive(spawned));
}

TEST_CASE("Play mode - UpdateContext advances the clock once and runs gameplay once per frame")
{
    EngineContext            context;
    std::vector<std::string> log;
    auto system = context.GetECS().RegisterSystem<RecordingSystem>("A", log);

    context.UpdateContext(1.0f, 2.0f * STEP);
    CHECK(context.GetFixedStepClock().FrameCount == 0);
    CHECK(system->DeltaTimes.empty());

    REQUIRE(context.Play());
    context.UpdateContext(1.0f, 2.0f * STEP);
    CHECK(context.GetFixedStepClock().FrameCount == 1);
    CHECK(context.GetFixedStepClock().StepCount == 2);
    CHECK(system->FixedDeltaTimes.size() == 2);
    REQUIRE(system->DeltaTimes.size() == 1);
    CHECK(system->DeltaTimes[0] == doctest::Approx(2.0f * STEP));
}

TEST_CASE("Play mode - save and load requests run after every system's update and before Animation")
{
    TempDir                  saves;
    std::vector<std::string> log;
    EngineContext            context;
    SaveGameManager&         manager = context.GetSaveGames();
    manager.SetSaveDirectory(saves.Path());
    ECS::ECS& ecs = context.GetECS();
    // Registered after the engine's systems, SaveRequestSystem included, as ScriptSystem is.
    ecs.RegisterSystem<SaveRequester>(manager, log);
    ecs.RegisterSystem<LaterSystem>(manager, log);
    ecs.RegisterSystem<AnimationProbe>(log);
    context.GetEventBus().Subscribe<HedgehogEngine::GameLoadedEvent>(
        [&](const HedgehogEngine::GameLoadedEvent& loaded) { log.push_back("loaded " + loaded.Slot); });

    REQUIRE(context.Play());
    context.UpdateContext(1.0f, STEP);
    // The save is written after the later system's update, before Animation.
    CHECK(log == std::vector<std::string>{ "requester.update", "later.update", "animation" });
    CHECK(manager.SlotExists("slot"));

    log.clear();
    context.UpdateContext(1.0f, STEP);
    CHECK(log == std::vector<std::string>{ "requester.update", "later.update(saved)", "loaded slot", "animation" });
}
