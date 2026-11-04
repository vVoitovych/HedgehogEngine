#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <memory>

using namespace HedgehogEngine;

namespace
{
    // An engine context with terrains:// on a temp folder holding Ground.terrain (33 x 33, flat),
    // and an entity drawing it.
    struct TerrainWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::Entity   Ground = ECS::INVALID_ENTITY;

        TerrainWorld()
        {
            TerrainData data     = MakeDefaultTerrain();
            data.Size            = 32.0f;
            data.Resolution      = 33;
            data.SplatResolution = 16;
            data.Heights.assign(33 * 33, 1.0f);
            data.Splat.assign(16 * 16 * 4, 0);
            Dir.WriteFile("Ground.terrain", WriteTerrain(data));

            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("terrains://", Dir.Path());
            Context.GetFileSystem().Register(std::move(fs));

            Ground = Context.GetSceneManager().CreateGameObject();
            TerrainComponent terrain;
            terrain.Terrain = "terrains://Ground.terrain";
            Context.GetECS().AddComponent(Ground, terrain);
        }

        void Frame() { Context.UpdateContext(1.0f, 1.0f / 60.0f); }

        const TerrainComponent& Component() { return Context.GetECS().GetComponent<TerrainComponent>(Ground); }
        const MeshContainer& Meshes() { return Context.GetResourceCatalog().GetMeshContainer(); }
        TerrainContainer& Terrains() { return Context.GetResourceCatalog().GetTerrainContainer(); }
    };
}

