#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "Logger/api/Logger.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::EngineContext;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Models/Animated/TwoBoneStrip.gltf: Bend turns Tip 45 -> 90 degrees about z over 1 s; Lift
    // moves Root up 0 -> 1 over 2 s.
    const std::string SKINNED = "Models/Animated/TwoBoneStrip.gltf";

    // Bending and Lifting, switched by the Lifted bool over half a second; Wave, a trigger, starts
    // Bending again from any state, itself included.
    const char* const STRIP = R"(Version: 1
Parameters:
  - Name: Lifted
    Type: Bool
  - Name: Wave
    Type: Trigger
States:
  - Name: Bending
    Clip: Bend
  - Name: Lifting
    Clip: Lift
Default: Bending
Transitions:
  - From: Bending
    To: Lifting
    Duration: 0.5
    Conditions:
      - {Parameter: Lifted, Op: True}
  - From: Lifting
    To: Bending
    Duration: 0.5
    Conditions:
      - {Parameter: Lifted, Op: False}
  - From: Any
    To: Bending
    Duration: 0
    CanTransitionToSelf: true
    Conditions:
      - {Parameter: Wave, Op: Triggered}
)";

    // Every Logger line while it lives.
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

    // An engine with anim:// on a temp folder holding the controller, and the strip using it.
    struct ControllerWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs = Context.GetECS();
        ECS::Entity   Strip{};
        std::vector<HedgehogEngine::AnimatorStateChangedEvent> Changes;

        explicit ControllerWorld(const std::string& controller = STRIP, const std::string& clip = "")
        {
            Dir.WriteFile("Strip.animctrl", controller);
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("anim://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));
            Context.GetEventBus().Subscribe<HedgehogEngine::AnimatorStateChangedEvent>(
                [this](const HedgehogEngine::AnimatorStateChangedEvent& event) { Changes.push_back(event); });

            Strip = Context.GetSceneManager().CreateGameObject();
            Ecs.AddComponent(Strip, HedgehogEngine::MeshComponent{});
            Context.GetMeshSystem()->LoadMesh(Ecs, Strip, SKINNED);
            AnimatorComponent animator;
            animator.Clip       = clip;
            animator.Controller = "anim://Strip.animctrl";
            Ecs.AddComponent(Strip, animator);
            Context.GetResourceCatalog().Update(*Context.GetRenderSystem(), *Context.GetMeshSystem());
        }

        void Frame(int count = 1)
        {
            for (int i = 0; i < count; ++i)
            {
                Context.UpdatePlayMode(STEP);
                Context.UpdateAnimation(STEP);
            }
        }

        HedgehogEngine::AnimationSystem& Animations() { return *Context.GetAnimationSystem(); }
        const AnimatorComponent&         Animator() { return Ecs.GetComponent<AnimatorComponent>(Strip); }
        std::string                      State() { return Animations().GetStateName(Ecs, Strip); }
        float Lift() { return Animator().Palette[0][3][1]; }
    };

    bool Near(float a, float b) { return std::abs(a - b) < 1e-3f; }
}

