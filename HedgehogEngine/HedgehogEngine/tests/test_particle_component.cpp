#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"
#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
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

    // assets:// on a temp folder holding Particles/Fire.particle, an effect of two textured emitters.
    struct ParticleFiles
    {
        TempDir               Dir;
        FS::FileSystemManager Files;

        ParticleFiles()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            Files.Register(std::move(fs));
            Write("Particles/Fire.particle", MakeFire(40.0f));
        }

        static ParticleEffect MakeFire(float rate)
        {
            ParticleEffect effect = MakeDefaultParticleEffect();
            effect.Emitters[0].Name    = "Flames";
            effect.Emitters[0].Rate    = rate;
            effect.Emitters[0].Texture = "Textures/Particles/Flame.png";
            effect.Emitters.emplace_back();
            effect.Emitters[1].Name    = "Smoke";
            effect.Emitters[1].Texture = "Textures\\Particles\\Smoke.png";
            return effect;
        }

        void Write(const std::string& name, const ParticleEffect& effect) { Write(name, WriteParticleEffect(effect)); }

        // Writes the file and moves its write time on, so a reload sees it changed whatever the
        // file system's timestamp resolution.
        void Write(const std::string& name, const std::string& text)
        {
            const std::filesystem::path path = Dir.WriteFile(name, text);
            static int                  bump = 0;
            std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(++bump));
        }
    };

    using Clock = std::chrono::steady_clock;
}

TEST_CASE("Particle system component - its defaults")
{
    const ParticleSystemComponent particles;
    CHECK(particles.Enabled);
    CHECK(particles.Effect.empty());
    CHECK(particles.PlayOnStart);
    CHECK(particles.Seed == 0u);
}

TEST_CASE("Particle system component - a scene holding one round-trips byte for byte")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    SceneManager&     scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    ParticleSystemComponent particles;
    particles.Enabled     = false;
    particles.Effect      = "Particles/Fire.particle";
    particles.PlayOnStart = false;
    particles.Seed        = 42;
    ecs.AddComponent(entity, particles);

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    CHECK(snapshot.Yaml.find("ParticleSystemComponent") != std::string::npos);
    CHECK(snapshot.Yaml.find("Effect: Particles/Fire.particle") != std::string::npos);

    REQUIRE(scenes.RestoreSnapshot(snapshot));
    REQUIRE(ecs.HasComponent<ParticleSystemComponent>(entity));
    const ParticleSystemComponent& restored = ecs.GetComponent<ParticleSystemComponent>(entity);
    CHECK_FALSE(restored.Enabled);
    CHECK(restored.Effect == "Particles/Fire.particle");
    CHECK_FALSE(restored.PlayOnStart);
    CHECK(restored.Seed == 42u);
    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}

TEST_CASE("Particle effect container - one file is one effect however it is spelt")
{
    ParticleFiles           files;
    ParticleEffectContainer effects(files.Files);

    const std::optional<size_t> first = effects.FindOrLoad("Particles/Fire.particle");
    REQUIRE(first);
    CHECK(effects.FindOrLoad("assets://Particles/Fire.particle") == first);
    CHECK(effects.FindOrLoad("Particles\\./Fire.particle") == first);
    CHECK(effects.GetCount() == 1);
    CHECK(effects.GetPath(*first) == "Particles/Fire.particle");
    CHECK(effects.GetVersion(*first) == 1u);
    REQUIRE(effects.GetEffect(*first));
    CHECK(*effects.GetEffect(*first) == ParticleFiles::MakeFire(40.0f));
    CHECK_FALSE(effects.FindOrLoad(""));

    // The catalog holds one.
    EngineContext context;
    CHECK(context.GetResourceCatalog().GetParticleEffectContainer().GetCount() == 0);
}

TEST_CASE("Particle effect container - an edited file is read again once the poll interval has passed")
{
    ParticleFiles           files;
    ParticleEffectContainer effects(files.Files);
    const size_t            index = *effects.FindOrLoad("Particles/Fire.particle");

    LogCapture        log;
    const Clock::time_point start = Clock::now();
    effects.ReloadChanged(start); // takes the poll's time; nothing changed
    files.Write("Particles/Fire.particle", ParticleFiles::MakeFire(80.0f));

    // Too soon: not looked at.
    effects.ReloadChanged(start + std::chrono::milliseconds(500));
    CHECK(effects.GetEffect(index)->Emitters[0].Rate == 40.0f);

    effects.ReloadChanged(start + ParticleEffectContainer::POLL_INTERVAL);
    CHECK(effects.GetEffect(index)->Emitters[0].Rate == 80.0f);
    CHECK(effects.GetVersion(index) == 2u);
    CHECK(log.Count("[Particles] Reloaded assets://Particles/Fire.particle.") == 1);

    // Unchanged since: not read again.
    effects.ReloadChanged(start + 2 * ParticleEffectContainer::POLL_INTERVAL);
    CHECK(effects.GetVersion(index) == 2u);
    CHECK(log.Count("Reloaded") == 1);
}

