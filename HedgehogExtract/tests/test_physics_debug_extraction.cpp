#include "doctest/doctest/doctest.h"

#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <set>
#include <tuple>

using HedgehogEngine::ColliderComponent;
using HedgehogEngine::ColliderShape;
using HedgehogEngine::RigidBodyComponent;
using HedgehogEngine::RigidBodyType;
using HedgehogEngine::TransformComponent;
using HX::SceneExtractor;

namespace
{
    constexpr float EPSILON = 1e-4f;

    // An ECS with the physics components and the physics system's entity view, without a world:
    // the extractor reads components only.
    struct PhysicsDebugFixture
    {
        ECS::ECS                                       ecs;
        std::shared_ptr<HedgehogEngine::PhysicsSystem> physics;

        PhysicsDebugFixture()
        {
            ecs.Init();
            ecs.RegisterComponent<TransformComponent>();
            ecs.RegisterComponent<ColliderComponent>();
            ecs.RegisterComponent<RigidBodyComponent>();
            physics = ecs.RegisterSystem<HedgehogEngine::PhysicsSystem>();
            ECS::Signature signature;
            signature.set(ecs.GetComponentType<ColliderComponent>());
            signature.set(ecs.GetComponentType<TransformComponent>());
            ecs.SetSystemSignature<HedgehogEngine::PhysicsSystem>(signature);
        }

        ECS::Entity Add(const HM::Matrix4x4& world, const ColliderComponent& collider)
        {
            const ECS::Entity  entity = ecs.CreateEntity();
            TransformComponent transform;
            transform.ObjMatrix = world;
            ecs.AddComponent(entity, transform);
            ecs.AddComponent(entity, collider);
            return entity;
        }

        HX::RenderScene Extract() const
        {
            HX::RenderScene scene;
            SceneExtractor{}.ExtractPhysicsDebug(ecs, *physics, scene);
            return scene;
        }
    };

    bool Near(const HM::Vector3& a, const HM::Vector3& b)
    {
        return std::abs(a.x() - b.x()) < EPSILON && std::abs(a.y() - b.y()) < EPSILON && std::abs(a.z() - b.z()) < EPSILON;
    }

    float Distance(const HM::Vector3& a, const HM::Vector3& b)
    {
        return std::sqrt((a - b).LengthSqr());
    }
}

TEST_CASE("Physics debug - a box collider gives its 12 edges at the scaled world corners")
{
    PhysicsDebugFixture fixture;
    ColliderComponent   box;
    box.Size   = HM::Vector3(2.0f, 2.0f, 2.0f);
    box.Center = HM::Vector3(0.0f, 0.0f, 1.0f);
    // Moved to (10, 0, 0), scaled (1, 2, 3), turned a quarter about Z: local X becomes world Y.
    const HM::Matrix4x4 world = HM::Matrix4x4::GetTranslation(10.0f, 0.0f, 0.0f) *
                                HM::Matrix4x4::GetRotationZ(std::numbers::pi_v<float> * 0.5f) *
                                HM::Matrix4x4::GetScale(1.0f, 2.0f, 3.0f);
    fixture.Add(world, box);

    const HX::RenderScene scene = fixture.Extract();
    REQUIRE(scene.DebugLines.size() == 24);

    // The centre is (10, 0, 3); half extents 1 along world Y, 2 along world -X, 3 along world Z.
    std::set<std::tuple<int, int, int>> corners;
    for (const HX::DebugLineVertex& vertex : scene.DebugLines)
    {
        const HM::Vector3 offset = vertex.Position - HM::Vector3(10.0f, 0.0f, 3.0f);
        CHECK(std::abs(std::abs(offset.x()) - 2.0f) < EPSILON);
        CHECK(std::abs(std::abs(offset.y()) - 1.0f) < EPSILON);
        CHECK(std::abs(std::abs(offset.z()) - 3.0f) < EPSILON);
        corners.insert({ offset.x() > 0.0f, offset.y() > 0.0f, offset.z() > 0.0f });
        CHECK(vertex.Color == SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR);
    }
    CHECK(corners.size() == 8);
    // Every edge runs along one axis.
    for (size_t i = 0; i < scene.DebugLines.size(); i += 2)
    {
        const HM::Vector3 edge = scene.DebugLines[i + 1].Position - scene.DebugLines[i].Position;
        const int moved = (std::abs(edge.x()) > EPSILON) + (std::abs(edge.y()) > EPSILON) + (std::abs(edge.z()) > EPSILON);
        CHECK(moved == 1);
    }
}

TEST_CASE("Physics debug - a sphere's points lie at its radius times its largest scale")
{
    PhysicsDebugFixture fixture;
    ColliderComponent   sphere;
    sphere.Shape  = ColliderShape::Sphere;
    sphere.Radius = 0.5f;
    fixture.Add(HM::Matrix4x4::GetTranslation(0.0f, 5.0f, 0.0f) * HM::Matrix4x4::GetScale(2.0f, 3.0f, 2.0f), sphere);

    const HX::RenderScene scene = fixture.Extract();
    REQUIRE(scene.DebugLines.size() == 3 * SceneExtractor::PHYSICS_DEBUG_CIRCLE_SEGMENTS * 2);
    float highest = -1.0f;
    for (const HX::DebugLineVertex& vertex : scene.DebugLines)
    {
        CHECK(std::abs(Distance(vertex.Position, HM::Vector3(0.0f, 5.0f, 0.0f)) - 1.5f) < EPSILON);
        highest = std::max(highest, vertex.Position.z());
    }
    CHECK(std::abs(highest - 1.5f) < EPSILON); // a great circle passes through the top
}

