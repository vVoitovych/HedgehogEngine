#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Containers/Mesh.hpp"
#include "HedgehogEngine/api/Containers/MeshContainer.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"
#include "HedgehogEngine/api/Navigation/NavigationGeometry.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Terrain/TerrainData.hpp"

#include "HedgehogSettings/api/LayerSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <set>
#include <tuple>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    // A context with terrains:// on a temp folder holding Flat.terrain (32 m, 33 x 33, at height 3).
    struct NavigationScene
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();

        NavigationScene()
        {
            TerrainData flat     = MakeDefaultTerrain();
            flat.Size            = 32.0f;
            flat.Resolution      = 33;
            flat.SplatResolution = 16;
            flat.Heights.assign(33 * 33, 3.0f);
            flat.Splat.assign(16 * 16 * 4, 0);
            Dir.WriteFile("Flat.terrain", WriteTerrain(flat));
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("terrains://", Dir.Path());
            Context.GetFileSystem().Register(std::move(fs));
        }

        ECS::Entity Object(const HM::Vector3& position, const HM::Vector3& rotation = {}, const HM::Vector3& scale = HM::Vector3(1.0f, 1.0f, 1.0f))
        {
            const ECS::Entity entity  = Scenes.CreateGameObject();
            auto&             transform = Ecs.GetComponent<TransformComponent>(entity);
            transform.Position          = position;
            transform.Rotation          = rotation;
            transform.Scale             = scale;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            return entity;
        }

        ECS::Entity Box(const HM::Vector3& position, RigidBodyType* body = nullptr, bool trigger = false)
        {
            const ECS::Entity entity = Object(position);
            ColliderComponent collider;
            collider.IsTrigger = trigger;
            Ecs.AddComponent(entity, collider);
            if (body)
            {
                RigidBodyComponent rigid;
                rigid.BodyType = *body;
                Ecs.AddComponent(entity, rigid);
            }
            return entity;
        }

        HN::NavMeshGeometry Gather(bool renderMeshes = false)
        {
            Context.UpdateContext(1.0f, FRAME); // resolves terrains and meshes, computes the matrices
            return GatherNavigationGeometry(Ecs, Context.GetResourceCatalog(), { renderMeshes });
        }
    };

    bool Near(float a, float b) { return std::abs(a - b) < 1e-4f; }
}

TEST_CASE("Navigation geometry - static colliders and terrains, not moving bodies or triggers")
{
    NavigationScene scene;
    // A 2 m box collider, scaled 2 along its y and turned a quarter about z, at (2, 3, 1): its world
    // corners at x 0 or 4, y 2 or 4, z 0 or 2.
    const ECS::Entity box = scene.Object(HM::Vector3(2.0f, 3.0f, 1.0f), HM::Vector3(0.0f, 0.0f, 90.0f), HM::Vector3(1.0f, 2.0f, 1.0f));
    scene.Ecs.AddComponent(box, ColliderComponent{});

    RigidBodyType dynamic = RigidBodyType::Dynamic;
    scene.Box(HM::Vector3(-20.0f, 0.0f, 5.0f), &dynamic);
    scene.Box(HM::Vector3(-30.0f, 0.0f, 5.0f), nullptr, true); // a trigger

    const ECS::Entity terrain = scene.Object(HM::Vector3(100.0f, 0.0f, 0.0f));
    TerrainComponent  tile;
    tile.Terrain = "terrains://Flat.terrain";
    scene.Ecs.AddComponent(terrain, tile);

    const HN::NavMeshGeometry geometry = scene.Gather();
    constexpr size_t TERRAIN_TRIANGLES = 32 * 32 * 2;
    REQUIRE(geometry.Indices.size() == (12 + TERRAIN_TRIANGLES) * 3);
    REQUIRE(geometry.Positions.size() == 8 + 33 * 33);

    // The box first: its 8 corners, every one of the 8 combinations once.
    std::set<std::tuple<int, int, int>> corners;
    for (size_t i = 0; i < 8; ++i)
    {
        const HM::Vector3& p = geometry.Positions[i];
        CAPTURE(i);
        CHECK((Near(p.x(), 0.0f) || Near(p.x(), 4.0f)));
        CHECK((Near(p.y(), 2.0f) || Near(p.y(), 4.0f)));
        CHECK((Near(p.z(), 0.0f) || Near(p.z(), 2.0f)));
        corners.emplace(static_cast<int>(std::lround(p.x())), static_cast<int>(std::lround(p.y())), static_cast<int>(std::lround(p.z())));
    }
    CHECK(corners.size() == 8);

    // The box's top triangles face up, so Recast walks on them.
    for (size_t t = 0; t < 36; t += 3)
    {
        const HM::Vector3& a = geometry.Positions[geometry.Indices[t]];
        const HM::Vector3& b = geometry.Positions[geometry.Indices[t + 1]];
        const HM::Vector3& c = geometry.Positions[geometry.Indices[t + 2]];
        if (!(Near(a.z(), 2.0f) && Near(b.z(), 2.0f) && Near(c.z(), 2.0f)))
            continue;
        const HM::Vector3 ab = b - a;
        const HM::Vector3 ac = c - a;
        CHECK(ab.x() * ac.y() - ab.y() * ac.x() > 0.0f);
    }

    // Then the terrain, at its entity: x from 100 to 132, all at height 3.
    for (size_t i = 8; i < geometry.Positions.size(); ++i)
    {
        CHECK(geometry.Positions[i].x() >= 100.0f - 1e-3f);
        CHECK(geometry.Positions[i].x() <= 132.0f + 1e-3f);
        CHECK(Near(geometry.Positions[i].z(), 3.0f));
    }
    for (uint32_t index : geometry.Indices)
        CHECK(index < geometry.Positions.size());
}

