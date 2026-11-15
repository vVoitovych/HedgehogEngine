#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>

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
