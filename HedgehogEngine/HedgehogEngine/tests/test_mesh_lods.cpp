#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include <filesystem>
#include <memory>

using HedgehogEngine::EngineContext;
using HedgehogEngine::MeshComponent;

TEST_CASE("A loaded DamagedHelmet exposes its levels of detail through MeshView, the cube none")
{
    EngineContext     context;
    ECS::ECS&         ecs    = context.GetECS();
    const ECS::Entity helmet = context.GetSceneManager().CreateGameObject();
    ecs.AddComponent(helmet, MeshComponent{});

    auto&                                  catalog = context.GetResourceCatalog();
    const HedgehogEngine::IResourceCatalog& view   = catalog;
    const size_t                           before = catalog.GetMeshContainer().GetMeshCount();
    context.GetMeshSystem()->LoadMesh(ecs, helmet, "Models/DamagedHelmet/DamagedHelmet.gltf");
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    REQUIRE(catalog.GetMeshContainer().GetMeshCount() == before + 1);

    const HedgehogEngine::MeshView mesh = view.GetMesh(before);
    REQUIRE(mesh.lodIndices != nullptr);
    REQUIRE(mesh.lodIndices->size() >= 1);
    size_t previous = mesh.indices.size();
    for (const auto& level : *mesh.lodIndices)
    {
        CHECK(level.size() % 3 == 0);
        CHECK(level.size() < previous);
        for (uint32_t index : level)
            CHECK(index < mesh.positions.size());
        previous = level.size();
    }

    // The default cube (the first mesh the context loads) is far under the minimum.
    const HedgehogEngine::MeshView cube = view.GetMesh(0);
    CHECK(cube.lodIndices != nullptr);
    CHECK(cube.lodIndices->empty());
}

TEST_CASE("A mesh's levels of detail go with ClearData, and procedural geometry has none")
{
    const std::filesystem::path root =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path();
    FS::FileSystemManager files;
    auto                  fs = std::make_unique<FS::FileSystem>();
    fs->RegisterPath("assets://", root / "Projects/FeatureTest/Assets");
    REQUIRE(files.Register(std::move(fs)));

    HedgehogEngine::Mesh mesh;
    REQUIRE(mesh.LoadData("Models/DamagedHelmet/DamagedHelmet.gltf", files));
    CHECK_FALSE(mesh.GetLodIndices().empty());
    mesh.ClearData();
    CHECK(mesh.GetLodIndices().empty());

    REQUIRE(mesh.LoadData("Models/DamagedHelmet/DamagedHelmet.gltf", files));
    mesh.SetGeometry({ HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f) },
                     { HM::Vector2(0.0f, 0.0f), HM::Vector2(1.0f, 0.0f), HM::Vector2(0.0f, 1.0f) },
                     { HM::Vector3(0.0f, 0.0f, 1.0f), HM::Vector3(0.0f, 0.0f, 1.0f), HM::Vector3(0.0f, 0.0f, 1.0f) },
                     { HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f), HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f), HM::Vector4(1.0f, 0.0f, 0.0f, 1.0f) }, { 0, 1, 2 });
    CHECK(mesh.GetLodIndices().empty());
}
