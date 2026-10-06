#include "doctest/doctest/doctest.h"

#include "Plugins/Spinner/SpinnerComponent.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Plugins/PluginManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr const char* SCENE      = "assets://Scenes/Spinner.yaml";
    constexpr float       FRAME_TIME = 1.0f / 60.0f;

    // Collects warning lines that mention text.
    struct WarningLog
    {
        std::string              Text;
        std::vector<std::string> Lines;
        int                      Sink = EngineLogger::Logger::Instance().AddSink(
            [this](EngineLogger::LogLevel level, const std::string& message)
            {
                if (level == EngineLogger::LogLevel::Warning && message.find(Text) != std::string::npos)
                    Lines.push_back(message);
            });

        explicit WarningLog(std::string text)
            : Text(std::move(text))
        {
        }
        WarningLog(const WarningLog&)            = delete;
        WarningLog& operator=(const WarningLog&) = delete;
        ~WarningLog() { EngineLogger::Logger::Instance().RemoveSink(Sink); }
    };

    bool LoadSpinnerScene(EngineContext& context)
    {
        const std::optional<std::filesystem::path> path = context.GetFileSystem().ResolvePhysical(SCENE);
        return path && context.GetSceneManager().LoadScene(path->string());
    }

    ECS::Entity FindByName(EngineContext& context, const std::string& name)
    {
        ECS::ECS& ecs = context.GetECS();
        for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
        {
            if (ecs.IsAlive(entity) && ecs.HasComponent<ECS::HierarchyComponent>(entity) &&
                ecs.GetComponent<ECS::HierarchyComponent>(entity).Name == name)
                return entity;
        }
        return ECS::INVALID_ENTITY;
    }

    HM::Vector3 RotationOf(EngineContext& context, ECS::Entity entity)
    {
        return context.GetECS().GetComponent<TransformComponent>(entity).Rotation;
    }

    void RunFrames(EngineContext& context, int frames)
    {
        for (int i = 0; i < frames; ++i)
            context.UpdateContext(1.0f, FRAME_TIME);
    }

    void CheckRotation(const HM::Vector3& rotation, float x, float y, float z)
    {
        CHECK(rotation.x() == doctest::Approx(x).epsilon(1e-3));
        CHECK(rotation.y() == doctest::Approx(y).epsilon(1e-3));
        CHECK(rotation.z() == doctest::Approx(z).epsilon(1e-3));
    }
}

TEST_CASE("Spinner - loaded, its component is registered under Samples and the scene reads its values")
{
    EngineContext context;
    REQUIRE(context.GetPlugins().Load("Spinner"));
    const EcsSerialization::ComponentInfo* info = context.GetComponentTypes().Find("SpinnerComponent");
    REQUIRE(info != nullptr);
    CHECK(info->DisplayName == "Spinner");
    CHECK(info->Category == "Samples");
    CHECK(info->EnabledProperty == "Enabled");
    CHECK(info->Addable);

    REQUIRE(LoadSpinnerScene(context));
    const ECS::Entity spinning = FindByName(context, "SpinningCube");
    REQUIRE(spinning != ECS::INVALID_ENTITY);
    const auto& spinner = context.GetECS().GetComponent<Spinner::SpinnerComponent>(spinning);
    CHECK(spinner.Enabled);
    CHECK(spinner.Speed == doctest::Approx(45.0f));
    CHECK(spinner.Axis.y() == doctest::Approx(1.0f));
    CHECK_FALSE(context.GetECS().HasComponent<Spinner::SpinnerComponent>(FindByName(context, "StillCube")));

    // Saved and read back, the scene is the same text.
    const SceneSnapshot saved = context.GetSceneManager().CaptureSnapshot();
    CHECK(saved.Yaml.find("SpinnerComponent:\n          Enabled: true\n          Speed: 45\n          Axis: [0, 1, 0]") !=
          std::string::npos);
    REQUIRE(context.GetSceneManager().RestoreSnapshot(saved));
    CHECK(context.GetSceneManager().CaptureSnapshot().Yaml == saved.Yaml);

    // The shipped file is what the engine writes, but for the Version line shipped scenes leave out.
    std::ifstream     file(*context.GetFileSystem().ResolvePhysical(SCENE), std::ios::binary);
    std::string       text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::erase(text, '\r'); // a checkout may give the file CRLF line endings
    CHECK("Version: 1\n" + text == saved.Yaml);
}

