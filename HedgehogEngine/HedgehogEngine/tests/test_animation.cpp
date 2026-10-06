#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/System.hpp"

#include <cmath>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::EngineContext;
using HedgehogEngine::MeshComponent;
using HedgehogEngine::MeshSystem;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Models/Animated/TwoBoneStrip.gltf: joints Root (at the origin) and Tip (one unit up).
    // Bend turns Tip 45 -> 90 degrees about z over 1 s; Lift moves Root up 0 -> 1 over 2 s.
    const std::string SKINNED = "Models/Animated/TwoBoneStrip.gltf";

    // Logger output while alive.
    class ConsoleCapture
    {
    public:
        ConsoleCapture() : m_Previous(std::cout.rdbuf(m_Captured.rdbuf())) {}
        ~ConsoleCapture() { std::cout.rdbuf(m_Previous); }

        ConsoleCapture(const ConsoleCapture&)            = delete;
        ConsoleCapture& operator=(const ConsoleCapture&) = delete;

        size_t Count(const std::string& fragment) const
        {
            std::istringstream lines(m_Captured.str());
            std::string        line;
            size_t             count = 0;
            while (std::getline(lines, line))
                count += line.find(fragment) != std::string::npos ? 1 : 0;
            return count;
        }

    private:
        std::ostringstream m_Captured;
        std::streambuf*    m_Previous;
    };

    // Stands in for ScriptSystem: registered after the engine's systems, it sets the animator's
    // clip from its OnUpdate on the frame it is told to.
    class ClipSwitcher : public ECS::System
    {
    public:
        void OnUpdate(ECS::ECS& ecs, float) override
        {
            if (Target && !NextClip.empty())
            {
                ecs.GetComponent<AnimatorComponent>(*Target).Clip = NextClip;
                NextClip.clear();
            }
        }

        std::optional<ECS::Entity> Target;
        std::string                NextClip;
    };

    // An entity with the skinned mesh loaded into the catalog, and optionally an animator.
    ECS::Entity AddSkinned(EngineContext& context, const std::string& path, std::optional<AnimatorComponent> animator)
    {
        ECS::ECS&         ecs    = context.GetECS();
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        ecs.AddComponent(entity, MeshComponent{});
        context.GetMeshSystem()->LoadMesh(ecs, entity, path);
        if (animator)
            ecs.AddComponent(entity, *animator);
        context.GetResourceCatalog().Update(*context.GetRenderSystem(), *context.GetMeshSystem());
        return entity;
    }

    void Frame(EngineContext& context)
    {
        context.UpdatePlayMode(STEP);
        context.UpdateAnimation(STEP);
    }

    const AnimatorComponent& Animator(EngineContext& context, ECS::Entity entity)
    {
        return context.GetECS().GetComponent<AnimatorComponent>(entity);
    }

    bool Near(float a, float b) { return std::abs(a - b) < 1e-3f; }

    bool IsIdentity(const HM::Matrix4x4& matrix)
    {
        for (size_t column = 0; column < 4; ++column)
            for (size_t row = 0; row < 4; ++row)
                if (!Near(matrix[column][row], column == row ? 1.0f : 0.0f))
                    return false;
        return true;
    }

    // The y of a palette matrix's translation (Matrix4x4 stores columns).
    float LiftOf(const HM::Matrix4x4& matrix) { return matrix[3][1]; }

    // The angle about z, in degrees, a palette matrix turns +x by.
    float TurnOf(const HM::Matrix4x4& matrix)
    {
        return std::atan2(matrix[0][1], matrix[0][0]) * 180.0f / 3.14159265f;
    }

    AnimatorComponent Playing(const std::string& clip, float crossfade = 0.0f)
    {
        AnimatorComponent animator;
        animator.Clip          = clip;
        animator.CrossfadeTime = crossfade;
        return animator;
    }
}