TEST_CASE("Navigation geometry - spheres and capsules are tessellated around their centre")
{
    NavigationScene   scene;
    const ECS::Entity ball = scene.Object(HM::Vector3(5.0f, 0.0f, 2.0f), {}, HM::Vector3(2.0f, 1.0f, 1.0f));
    ColliderComponent sphere;
    sphere.Shape  = ColliderShape::Sphere;
    sphere.Radius = 0.5f;
    scene.Ecs.AddComponent(ball, sphere);

    const ECS::Entity pill = scene.Object(HM::Vector3(-5.0f, 0.0f, 2.0f));
    ColliderComponent capsule;
    capsule.Shape  = ColliderShape::Capsule;
    capsule.Radius = 0.5f;
    capsule.Height = 3.0f;
    scene.Ecs.AddComponent(pill, capsule);

    const HN::NavMeshGeometry geometry = scene.Gather();
    REQUIRE_FALSE(geometry.Positions.empty());
    float capsuleTop = -1e9f, capsuleBottom = 1e9f;
    for (const HM::Vector3& p : geometry.Positions)
    {
        if (p.x() > 0.0f) // the sphere: radius 0.5 times the largest scale, 2
            CHECK((p - HM::Vector3(5.0f, 0.0f, 2.0f)).LengthSlow() == doctest::Approx(1.0f).epsilon(1e-3));
        else
        {
            capsuleTop    = std::max(capsuleTop, p.z());
            capsuleBottom = std::min(capsuleBottom, p.z());
        }
    }
    // The capsule spans its 3 m height, end to end, along +z.
    CHECK(capsuleTop == doctest::Approx(3.5f));
    CHECK(capsuleBottom == doctest::Approx(0.5f));
}

TEST_CASE("Navigation geometry - render meshes only when asked, never moving ones or the editor layer")
{
    NavigationScene   scene;
    const ECS::Entity statue = scene.Object(HM::Vector3(10.0f, 0.0f, 0.0f));
    scene.Ecs.AddComponent(statue, MeshComponent{});
    scene.Context.GetMeshSystem()->LoadMesh(scene.Ecs, statue, "engine://Content/Models/Default/cube.obj");

    RigidBodyType     kinematic = RigidBodyType::Kinematic;
    const ECS::Entity mover     = scene.Object(HM::Vector3(20.0f, 0.0f, 0.0f));
    scene.Ecs.AddComponent(mover, MeshComponent{});
    scene.Context.GetMeshSystem()->LoadMesh(scene.Ecs, mover, "engine://Content/Models/Default/cube.obj");
    RigidBodyComponent rigid;
    rigid.BodyType = kinematic;
    scene.Ecs.AddComponent(mover, rigid);

    // A static collider on the editor layer gives nothing either.
    const ECS::Entity gizmo = scene.Box(HM::Vector3(0.0f, 0.0f, 0.0f));
    RenderComponent   render;
    render.Layer = HedgehogSettings::LayerSettings::EDITOR_LAYER;
    scene.Ecs.AddComponent(gizmo, render);

    CHECK(scene.Gather(false).Indices.empty());

    const HN::NavMeshGeometry withMeshes = scene.Gather(true);
    const auto&               component  = scene.Ecs.GetComponent<MeshComponent>(statue);
    REQUIRE(component.MeshIndex.has_value());
    const Mesh& cube = scene.Context.GetResourceCatalog().GetMeshContainer().GetMesh(static_cast<size_t>(*component.MeshIndex));
    CHECK(withMeshes.Indices.size() == cube.GetIndices().size()); // the statue alone
    for (const HM::Vector3& p : withMeshes.Positions)
        CHECK(p.x() == doctest::Approx(10.0f).epsilon(0.2)); // the cube at x 10, 2 m across
}

TEST_CASE("Navmesh files round-trip and refuse what is not one, naming why")
{
    HN::NavMeshBuildSettings settings;
    settings.CellSize     = 0.25f;
    settings.AgentRadius  = 0.6f;
    settings.VertsPerPoly = 5;
    const std::vector<uint8_t> data   = { 1, 2, 3, 4, 5, 250 };
    const std::vector<uint8_t> file   = WriteNavMeshFile(settings, data);
    const NavMeshFileReadResult read  = ReadNavMeshFile(file);
    REQUIRE(read.Error.empty());
    CHECK(read.Data == data);
    CHECK(read.Settings.CellSize == 0.25f);
    CHECK(read.Settings.AgentRadius == 0.6f);
    CHECK(read.Settings.VertsPerPoly == 5);
    CHECK(WriteNavMeshFile(read.Settings, read.Data) == file);

    const auto refused = [](std::vector<uint8_t> bytes, const char* reason)
    {
        CAPTURE(reason);
        const NavMeshFileReadResult result = ReadNavMeshFile(bytes);
        CHECK(result.Data.empty());
        CHECK(result.Error.find(reason) != std::string::npos);
    };
    std::vector<uint8_t> magic = file;
    magic[0] = 'X';
    refused(magic, "no HNAV magic");
    std::vector<uint8_t> version = file;
    version[4] = 2;
    refused(version, "version 2");
    refused(std::vector<uint8_t>(file.begin(), file.begin() + 20), "ends inside its build settings");
    refused(std::vector<uint8_t>(file.begin(), file.end() - 2), "2 bytes before its navmesh data");
    std::vector<uint8_t> trailing = file;
    trailing.push_back(0);
    refused(trailing, "1 bytes past its navmesh data");
}
