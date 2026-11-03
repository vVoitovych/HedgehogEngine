#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogCommon/api/Resource/IResourceCatalog.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/TerrainSystem.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

using HedgehogEngine::TerrainComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    // A catalog of meshes made of positions alone, each with a version a test bumps.
    class PositionsCatalog : public HedgehogEngine::IResourceCatalog
    {
    public:
        std::vector<std::vector<HM::Vector3>> Positions;
        std::vector<uint32_t>                 Versions;

        size_t GetMeshCount() const override { return Positions.size(); }
        HedgehogEngine::MeshView GetMesh(size_t index) const override
        {
            return HedgehogEngine::MeshView{ Positions[index], m_Normals, m_TexCoords, m_Tangents, m_Indices,
                                             m_Joints,         m_Weights, 0,          0,           0 };
        }
        uint32_t GetMeshVersion(size_t index) const override { return Versions[index]; }

        size_t                       GetMaterialCount() const override { return 0; }
        HedgehogEngine::MaterialView GetMaterial(size_t) const override { throw std::logic_error("no materials"); }
        void                         ClearMaterialDirty(size_t) override {}
        void                         RegisterTexturePath(const std::string&) override {}
        size_t                       GetFontCount() const override { return 0; }
        HedgehogEngine::FontAtlasView GetFontAtlas(size_t) const override { return {}; }
        const FS::FileSystemManager& GetFileSystem() const override { throw std::logic_error("no file system"); }

    private:
        std::vector<HM::Vector3>  m_Normals;
        std::vector<HM::Vector2>  m_TexCoords;
        std::vector<HM::Vector4>  m_Tangents;
        std::vector<uint32_t>     m_Indices;
        std::vector<HM::Vector4u> m_Joints;
        std::vector<HM::Vector4>  m_Weights;
    };

    struct TerrainFixture
    {
        ECS::ECS                                       ecs;
        std::shared_ptr<HedgehogEngine::TerrainSystem> terrains;

        TerrainFixture()
        {
            ecs.Init();
            ecs.RegisterComponent<TerrainComponent>();
            ecs.RegisterComponent<TransformComponent>();
            terrains = ecs.RegisterSystem<HedgehogEngine::TerrainSystem>();
            ECS::Signature signature;
            signature.set(ecs.GetComponentType<TerrainComponent>());
            signature.set(ecs.GetComponentType<TransformComponent>());
            ecs.SetSystemSignature<HedgehogEngine::TerrainSystem>(signature);
        }

        ECS::Entity Add(int32_t meshIndex, uint32_t layer, bool enabled, const HM::Vector3& position)
        {
            const ECS::Entity entity = ecs.CreateEntity();
            TerrainComponent  terrain;
            terrain.MeshIndex   = meshIndex;
            terrain.RenderLayer = layer;
            terrain.Enabled     = enabled;
            ecs.AddComponent(entity, terrain);
            TransformComponent transform;
            transform.ObjMatrix = HM::Matrix4x4::GetTranslation(position.x(), position.y(), position.z());
            ecs.AddComponent(entity, transform);
            return entity;
        }
    };
}

TEST_CASE("Mesh bounds - a mesh replaced under a new version gets new bounds")
{
    PositionsCatalog catalog;
    catalog.Positions = { { HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f) },
                          { HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(2.0f, 2.0f, 0.0f) } };
    catalog.Versions  = { 0, 0 };

    HX::MeshBoundsCache cache;
    cache.Update(catalog);
    REQUIRE(cache.GetBounds().size() == 2);
    CHECK(cache.GetBounds()[1].GetMax().z() == 0.0f);

    // Changed positions under the same version are not looked at; a new version is.
    catalog.Positions[1][1] = HM::Vector3(2.0f, 2.0f, 5.0f);
    cache.Update(catalog);
    CHECK(cache.GetBounds()[1].GetMax().z() == 0.0f);
    catalog.Versions[1] = 1;
    cache.Update(catalog);
    CHECK(cache.GetBounds()[1].GetMax().z() == 5.0f);
    CHECK(cache.GetBounds()[0].GetMax().z() == 1.0f);
}

TEST_CASE("Terrain extraction - an enabled terrain with a mesh is an instance on its render layer")
{
    TerrainFixture  fixture;
    const auto      drawn    = fixture.Add(1, 3, true, HM::Vector3(10.0f, 0.0f, 0.0f));
    fixture.ecs.GetComponent<TerrainComponent>(drawn).TerrainIndex = 4;
    fixture.ecs.GetComponent<TerrainComponent>(drawn).CastShadows  = false;
    fixture.Add(1, 0, false, HM::Vector3(0.0f, 0.0f, 0.0f));  // disabled
    fixture.Add(-1, 0, true, HM::Vector3(0.0f, 0.0f, 0.0f));  // no mesh listed
    fixture.Add(5, 0, true, HM::Vector3(0.0f, 0.0f, 0.0f));   // mesh not loaded
    const std::vector<HM::AABB> bounds = { HM::AABB(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(1.0f, 1.0f, 1.0f)),
                                           HM::AABB(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(32.0f, 32.0f, 4.0f)) };

    HX::RenderScene scene;
    HX::SceneExtractor{}.ExtractTerrains(fixture.ecs, *fixture.terrains, scene, bounds);
    REQUIRE(scene.Instances.size() == 1);
    const HX::RenderInstance& instance = scene.Instances[0];
    CHECK(instance.MeshIndex == 1);
    CHECK(instance.Layer == 3);
    CHECK(instance.SourceId == drawn);
    CHECK(instance.TerrainIndex == 4);
    CHECK_FALSE(instance.CastsShadows);
    CHECK(instance.MaterialIndex == UINT64_MAX);
    CHECK(instance.WorldBounds.GetMin().x() == doctest::Approx(10.0f));
    CHECK(instance.WorldBounds.GetMax().x() == doctest::Approx(42.0f));
    CHECK(instance.WorldBounds.GetMax().z() == doctest::Approx(4.0f));
}
