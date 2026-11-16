#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <cmath>
#include <memory>
#include <string>

using HedgehogEngine::ParticleSystemComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    // An ECS with ParticleSystem over a resource catalog whose assets:// holds Fire.particle, two
    // emitters at 60 and 120 particles a second.
    struct ParticleFixture
    {
        TempDir                                         Dir;
        FS::FileSystemManager                           Files;
        std::unique_ptr<HedgehogEngine::ResourceCatalog> Catalog;
        ECS::ECS                                        Ecs;
        std::shared_ptr<HedgehogEngine::ParticleSystem> Particles;

        ParticleFixture()
        {
            HedgehogEngine::ParticleEffect effect = HedgehogEngine::MakeDefaultParticleEffect();
            effect.Emitters[0].Rate = 60.0f;
            effect.Emitters.emplace_back();
            effect.Emitters[1].Name = "Second";
            effect.Emitters[1].Rate = 120.0f;
            Dir.WriteFile("Fire.particle", HedgehogEngine::WriteParticleEffect(effect));
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            Files.Register(std::move(fs));
            Catalog = std::make_unique<HedgehogEngine::ResourceCatalog>(Files);

            Ecs.Init();
            Ecs.GetServices().Register(*Catalog);
            Ecs.RegisterComponent<ParticleSystemComponent>();
            Ecs.RegisterComponent<TransformComponent>();
            Particles = Ecs.RegisterSystem<HedgehogEngine::ParticleSystem>();
            ECS::Signature signature;
            signature.set(Ecs.GetComponentType<ParticleSystemComponent>());
            signature.set(Ecs.GetComponentType<TransformComponent>());
            Ecs.SetSystemSignature<HedgehogEngine::ParticleSystem>(signature);
        }

        ECS::Entity Add(const std::string& effect, bool enabled, float x)
        {
            const ECS::Entity       entity = Ecs.CreateEntity();
            ParticleSystemComponent particles;
            particles.Effect  = effect;
            particles.Enabled = enabled;
            particles.Playing = true;
            Ecs.AddComponent(entity, particles);
            TransformComponent transform;
            transform.ObjMatrix = HM::Matrix4x4::GetTranslation(x, 0.0f, 0.0f);
            Ecs.AddComponent(entity, transform);
            return entity;
        }

        void Frame(ECS::PlayMode mode)
        {
            ECS::FrameContext context;
            context.DeltaTime       = 1.0f / 60.0f;
            context.ScaledDeltaTime = 1.0f / 60.0f;
            context.Mode            = mode;
            Particles->OnFrame(Ecs, context);
        }
    };
}

TEST_CASE("Particle extraction - every emitter of every enabled instance, at its entity, with what it spawns")
{
    ParticleFixture   fixture;
    const ECS::Entity first  = fixture.Add("Fire.particle", true, 1.0f);
    const ECS::Entity second = fixture.Add("assets://Fire.particle", true, 2.0f);
    fixture.Add("Fire.particle", false, 3.0f); // disabled
    fixture.Add("", true, 4.0f);               // no effect
    fixture.Add("Missing.particle", true, 5.0f);

    fixture.Frame(ECS::PlayMode::Playing);
    HX::RenderScene scene;
    HX::SceneExtractor{}.ExtractParticles(fixture.Ecs, *fixture.Particles, scene);

    REQUIRE(scene.ParticleEmitters.size() == 4);
    const auto& emitters = scene.ParticleEmitters;
    CHECK(emitters[0].InstanceId == ((uint64_t{ fixture.Ecs.GetGeneration(first) } << 32) | first));
    CHECK(emitters[0].EmitterIndex == 0);
    CHECK(emitters[0].SpawnCount == 1);  // 60 a second over 1/60 s
    CHECK(emitters[1].EmitterIndex == 1);
    CHECK(emitters[1].SpawnCount == 2);
    CHECK(emitters[2].InstanceId == ((uint64_t{ fixture.Ecs.GetGeneration(second) } << 32) | second));
    CHECK(emitters[0].EffectIndex == emitters[2].EffectIndex); // one effect, two spellings
    CHECK(emitters[0].WorldMatrix == HM::Matrix4x4::GetTranslation(1.0f, 0.0f, 0.0f));
    CHECK(emitters[2].WorldMatrix == HM::Matrix4x4::GetTranslation(2.0f, 0.0f, 0.0f));
    CHECK(emitters[0].Playing);
    CHECK(emitters[0].DeltaTime == doctest::Approx(1.0f / 60.0f));
    CHECK(emitters[0].Seed != emitters[1].Seed);
    CHECK(emitters[0].Seed != emitters[2].Seed);

    // Edit: listed, spawning nothing.
    fixture.Frame(ECS::PlayMode::Edit);
    scene.Clear();
    HX::SceneExtractor{}.ExtractParticles(fixture.Ecs, *fixture.Particles, scene);
    REQUIRE(scene.ParticleEmitters.size() == 4);
    for (const HX::ParticleEmitterInstance& emitter : scene.ParticleEmitters)
    {
        CHECK(emitter.SpawnCount == 0);
        CHECK_FALSE(emitter.Playing);
        CHECK(emitter.DeltaTime == 0.0f);
    }
}