TEST_CASE("Animation - a skinned mesh exposes its skeleton and clips through the catalog, a static mesh none")
{
    EngineContext context;
    const ECS::Entity skinned = AddSkinned(context, SKINNED, std::nullopt);
    const ECS::Entity cube    = AddSkinned(context, MeshSystem::sDefaultMeshPath, std::nullopt);

    const auto& meshes  = context.GetResourceCatalog().GetMeshContainer();
    const auto  meshOf  = [&](ECS::Entity entity) -> const HedgehogEngine::Mesh&
    { return meshes.GetMesh(*context.GetECS().GetComponent<MeshComponent>(entity).MeshIndex); };

    const HedgehogAnimation::Skeleton* skeleton = meshOf(skinned).GetSkeleton();
    REQUIRE(skeleton != nullptr);
    CHECK(skeleton->JointNames == std::vector<std::string>{ "Root", "Tip" });
    CHECK(skeleton->Parents == std::vector<int32_t>{ -1, 0 });
    const auto& clips = meshOf(skinned).GetAnimationClips();
    REQUIRE(clips.size() == 2u);
    CHECK(clips[0].Name == "Bend");
    CHECK(clips[1].Name == "Lift");
    CHECK(clips[1].Duration == doctest::Approx(2.0f));

    CHECK(meshOf(cube).GetSkeleton() == nullptr);
    CHECK(meshOf(cube).GetAnimationClips().empty());

    // The per-vertex skinning streams reach the renderer through the catalog's mesh view.
    const auto& catalog   = context.GetResourceCatalog();
    const auto  indexOf   = [&](ECS::Entity entity)
    { return static_cast<size_t>(*context.GetECS().GetComponent<MeshComponent>(entity).MeshIndex); };
    const auto  skinnedView = catalog.GetMesh(indexOf(skinned));
    CHECK(skinnedView.joints.size() == skinnedView.positions.size());
    CHECK(skinnedView.weights.size() == skinnedView.positions.size());
    const auto  cubeView = catalog.GetMesh(indexOf(cube));
    CHECK(cubeView.joints.empty());
    CHECK(cubeView.weights.empty());
}

TEST_CASE("Animation - Edit mode shows the bind pose, or the clip at a preview time; a static mesh gets no palette")
{
    EngineContext     context;
    const ECS::Entity entity = AddSkinned(context, SKINNED, Playing("Lift"));
    const ECS::Entity cube   = AddSkinned(context, MeshSystem::sDefaultMeshPath, Playing("Lift"));

    context.UpdateAnimation(STEP);
    REQUIRE(Animator(context, entity).Palette.size() == 2u);
    CHECK(IsIdentity(Animator(context, entity).Palette[0]));
    CHECK(IsIdentity(Animator(context, entity).Palette[1]));
    CHECK(Animator(context, cube).Palette.empty());

    context.GetECS().GetComponent<AnimatorComponent>(entity).PreviewTime = 1.0f;
    context.UpdateAnimation(STEP);
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));
    CHECK(Near(LiftOf(Animator(context, entity).Palette[1]), 0.5f));
}

TEST_CASE("Animation - Play advances the clip, Pause holds it and Stop puts the scene back")
{
    EngineContext     context;
    const ECS::Entity entity = AddSkinned(context, SKINNED, Playing("Lift"));

    REQUIRE(context.Play());
    Frame(context); // starts at time 0
    CHECK(IsIdentity(Animator(context, entity).Palette[0]));
    for (int frame = 0; frame < 60; ++frame)
        Frame(context);
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));

    REQUIRE(context.Pause());
    for (int frame = 0; frame < 30; ++frame)
        Frame(context);
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));
    REQUIRE(context.Resume());
    for (int frame = 0; frame < 30; ++frame)
        Frame(context);
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.75f));

    REQUIRE(context.Stop());
    CHECK_FALSE(Animator(context, entity).Started);
    context.UpdateAnimation(STEP);
    CHECK(IsIdentity(Animator(context, entity).Palette[0]));
}