TEST_CASE("Particle effect container - a broken file is logged once and the last good effect kept")
{
    ParticleFiles files;
    files.Dir.WriteFile("Particles/Broken.particle", "Version: 1\nEmitters:\n  - Rate: 3\n");
    ParticleEffectContainer effects(files.Files);

    LogCapture log;
    CHECK_FALSE(effects.FindOrLoad("Particles/Missing.particle"));
    CHECK_FALSE(effects.FindOrLoad("assets://Particles/Missing.particle"));
    CHECK(log.Count("[Particles] assets://Particles/Missing.particle cannot be read.") == 1);
    CHECK_FALSE(effects.FindOrLoad("Particles/Broken.particle"));
    CHECK_FALSE(effects.FindOrLoad("Particles/Broken.particle"));
    CHECK(log.Count("[Particles] assets://Particles/Broken.particle: Emitters[0]: Name is missing.") == 1);

    // A good effect broken by a later save keeps its last good version, logged once.
    const size_t      fire  = *effects.FindOrLoad("Particles/Fire.particle");
    const Clock::time_point start = Clock::now();
    effects.ReloadChanged(start);
    files.Write("Particles/Fire.particle", std::string("Version: 1\nEmitters: [\n"));
    effects.ReloadChanged(start + ParticleEffectContainer::POLL_INTERVAL);
    effects.ReloadChanged(start + 2 * ParticleEffectContainer::POLL_INTERVAL);
    REQUIRE(effects.GetEffect(fire));
    CHECK(effects.GetEffect(fire)->Emitters[0].Rate == 40.0f);
    CHECK(effects.GetVersion(fire) == 1u);
    CHECK(log.Count("[Particles] assets://Particles/Fire.particle: the file is not valid YAML") == 1);

    // Fixed, it is read again; a broken file that is fixed loads too.
    files.Write("Particles/Fire.particle", ParticleFiles::MakeFire(60.0f));
    files.Write("Particles/Broken.particle", MakeDefaultParticleEffect());
    effects.ReloadChanged(start + 3 * ParticleEffectContainer::POLL_INTERVAL);
    CHECK(effects.GetEffect(fire)->Emitters[0].Rate == 60.0f);
    CHECK(effects.FindOrLoad("Particles/Broken.particle").has_value());
}

TEST_CASE("Particle effects - a scene's particle system brings its effect and every emitter texture into a cook")
{
    ParticleFiles files;
    files.Dir.WriteFile("Textures/Particles/Flame.png", "png");
    files.Dir.WriteFile("Textures/Particles/Smoke.png", "png");

    // A scene of one particle system, as the engine writes it.
    {
        EngineContext     context;
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        ParticleSystemComponent particles;
        particles.Effect = "Particles/Fire.particle";
        context.GetECS().AddComponent(entity, particles);
        files.Dir.WriteFile("Scenes/Fire.yaml", context.GetSceneManager().CaptureSnapshot().Yaml);
    }

    EcsSerialization::AssetDependencyCollector collector;
    RegisterEngineAssetDependencies(collector);
    const EcsSerialization::AssetDependencies result = collector.CollectScene("assets://Scenes/Fire.yaml", files.Files);
    CHECK(result.Warnings.empty());
    CHECK(result.Assets == std::vector<std::string>{ "assets://Particles/Fire.particle", "assets://Scenes/Fire.yaml",
                                                     "assets://Textures/Particles/Flame.png",
                                                     "assets://Textures/Particles/Smoke.png" });

    // A missing texture is one warning naming the effect.
    std::filesystem::remove(files.Dir.Path() / "Textures/Particles/Smoke.png");
    const EcsSerialization::AssetDependencies missing = collector.CollectScene("assets://Scenes/Fire.yaml", files.Files);
    REQUIRE(missing.Warnings.size() == 1);
    CHECK(missing.Warnings[0].find("assets://Textures/Particles/Smoke.png (referenced by assets://Particles/Fire.particle)")
          != std::string::npos);
}