TEST_CASE("Animator controller - a bool moves the machine between states, crossfading over the transition")
{
    ControllerWorld world;
    REQUIRE(world.Animations().GetController(world.Ecs, world.Strip) != nullptr);

    REQUIRE(world.Context.Play());
    world.Frame();
    CHECK(world.State() == "Bending");
    CHECK(world.Animator().CurrentClip == "Bend");
    REQUIRE(world.Changes.size() == 1u);
    CHECK(world.Changes[0].From.empty());
    CHECK(world.Changes[0].To == "Bending");
    CHECK(Near(world.Lift(), 0.0f));

    CHECK(world.Animations().SetBool(world.Ecs, world.Strip, "Lifted", true));
    world.Frame(); // fires: Lifting starts at weight 0
    CHECK(world.State() == "Lifting");
    CHECK(world.Animator().CurrentClip == "Lift");
    REQUIRE(world.Changes.size() == 2u);
    CHECK(world.Changes[1].From == "Bending");
    CHECK(world.Changes[1].To == "Lifting");
    CHECK(Near(world.Lift(), 0.0f));

    world.Frame(15); // halfway through the 0.5 s fade: Lift at 0.25 s (0.125 up) at half weight
    CHECK(Near(world.Lift(), 0.0625f));
    world.Frame(30); // faded in: Lift alone, 0.75 s in
    CHECK(world.Animator().Blend.Count == 1u);
    CHECK(Near(world.Lift(), 0.375f));

    CHECK(world.Animations().SetBool(world.Ecs, world.Strip, "Lifted", false));
    world.Frame();
    CHECK(world.State() == "Bending");
    CHECK(world.Changes.size() == 3u);
    world.Frame(10);
    CHECK(world.Changes.size() == 3u); // one event per change
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Animator controller - a trigger fires its transition once, and is used up")
{
    ControllerWorld world;
    REQUIRE(world.Context.Play());
    world.Frame(30);
    CHECK(world.Animations().GetParameter(world.Ecs, world.Strip, "Wave") == 0.0f);

    CHECK(world.Animations().SetTrigger(world.Ecs, world.Strip, "Wave"));
    world.Frame(); // Any -> Bending, itself, from the start
    CHECK(world.State() == "Bending");
    CHECK(world.Changes.size() == 2u);
    CHECK(world.Changes[1].From == "Bending");
    CHECK(HedgehogEngine::AnimationSystem::GetTime(world.Animator()) == 0.0f);
    CHECK(world.Animations().GetParameter(world.Ecs, world.Strip, "Wave") == 0.0f);

    world.Frame(5);
    CHECK(world.Changes.size() == 2u);

    // Wrong names and types change nothing.
    CHECK_FALSE(world.Animations().SetFloat(world.Ecs, world.Strip, "Lifted", 1.0f));
    CHECK_FALSE(world.Animations().SetTrigger(world.Ecs, world.Strip, "Lifted"));
    CHECK_FALSE(world.Animations().SetInt(world.Ecs, world.Strip, "Height", 2));
    CHECK_FALSE(world.Animations().GetParameter(world.Ecs, world.Strip, "Height").has_value());
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Animator controller - Play names a state; Stop holds the machine; Stop of Play restores the scene")
{
    ControllerWorld world;
    const auto&     meshes = world.Context.GetResourceCatalog().GetMeshContainer();
    REQUIRE(world.Context.Play());
    world.Frame();

    LogCapture log;
    CHECK_FALSE(world.Animations().Play(world.Ecs, meshes, world.Strip, "Bend")); // a clip, not a state
    CHECK(log.Count("has no state 'Bend'") == 1u);
    CHECK(world.Animations().Play(world.Ecs, meshes, world.Strip, "Lifting", 0.0f));
    world.Frame();
    CHECK(world.State() == "Lifting");
    CHECK(world.Animator().Blend.Count == 1u); // no fade asked for

    world.Animations().Stop(world.Ecs, world.Strip);
    world.Frame(5);
    CHECK(world.Animator().Blend.Count == 0u); // the bind pose
    CHECK(Near(world.Lift(), 0.0f));
    CHECK(world.State() == "Lifting");         // held
    CHECK(world.Animations().Play(world.Ecs, meshes, world.Strip, "Bending"));
    world.Frame();
    CHECK(world.State() == "Bending");
    CHECK(world.Animator().CurrentClip == "Bend");

    REQUIRE(world.Context.Stop());
    CHECK(world.Animator().Controller == "anim://Strip.animctrl");
    CHECK(world.State().empty());
    // A second Play starts from the default state and the defaults again.
    REQUIRE(world.Context.Play());
    world.Frame();
    CHECK(world.State() == "Bending");
    CHECK(world.Animations().GetParameter(world.Ecs, world.Strip, "Lifted") == 0.0f);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Animator controller - a parameter set before the first frame (as a script's OnStart would) is kept")
{
    ControllerWorld world;
    REQUIRE(world.Context.Play());
    CHECK(world.Animations().SetBool(world.Ecs, world.Strip, "Lifted", true));
    world.Frame(); // the default state first
    CHECK(world.State() == "Bending");
    world.Frame(); // then its transition
    CHECK(world.State() == "Lifting");
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Animator controller - an edited file is read again once its write time moves on")
{
    ControllerWorld world;
    REQUIRE(world.Context.Play());
    world.Frame(3);
    CHECK(world.State() == "Bending");

    std::string edited = STRIP;
    edited.replace(edited.find("Default: Bending"), 16, "Default: Lifting");
    const std::filesystem::path file   = world.Dir.WriteFile("Strip.animctrl", edited);
    const auto                  before = std::filesystem::last_write_time(file);
    std::filesystem::last_write_time(file, before + std::chrono::seconds(2));

    LogCapture log;
    world.Animations().ReloadChangedControllers(std::chrono::steady_clock::now() + std::chrono::hours(1));
    CHECK(log.Count("[Animation] Reloaded anim://Strip.animctrl.") == 1u);
    world.Frame();
    CHECK(world.State() == "Lifting"); // the new default, from the start
    world.Animations().ReloadChangedControllers(std::chrono::steady_clock::now() + std::chrono::hours(2));
    CHECK(log.Count("Reloaded") == 1u); // not changed again
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Animator controller - a controller that cannot run logs why once, and Clip plays instead")
{
    SUBCASE("a state's clip the mesh lacks")
    {
        std::string missing = STRIP;
        missing.replace(missing.find("Clip: Lift"), 10, "Clip: Jump");
        LogCapture      log;
        ControllerWorld world(missing, "Lift");
        REQUIRE(world.Context.Play());
        world.Frame(3);
        CHECK(log.Count("state 'Lifting' of anim://Strip.animctrl plays clip 'Jump', which its mesh does not have") == 1u);
        CHECK(world.State().empty());
        CHECK(world.Animator().CurrentClip == "Lift");
        REQUIRE(world.Context.Stop());
    }
    SUBCASE("a file that does not parse")
    {
        LogCapture      log;
        ControllerWorld world("Version: 1\nStates: []\n", "Lift");
        REQUIRE(world.Context.Play());
        world.Frame(3);
        CHECK(log.Count("[Animation] anim://Strip.animctrl: Default is missing") == 1u);
        CHECK(world.Animations().GetController(world.Ecs, world.Strip) == nullptr);
        CHECK(world.Animator().CurrentClip == "Lift");
        REQUIRE(world.Context.Stop());
    }
    SUBCASE("a file that does not validate")
    {
        std::string invalid = STRIP;
        invalid.replace(invalid.find("Op: True"), 8, "Op: Less, Value: 1");
        LogCapture      log;
        ControllerWorld world(invalid, "Lift");
        REQUIRE(world.Context.Play());
        world.Frame(3);
        CHECK(log.Count("Less cannot test Bool parameter 'Lifted'") == 1u);
        CHECK(log.Count("anim://Strip.animctrl cannot run") == 1u);
        CHECK(world.Animator().CurrentClip == "Lift");
        REQUIRE(world.Context.Stop());
    }
}

TEST_CASE("Animator controller - Edit mode previews the default state's clip")
{
    ControllerWorld world;
    world.Ecs.GetComponent<AnimatorComponent>(world.Strip).PreviewTime = 0.5f;
    world.Context.UpdateAnimation(STEP);
    // Bend at 0.5 s: the Tip turned 67.5 degrees (Lift would have raised the root instead).
    const HM::Matrix4x4& tip = world.Animator().Palette[1];
    CHECK(Near(std::atan2(tip[0][1], tip[0][0]) * 180.0f / 3.14159265f, 67.5f));
    CHECK(Near(world.Lift(), 0.0f));
}
