#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Assets/EngineAssetDependencies.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectTemplate.hpp"

#include "EcsSerialization/api/Assets/AssetDependencyCollector.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <string>

namespace
{
    // Points the project root at a folder for one test, then puts the default back.
    struct ProjectRootOverride
    {
        explicit ProjectRootOverride(const std::filesystem::path& root) { FS::SetProjectRootDirectory(root); }
        ~ProjectRootOverride() { FS::SetProjectRootDirectory({}); }
        ProjectRootOverride(const ProjectRootOverride&)            = delete;
        ProjectRootOverride& operator=(const ProjectRootOverride&) = delete;
    };

    template <typename T>
    size_t CountWith(ECS::ECS& ecs)
    {
        size_t count = 0;
        for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
            if (ecs.IsAlive(entity) && ecs.HasComponent<T>(entity))
                ++count;
        return count;
    }
}

TEST_CASE("Project template - a project made from the Empty template loads its scene with every reference")
{
    TempDir                     dir;
    const std::filesystem::path project = dir.Path() / "Fresh";
    const std::filesystem::path templateDir = FS::GetEngineRootDirectory() / HedgehogSettings::EMPTY_TEMPLATE_DIRECTORY;
    REQUIRE(HedgehogSettings::CreateProject(templateDir, project, "Fresh").empty());

    const ProjectRootOverride        override(project);
    HedgehogEngine::EngineContext    context;
    const FS::FileSystemManager&     files = context.GetFileSystem();
    HedgehogSettings::ProjectSettings settings;
    REQUIRE(settings.Load(HedgehogSettings::ProjectSettings::PATH, files));
    CHECK(settings.GetName() == "Fresh");
    REQUIRE(context.GetSettings().Load(HedgehogSettings::Settings::PATH, files));

    // Every file the scene reaches exists: its material, the engine's cube and cells texture and
    // the game graph with its shaders.
    EcsSerialization::AssetDependencyCollector collector;
    HedgehogEngine::RegisterEngineAssetDependencies(collector);
    const EcsSerialization::AssetDependencies dependencies = collector.CollectScene(settings.GetStartupScene(), files);
    CHECK(dependencies.Warnings.empty());
    for (const std::string& warning : dependencies.Warnings)
        MESSAGE(warning);
    for (const char* asset : { "assets://Materials/Default.material", "engine://Content/Models/Default/cube.obj",
                               "engine://Content/Textures/Default/cells.png" })
        CHECK(std::find(dependencies.Assets.begin(), dependencies.Assets.end(), asset) != dependencies.Assets.end());

    // It loads: a cube, a light and a camera.
    const auto scene = files.ResolvePhysical(settings.GetStartupScene());
    REQUIRE(scene.has_value());
    REQUIRE(context.GetSceneManager().LoadScene(scene->string()));
    CHECK(CountWith<HedgehogEngine::MeshComponent>(context.GetECS()) == 1);
    CHECK(CountWith<HedgehogEngine::LightComponent>(context.GetECS()) == 1);
    CHECK(CountWith<HedgehogEngine::CameraComponent>(context.GetECS()) == 1);
}