TEST_CASE("Particle extraction - a second extraction into a cleared scene keeps the storage, and a dead entity is left out")
{
    ParticleFixture   fixture;
    fixture.Add("Fire.particle", true, 0.0f);
    const ECS::Entity doomed = fixture.Add("Fire.particle", true, 0.0f);
    fixture.Frame(ECS::PlayMode::Playing);

    HX::RenderScene scene;
    HX::SceneExtractor{}.ExtractParticles(fixture.Ecs, *fixture.Particles, scene);
    REQUIRE(scene.ParticleEmitters.size() == 4);
    const HX::ParticleEmitterInstance* storage = scene.ParticleEmitters.data();

    scene.Clear();
    CHECK(scene.ParticleEmitters.empty());
    HX::SceneExtractor{}.ExtractParticles(fixture.Ecs, *fixture.Particles, scene);
    CHECK(scene.ParticleEmitters.data() == storage);

    // Destroyed after the system listed it: not extracted.
    fixture.Ecs.DestroyEntity(doomed);
    scene.Clear();
    HX::SceneExtractor{}.ExtractParticles(fixture.Ecs, *fixture.Particles, scene);
    CHECK(scene.ParticleEmitters.size() == 2);
}

TEST_CASE("Particle extraction - the emitter shapes of an entity, as lines through its matrix")
{
    ParticleFixture fixture;
    HedgehogEngine::ParticleEffect effect = HedgehogEngine::MakeDefaultParticleEffect();
    effect.Emitters.resize(4);
    effect.Emitters[0].Shape  = HedgehogEngine::ParticleShape::Cone;
    effect.Emitters[0].Radius = 0.5f;
    effect.Emitters[0].Angle  = 45.0f;
    effect.Emitters[1].Shape  = HedgehogEngine::ParticleShape::Sphere;
    effect.Emitters[1].Radius = 2.0f;
    effect.Emitters[2].Shape   = HedgehogEngine::ParticleShape::Box;
    effect.Emitters[2].BoxSize = HM::Vector3(2.0f, 4.0f, 6.0f);
    effect.Emitters[3].Shape   = HedgehogEngine::ParticleShape::Point;
    for (size_t i = 0; i < effect.Emitters.size(); ++i)
        effect.Emitters[i].Name = "Emitter " + std::to_string(i);
    fixture.Dir.WriteFile("Shapes.particle", HedgehogEngine::WriteParticleEffect(effect));
    const ECS::Entity entity = fixture.Add("Shapes.particle", true, 10.0f);
    const ECS::Entity bare   = fixture.Ecs.CreateEntity();
    fixture.Frame(ECS::PlayMode::Edit); // resolves the effect

    const HedgehogEngine::ParticleEffectContainer& effects = fixture.Catalog->GetParticleEffectContainer();
    HX::RenderScene scene;
    HX::SceneExtractor{}.ExtractParticleShapes(fixture.Ecs, effects, bare, scene);
    CHECK(scene.DebugLines.empty());
    HX::SceneExtractor{}.ExtractParticleShapes(fixture.Ecs, effects, entity, scene);

    // A cone of two circles and four sides, a sphere of three circles, a box of 12 edges, a cross.
    constexpr size_t CIRCLE = 24 * 2;
    REQUIRE(scene.DebugLines.size() == 2 * CIRCLE + 8 + 3 * CIRCLE + 24 + 6);
    for (const HX::DebugLineVertex& vertex : scene.DebugLines)
        CHECK(vertex.Color == HX::SceneExtractor::PARTICLE_SHAPE_COLOR);
    const auto near = [](const HM::Vector3& a, const HM::Vector3& b)
    {
        return std::abs(a.x() - b.x()) < 1e-4f && std::abs(a.y() - b.y()) < 1e-4f && std::abs(a.z() - b.z()) < 1e-4f;
    };

    // The cone's first side: from its base's edge to where its 45 degrees open by 1 at z = 1.
    const size_t side = 2 * CIRCLE;
    CHECK(near(scene.DebugLines[side].Position, HM::Vector3(10.5f, 0.0f, 0.0f)));
    CHECK(near(scene.DebugLines[side + 1].Position, HM::Vector3(11.5f, 0.0f, 1.0f)));
    // Every point of the sphere at its radius from the entity.
    const size_t sphere = 2 * CIRCLE + 8;
    for (size_t i = sphere; i < sphere + 3 * CIRCLE; ++i)
    {
        const HM::Vector3 offset = scene.DebugLines[i].Position - HM::Vector3(10.0f, 0.0f, 0.0f);
        CHECK(std::sqrt(offset.LengthSqr()) == doctest::Approx(2.0f));
    }
    // Every point of the box on its corners, half its size from the entity on each axis.
    for (size_t i = sphere + 3 * CIRCLE; i < sphere + 3 * CIRCLE + 24; ++i)
    {
        const HM::Vector3& p = scene.DebugLines[i].Position;
        CHECK(std::abs(p.x() - 10.0f) == doctest::Approx(1.0f));
        CHECK(std::abs(p.y()) == doctest::Approx(2.0f));
        CHECK(std::abs(p.z()) == doctest::Approx(3.0f));
    }
    // The point's cross, along x first.
    CHECK(near(scene.DebugLines[scene.DebugLines.size() - 6].Position, HM::Vector3(9.9f, 0.0f, 0.0f)));
}