TEST_CASE("Spinner - in Play the cube turns by Speed degrees a second; Edit and Paused hold it, Stop restores it")
{
    EngineContext context;
    REQUIRE(context.GetPlugins().Load("Spinner"));
    REQUIRE(LoadSpinnerScene(context));
    const ECS::Entity spinning = FindByName(context, "SpinningCube");
    const ECS::Entity still    = FindByName(context, "StillCube");

    RunFrames(context, 30);
    CheckRotation(RotationOf(context, spinning), 0.0f, 0.0f, 0.0f);

    REQUIRE(context.Play());
    RunFrames(context, 60);
    CheckRotation(RotationOf(context, spinning), 0.0f, 45.0f, 0.0f);
    CheckRotation(RotationOf(context, still), 0.0f, 0.0f, 0.0f);
    // The world matrix follows the same frame: the cube's +X axis turned 45 degrees about Y.
    const HM::Matrix4x4& world = context.GetECS().GetComponent<TransformComponent>(spinning).ObjMatrix;
    const HM::Vector4    xAxis = world * HM::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    CHECK(xAxis.x() == doctest::Approx(0.70710678f).epsilon(1e-3));
    CHECK(std::abs(xAxis.z()) == doctest::Approx(0.70710678f).epsilon(1e-3));

    REQUIRE(context.Pause());
    RunFrames(context, 30);
    CheckRotation(RotationOf(context, spinning), 0.0f, 45.0f, 0.0f);

    REQUIRE(context.Resume());
    RunFrames(context, 30);
    CheckRotation(RotationOf(context, spinning), 0.0f, 67.5f, 0.0f);

    REQUIRE(context.Stop());
    CheckRotation(RotationOf(context, FindByName(context, "SpinningCube")), 0.0f, 0.0f, 0.0f);
}

TEST_CASE("Spinner - unloaded, the scene still loads and keeps its data; loaded again, the cube spins")
{
    EngineContext context;
    {
        WarningLog warnings("SpinnerComponent");
        REQUIRE(LoadSpinnerScene(context));
        CHECK(warnings.Lines.size() == 1);
    }
    CHECK(context.GetComponentTypes().Find("SpinnerComponent") == nullptr);
    const std::string unloaded = context.GetSceneManager().CaptureSnapshot().Yaml;
    CHECK(unloaded.find("SpinnerComponent:") != std::string::npos);
    CHECK(unloaded.find("Speed: 45") != std::string::npos);

    // Without the plugin, Play turns nothing.
    REQUIRE(context.Play());
    RunFrames(context, 30);
    CheckRotation(RotationOf(context, FindByName(context, "SpinningCube")), 0.0f, 0.0f, 0.0f);
    REQUIRE(context.Stop());

    REQUIRE(context.GetPlugins().Load("Spinner"));
    const ECS::Entity spinning = FindByName(context, "SpinningCube");
    REQUIRE(context.GetECS().HasComponent<Spinner::SpinnerComponent>(spinning));
    CHECK(context.GetSceneManager().CaptureSnapshot().Yaml == unloaded);

    REQUIRE(context.Play());
    RunFrames(context, 60);
    CheckRotation(RotationOf(context, spinning), 0.0f, 45.0f, 0.0f);
    REQUIRE(context.Stop());

    // Unloading again keeps the values in the scene.
    REQUIRE(context.GetPlugins().Unload("Spinner"));
    CHECK(context.GetSceneManager().CaptureSnapshot().Yaml == unloaded);
}
