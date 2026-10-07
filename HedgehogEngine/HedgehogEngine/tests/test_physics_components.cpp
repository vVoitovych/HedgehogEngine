#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <string>

using namespace HedgehogEngine;

TEST_CASE("Physics components - their defaults")
{
    const ColliderComponent collider;
    CHECK(collider.Shape == ColliderShape::Box);
    CHECK(collider.Size.x() == 2.0f);
    CHECK(collider.Radius == 0.5f);
    CHECK(collider.Height == 2.0f);
    CHECK_FALSE(collider.IsTrigger);
    CHECK(collider.Layer == 0);
    CHECK(collider.Friction == 0.5f);
    CHECK(collider.Restitution == 0.0f);

    const RigidBodyComponent body;
    CHECK(body.BodyType == RigidBodyType::Dynamic);
    CHECK(body.Mass == 1.0f);
    CHECK(body.LinearDamping == 0.05f);
    CHECK(body.AngularDamping == 0.05f);
    CHECK(body.GravityScale == 1.0f);
    CHECK_FALSE(body.Body.IsSet());
}

TEST_CASE("Physics components - a scene with both round-trips byte for byte, its runtime body never written")
{
    EngineContext context;
    ECS::ECS&     ecs    = context.GetECS();
    SceneManager& scenes = context.GetSceneManager();
    const ECS::Entity entity = scenes.CreateGameObject();

    ColliderComponent collider;
    collider.Shape       = ColliderShape::Capsule;
    collider.Center      = HM::Vector3(0.0f, 0.5f, 1.0f);
    collider.Size        = HM::Vector3(1.0f, 3.0f, 0.25f);
    collider.Radius      = 0.75f;
    collider.Height      = 4.0f;
    collider.IsTrigger   = true;
    collider.Layer       = 7;
    collider.Friction    = 0.2f;
    collider.Restitution = 0.9f;
    ecs.AddComponent(entity, collider);

    RigidBodyComponent body;
    body.BodyType       = RigidBodyType::Kinematic;
    body.Mass           = 12.5f;
    body.LinearDamping  = 0.5f;
    body.AngularDamping = 0.25f;
    body.GravityScale   = 0.0f;
    body.Body           = HP::BodyHandle{ 1234 };
    body.BodyGeneration = 9;
    ecs.AddComponent(entity, body);

    const SceneSnapshot snapshot = scenes.CaptureSnapshot();
    for (const char* key : { "ColliderComponent", "RigidBodyComponent", "Shape", "Center", "Size", "Radius", "Height",
                             "IsTrigger", "Layer", "Friction", "Restitution", "BodyType", "Mass", "LinearDamping",
                             "AngularDamping", "GravityScale" })
    {
        CAPTURE(key);
        CHECK(snapshot.Yaml.find(key) != std::string::npos);
    }
    CHECK(snapshot.Yaml.find("BodyGeneration") == std::string::npos);
    CHECK(snapshot.Yaml.find("1234") == std::string::npos);

    REQUIRE(scenes.RestoreSnapshot(snapshot));
    REQUIRE(ecs.HasComponent<ColliderComponent>(entity));
    REQUIRE(ecs.HasComponent<RigidBodyComponent>(entity));
    const ColliderComponent&  restoredCollider = ecs.GetComponent<ColliderComponent>(entity);
    const RigidBodyComponent& restoredBody     = ecs.GetComponent<RigidBodyComponent>(entity);
    CHECK(restoredCollider.Shape == ColliderShape::Capsule);
    CHECK(restoredCollider.Center.z() == 1.0f);
    CHECK(restoredCollider.Size.y() == 3.0f);
    CHECK(restoredCollider.Radius == 0.75f);
    CHECK(restoredCollider.Height == 4.0f);
    CHECK(restoredCollider.IsTrigger);
    CHECK(restoredCollider.Layer == 7);
    CHECK(restoredCollider.Friction == doctest::Approx(0.2f));
    CHECK(restoredCollider.Restitution == doctest::Approx(0.9f));
    CHECK(restoredBody.BodyType == RigidBodyType::Kinematic);
    CHECK(restoredBody.Mass == 12.5f);
    CHECK(restoredBody.LinearDamping == 0.5f);
    CHECK(restoredBody.AngularDamping == 0.25f);
    CHECK(restoredBody.GravityScale == 0.0f);
    CHECK_FALSE(restoredBody.Body.IsSet());
    CHECK(restoredBody.BodyGeneration == 0);

    CHECK(scenes.CaptureSnapshot().Yaml == snapshot.Yaml);
}