TEST_CASE("Physics debug - a capsule's extremes are its half height plus its radius along local Z")
{
    PhysicsDebugFixture fixture;
    ColliderComponent   capsule;
    capsule.Shape  = ColliderShape::Capsule;
    capsule.Radius = 0.5f;
    capsule.Height = 3.0f; // a cylinder of half height 1
    // Turned a quarter about X: local +Z points along world -Y.
    fixture.Add(HM::Matrix4x4::GetRotationX(std::numbers::pi_v<float> * 0.5f), capsule);

    const HX::RenderScene scene = fixture.Extract();
    // Two end circles, four side lines and four half arcs.
    const size_t segments = 2 * SceneExtractor::PHYSICS_DEBUG_CIRCLE_SEGMENTS + 4 + 2 * SceneExtractor::PHYSICS_DEBUG_CIRCLE_SEGMENTS;
    REQUIRE(scene.DebugLines.size() == 2 * segments);

    float lowY = 0.0f, highY = 0.0f;
    for (const HX::DebugLineVertex& vertex : scene.DebugLines)
    {
        const HM::Vector3& p = vertex.Position;
        lowY  = std::min(lowY, p.y());
        highY = std::max(highY, p.y());
        // Every point is within the radius of the axis segment, and on it when not on the cylinder.
        const float along = std::clamp(-p.y(), -1.0f, 1.0f);
        CHECK(std::abs(Distance(p, HM::Vector3(0.0f, -along, 0.0f)) - 0.5f) < EPSILON);
        CHECK(vertex.Color == SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR);
    }
    CHECK(std::abs(lowY + 1.5f) < EPSILON);
    CHECK(std::abs(highY - 1.5f) < EPSILON);
    const auto reaches = [&](const HM::Vector3& point)
    {
        return std::any_of(scene.DebugLines.begin(), scene.DebugLines.end(),
                           [&](const HX::DebugLineVertex& vertex) { return Near(vertex.Position, point); });
    };
    CHECK(reaches(HM::Vector3(0.0f, -1.5f, 0.0f)));
    CHECK(reaches(HM::Vector3(0.0f, 1.5f, 0.0f)));
}

TEST_CASE("Physics debug - colours follow the body kind, a trigger yellow whatever its body")
{
    PhysicsDebugFixture fixture;
    const HM::Matrix4x4 identity = HM::Matrix4x4::GetIdentity();
    const auto          withBody = [&](RigidBodyType type, bool trigger)
    {
        ColliderComponent collider;
        collider.IsTrigger      = trigger;
        const ECS::Entity entity = fixture.Add(identity, collider);
        RigidBodyComponent body;
        body.BodyType = type;
        fixture.ecs.AddComponent(entity, body);
    };
    fixture.Add(identity, ColliderComponent{});        // no rigid body: static
    withBody(RigidBodyType::Static, false);
    withBody(RigidBodyType::Dynamic, false);
    withBody(RigidBodyType::Kinematic, false);
    withBody(RigidBodyType::Dynamic, true);

    const HX::RenderScene scene = fixture.Extract();
    REQUIRE(scene.DebugLines.size() == 5 * 24);
    const uint32_t expected[] = { SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR, SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR,
                                  SceneExtractor::PHYSICS_DEBUG_DYNAMIC_COLOR, SceneExtractor::PHYSICS_DEBUG_KINEMATIC_COLOR,
                                  SceneExtractor::PHYSICS_DEBUG_TRIGGER_COLOR };
    for (size_t i = 0; i < 5; ++i)
        for (size_t v = 0; v < 24; ++v)
            CHECK(scene.DebugLines[i * 24 + v].Color == expected[i]);
}

TEST_CASE("Physics debug - a second extraction into a cleared scene reuses the lines' storage")
{
    PhysicsDebugFixture fixture;
    ColliderComponent   sphere;
    sphere.Shape = ColliderShape::Sphere;
    ColliderComponent capsule;
    capsule.Shape = ColliderShape::Capsule;
    for (int i = 0; i < 8; ++i)
    {
        const HM::Matrix4x4 world = HM::Matrix4x4::GetTranslation(static_cast<float>(i), 0.0f, 0.0f);
        fixture.Add(world, ColliderComponent{});
        fixture.Add(world, sphere);
        fixture.Add(world, capsule);
    }

    HX::RenderScene scene;
    SceneExtractor{}.ExtractPhysicsDebug(fixture.ecs, *fixture.physics, scene);
    const size_t                    count    = scene.DebugLines.size();
    const HX::DebugLineVertex*      storage  = scene.DebugLines.data();
    const size_t                    capacity = scene.DebugLines.capacity();
    REQUIRE(count > 0);

    scene.Clear();
    SceneExtractor{}.ExtractPhysicsDebug(fixture.ecs, *fixture.physics, scene);
    CHECK(scene.DebugLines.size() == count);
    CHECK(scene.DebugLines.data() == storage);
    CHECK(scene.DebugLines.capacity() == capacity);
}

TEST_CASE("Physics debug - nothing without colliders")
{
    PhysicsDebugFixture fixture;
    const ECS::Entity   entity = fixture.ecs.CreateEntity();
    fixture.ecs.AddComponent(entity, TransformComponent{});
    CHECK(fixture.Extract().DebugLines.empty());
}