TEST_CASE("Terrain mesh - a terrain's grid is listed and built as a mesh in one frame")
{
    TerrainWorld world;
    world.Frame();

    const TerrainComponent& terrain = world.Component();
    REQUIRE(terrain.TerrainIndex == 0);
    REQUIRE(terrain.MeshIndex >= 0);
    const size_t mesh = static_cast<size_t>(terrain.MeshIndex);
    REQUIRE(mesh < world.Meshes().GetMeshCount());
    CHECK(world.Context.GetMeshSystem()->GetMeshes()[mesh] == "terrain:33:terrains://Ground.terrain");
    CHECK(world.Meshes().FindMesh("terrain:33:terrains://Ground.terrain") == mesh);

    const Mesh& grid = world.Meshes().GetMesh(mesh);
    CHECK(grid.GetPositions().size() == 33 * 33);
    CHECK(grid.GetIndices().size() == 6 * 32 * 32);
    CHECK(grid.GetIndexCount() == 6 * 32 * 32);
    CHECK(grid.GetTangents().size() == 33 * 33);
    CHECK(grid.GetSkeleton() == nullptr);
    CHECK(grid.GetPositions()[33 * 33 - 1].z() == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(grid.GetPositions()[33 * 33 - 1].x() == doctest::Approx(32.0f));

    // A second entity naming the file another way shares the mesh.
    const ECS::Entity other = world.Context.GetSceneManager().CreateGameObject();
    TerrainComponent  twin;
    twin.Terrain = "terrains://./Ground.terrain";
    world.Context.GetECS().AddComponent(other, twin);
    const size_t meshCount = world.Meshes().GetMeshCount();
    world.Frame();
    CHECK(world.Context.GetECS().GetComponent<TerrainComponent>(other).MeshIndex == terrain.MeshIndex);
    CHECK(world.Meshes().GetMeshCount() == meshCount);
}

TEST_CASE("Terrain mesh - changed heights replace the mesh in place under a new version")
{
    TerrainWorld world;
    world.Frame();
    const size_t   mesh       = static_cast<size_t>(world.Component().MeshIndex);
    const uint32_t version    = world.Meshes().GetMeshVersion(mesh);
    const uint32_t firstIndex = world.Meshes().GetMesh(mesh).GetFirstIndex();
    const uint32_t offset     = world.Meshes().GetMesh(mesh).GetVertexOffset();

    // Nothing changed: nothing is rebuilt.
    world.Frame();
    CHECK(world.Meshes().GetMeshVersion(mesh) == version);

    TerrainData& data              = world.Terrains().GetTerrain(0);
    data.Heights[16 * 33 + 16]     = 9.0f;
    world.Terrains().MarkDirty(0, TerrainDirty::Heights);
    world.Frame();

    CHECK(world.Meshes().GetMeshVersion(mesh) == version + 1);
    CHECK(world.Context.GetResourceCatalog().GetMeshVersion(mesh) == version + 1);
    const Mesh& grid = world.Meshes().GetMesh(mesh);
    CHECK(grid.GetPositions()[16 * 33 + 16].z() == doctest::Approx(9.0f));
    CHECK(grid.GetNormals()[16 * 33 + 15].x() < 0.0f); // leaning away from the peak
    CHECK(grid.GetFirstIndex() == firstIndex);
    CHECK(grid.GetVertexOffset() == offset);
    CHECK_FALSE(HasAny(world.Terrains().GetDirty(0), TerrainDirty::Heights));

    // Another resolution is a mesh of its own, listed and built the next frame; the old one stays.
    data.Resolution = 65;
    data.Heights.assign(65 * 65, 2.0f);
    world.Terrains().MarkDirty(0, TerrainDirty::Heights);
    world.Frame();
    CHECK(world.Meshes().GetMeshVersion(mesh) == version + 1);
    CHECK(world.Meshes().GetMesh(mesh).GetPositions().size() == 33 * 33);
    const int32_t resampled = world.Component().MeshIndex;
    REQUIRE(resampled >= 0);
    CHECK(resampled != static_cast<int32_t>(mesh));
    CHECK(world.Meshes().FindMesh("terrain:65:terrains://Ground.terrain") == static_cast<size_t>(resampled));
    CHECK(world.Meshes().GetMesh(static_cast<size_t>(resampled)).GetPositions().size() == 65 * 65);
    CHECK(world.Meshes().GetMesh(static_cast<size_t>(resampled)).GetPositions()[0].z() == doctest::Approx(2.0f));
    CHECK_FALSE(HasAny(world.Terrains().GetDirty(0), TerrainDirty::Heights));
}

TEST_CASE("Terrain mesh - a terrain that does not load lists no mesh")
{
    TerrainWorld world;
    world.Context.GetECS().GetComponent<TerrainComponent>(world.Ground).Terrain = "terrains://Missing.terrain";
    const size_t meshCount = world.Context.GetMeshSystem()->GetMeshes().size();
    world.Frame();
    CHECK(world.Component().TerrainIndex == -1);
    CHECK(world.Component().MeshIndex == -1);
    CHECK(world.Context.GetMeshSystem()->GetMeshes().size() == meshCount);
}

TEST_CASE("Terrain mesh - the catalog shows the renderer each terrain's splat and layers, dirty until taken")
{
    TerrainWorld world;
    world.Frame();
    world.Terrains().GetTerrain(0).Layers[2].BaseColor = "Textures/Rock.png";
    world.Terrains().GetTerrain(0).Layers[2].Tiling    = 4.0f;

    ResourceCatalog& catalog = world.Context.GetResourceCatalog();
    REQUIRE(catalog.GetTerrainCount() == 1);
    TerrainView view = catalog.GetTerrain(0);
    CHECK(view.splatResolution == 16);
    CHECK(view.splat.size() == 16 * 16 * 4);
    CHECK(view.layers[2].baseColor == "Textures/Rock.png");
    CHECK(view.layers[2].tiling == 4.0f);
    CHECK(view.splatDirty);
    CHECK(view.layersDirty);

    catalog.ClearTerrainDirty(0, true, false);
    view = catalog.GetTerrain(0);
    CHECK_FALSE(view.splatDirty);
    CHECK(view.layersDirty);
    catalog.ClearTerrainDirty(0, false, true);
    CHECK_FALSE(catalog.GetTerrain(0).layersDirty);

    world.Terrains().MarkDirty(0, TerrainDirty::Splat);
    CHECK(catalog.GetTerrain(0).splatDirty);
    CHECK_FALSE(catalog.GetTerrain(0).layersDirty);
}