TEST_CASE("Animation - a clip a system sets in OnUpdate shows in that frame's palette")
{
    EngineContext context;
    // Registered after the engine's systems, as the application registers ScriptSystem.
    const auto        switcher = context.GetECS().RegisterSystem<ClipSwitcher>();
    const ECS::Entity entity   = AddSkinned(context, SKINNED, Playing("Lift"));
    switcher->Target           = entity;

    REQUIRE(context.Play());
    for (int frame = 0; frame < 61; ++frame)
        Frame(context);
    REQUIRE(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));

    switcher->NextClip = "Bend";
    Frame(context);
    // Bend from its start: Root back at the origin and Tip turned 45 degrees.
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.0f));
    CHECK(Near(TurnOf(Animator(context, entity).Palette[1]), 45.0f));
    REQUIRE(context.Stop());
}

TEST_CASE("Animation - runs in the Animation phase, so a clip set in OnUpdate shows through UpdateContext too")
{
    EngineContext context;
    CHECK(context.GetAnimationSystem()->GetPhase() == ECS::SystemPhase::Animation);

    const auto        switcher = context.GetECS().RegisterSystem<ClipSwitcher>();
    const ECS::Entity entity   = AddSkinned(context, SKINNED, Playing("Lift"));
    switcher->Target           = entity;

    REQUIRE(context.Play());
    for (int frame = 0; frame < 61; ++frame)
        context.UpdateContext(1.0f, STEP);
    REQUIRE(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));

    switcher->NextClip = "Bend";
    context.UpdateContext(1.0f, STEP);
    CHECK(Animator(context, entity).CurrentClip == "Bend");
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.0f));
    CHECK(Near(TurnOf(Animator(context, entity).Palette[1]), 45.0f));

    // Paused through UpdateContext: the palette holds.
    REQUIRE(context.Pause());
    const float turn = TurnOf(Animator(context, entity).Palette[1]);
    for (int frame = 0; frame < 10; ++frame)
        context.UpdateContext(1.0f, STEP);
    CHECK(Near(TurnOf(Animator(context, entity).Palette[1]), turn));
    REQUIRE(context.Stop());
}

TEST_CASE("Animation - without its services the system's frame does nothing")
{
    ECS::ECS ecs;
    ecs.Init();
    ecs.RegisterComponent<AnimatorComponent>();
    auto system = ecs.RegisterSystem<HedgehogEngine::AnimationSystem>();

    ECS::FrameContext ctx;
    ctx.Mode = ECS::PlayMode::Playing;
    ecs.RunPhase(ECS::SystemPhase::Animation, ctx);
    CHECK(ecs.UnregisterSystem<HedgehogEngine::AnimationSystem>());
}

TEST_CASE("Animation - changing the clip crossfades from where the old clip was")
{
    EngineContext     context;
    const ECS::Entity entity = AddSkinned(context, SKINNED, Playing("Lift", 0.5f));

    REQUIRE(context.Play());
    for (int frame = 0; frame < 61; ++frame)
        Frame(context);
    context.GetECS().GetComponent<AnimatorComponent>(entity).Clip = "Bend";
    Frame(context); // the switch: weight 0, all Lift
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.5f));

    for (int frame = 0; frame < 15; ++frame)
        Frame(context);
    // Halfway through the fade: Lift (at 1.25 s, 0.625 up) and Bend (no lift) blend evenly.
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.3125f));

    for (int frame = 0; frame < 30; ++frame)
        Frame(context);
    CHECK(Animator(context, entity).PreviousClip.empty());
    CHECK(Near(LiftOf(Animator(context, entity).Palette[0]), 0.0f));
    REQUIRE(context.Stop());
}

TEST_CASE("Animation - an unknown clip shows the bind pose and warns once")
{
    EngineContext     context;
    const ECS::Entity entity = AddSkinned(context, SKINNED, Playing("Dance"));

    ConsoleCapture console;
    REQUIRE(context.Play());
    for (int frame = 0; frame < 10; ++frame)
        Frame(context);
    CHECK(console.Count("has no clip 'Dance'") == 1u);
    CHECK(IsIdentity(Animator(context, entity).Palette[0]));
    REQUIRE(context.Stop());
}
