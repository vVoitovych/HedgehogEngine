#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/tests/test_arm_gltf.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <cmath>
#include <memory>
#include <string>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnStart()\n" + onStart + "\nend\n" + "function " + name +
               ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
    }

    // The arm, scripted with script, and a Target entity for it to reach.
    struct ArmScene : EngineWorld
    {
        TempDir     ArmDir;
        ECS::Entity Arm{};
        ECS::Entity Target{};

        explicit ArmScene(const std::string& script)
        {
            ArmDir.WriteFile("Arm.gltf", ArmTest::MakeArmGltf());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("arm://", ArmDir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));

            Arm = AddScripted(script);
            Context.GetECS().AddComponent(Arm, HedgehogEngine::MeshComponent{});
            Context.GetMeshSystem()->LoadMesh(Context.GetECS(), Arm, "arm://Arm.gltf");
            Context.GetECS().AddComponent(Arm, AnimatorComponent{});
            Context.GetResourceCatalog().Update(*Context.GetRenderSystem(), *Context.GetMeshSystem());

            Target = Context.GetSceneManager().CreateGameObject();
            Context.GetECS().GetComponent<ECS::HierarchyComponent>(Target).Name = "Target";
        }

        void MoveTarget(const HM::Vector3& position)
        {
            Context.GetECS().GetComponent<TransformComponent>(Target).Position = position;
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ Target });
        }

        void Step() { Context.UpdateContext(1.0f, STEP); }

        // The arm sits at the origin unturned, so the hand's skinning matrix is in world space.
        HM::Vector3 Hand()
        {
            const std::vector<HM::Matrix4x4>& palette = Context.GetECS().GetComponent<AnimatorComponent>(Arm).Palette;
            REQUIRE(palette.size() == 3u);
            const HM::Matrix4x4& hand = palette[ArmTest::HAND];
            return HM::Vector3(hand[3].x(), hand[3].y(), hand[3].z());
        }
    };
}

TEST_CASE("IK bindings - a script reaching for a moving entity tracks it every frame")
{
    ArmScene scene(R"(Scripts/Reach.lua)");
    scene.WriteScript("Reach.lua", Script("Reach", "self.target = Scene.find('Target')",
        "self.entity:getAnimator():setIkGoal('Hand', self.target.transform.position)"));

    REQUIRE(scene.Context.Play());
    for (int frame = 0; frame < 30; ++frame)
    {
        const float       t = frame * 0.05f;
        const HM::Vector3 target(1.0f + 0.3f * std::cos(t), 0.8f + 0.3f * std::sin(t), 0.2f * t);
        scene.MoveTarget(target);
        scene.Step();
        if (frame == 0)
            continue; // the transform the script reads is applied in the frame's transform phase
        CAPTURE(frame);
        CHECK((scene.Hand() - target).LengthSlow() < 1e-3f);
    }
    REQUIRE(scene.Stop());
}

TEST_CASE("IK bindings - clearIkGoal lets go, and bad arguments are script errors")
{
    ArmScene scene(R"(Scripts/Bad.lua)");
    scene.WriteScript("Bad.lua", Script("Bad",
        "local animator = self.entity:getAnimator()\n"
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('string', function() animator:setIkGoal('Hand', 'up') end)\n"
        "try('nan', function() animator:setIkGoal('Hand', Vector3(0 / 0, 0, 0)) end)\n"
        "try('weight', function() animator:setIkGoal('Hand', Vector3(1, 1, 0), 2) end)\n"
        "try('pole', function() animator:setIkGoal('Hand', Vector3(1, 1, 0), 1, 5) end)\n"
        "animator:setIkGoal('Hand', Vector3(1, 1, 0), 1, Vector3(0, 5, 0))\n"
        "Log.info('cleared', animator:clearIkGoal('Hand'), animator:clearIkGoal('Hand'))",
        ""));

    LogCapture log;
    REQUIRE(scene.Context.Play());
    scene.Step();
    for (const char* label : { "string false", "nan false", "weight false", "pole false" })
    {
        CAPTURE(label);
        CHECK(log.Lines(label).size() == 1);
    }
    CHECK(log.Lines("an IK goal's target must be a Vector3").size() == 1);
    CHECK(log.Lines("an IK goal's target must be finite").size() == 1);
    CHECK(log.Lines("an IK goal's weight must be 0 to 1").size() == 1);
    CHECK(log.Lines("an IK goal's pole must be a Vector3").size() == 1);
    CHECK(log.Lines("cleared true false").size() == 1);
    CHECK(scene.Context.GetECS().GetComponent<AnimatorComponent>(scene.Arm).IkGoals.empty());
    REQUIRE(scene.Stop());
}
