#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::ColliderComponent;
        using HedgehogEngine::ColliderShape;
        using HedgehogEngine::RigidBodyComponent;
        using HedgehogEngine::RigidBodyType;
        using RigidBodyRef = ScriptComponentRef<RigidBodyComponent>;
        using ColliderRef  = ScriptComponentRef<ColliderComponent>;

        constexpr int32_t LAYER_COUNT = 16;

        float RequireFinite(float value, const char* what)
        {
            if (!std::isfinite(value))
                throw std::runtime_error(std::string(what) + " must be a finite number");
            return value;
        }

        const HM::Vector3& RequireFinite(const HM::Vector3& value, const char* what)
        {
            if (!std::isfinite(value.x()) || !std::isfinite(value.y()) || !std::isfinite(value.z()))
                throw std::runtime_error(std::string(what) + " must be a finite Vector3");
            return value;
        }

        // Physics runs only in Play mode (a script's top level also runs when the editor describes
        // it): outside it a call does nothing and the first one logs why.
        struct PlayGuard
        {
            HedgehogEngine::EngineContext& Context;
            bool                           Warned = false;

            bool Allows(const char* call)
            {
                if (Context.GetPlayState() != HedgehogEngine::PlayState::Edit)
                    return true;
                if (!Warned)
                    LOGWARNING("[Script] Physics runs only in Play mode; " + std::string(call) + " did nothing.");
                Warned = true;
                return false;
            }
        };

        // A field written to the component; during Play the entity's body is made again from it, so
        // the change applies at once. check validates a value before it is stored.
        template<typename T, typename V, typename Check>
        auto BodyField(HedgehogEngine::EngineContext& context, V T::*member, Check check)
        {
            ECS::ECS&                      ecs     = context.GetECS();
            HedgehogEngine::PhysicsSystem& physics = *context.GetPhysicsSystem();
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return Resolve(ecs, ref).*member; },
                [&ecs, &physics, member, check](const ScriptComponentRef<T>& ref, const V& value)
                {
                    T& component = Resolve(ecs, ref);
                    component.*member = check(value);
                    physics.RebuildBody(ecs, ref.Entity.Id);
                });
        }

        template<typename T, typename E>
        auto EnumBodyField(HedgehogEngine::EngineContext& context, E T::*member, int count, const char* enumName)
        {
            ECS::ECS&                      ecs     = context.GetECS();
            HedgehogEngine::PhysicsSystem& physics = *context.GetPhysicsSystem();
            return sol::property(
                [&ecs, member](const ScriptComponentRef<T>& ref) { return static_cast<int>(Resolve(ecs, ref).*member); },
                [&ecs, &physics, member, count, enumName](const ScriptComponentRef<T>& ref, int value)
                {
                    if (value < 0 || value >= count)
                        throw std::runtime_error(std::to_string(value) + " is not a " + enumName);
                    Resolve(ecs, ref).*member = static_cast<E>(value);
                    physics.RebuildBody(ecs, ref.Entity.Id);
                });
        }

        auto Finite(const char* what)
        {
            return [what](float value) { return RequireFinite(value, what); };
        }

        auto Positive(const char* what)
        {
            return [what](float value)
            {
                if (!(RequireFinite(value, what) > 0.0f))
                    throw std::runtime_error(std::string(what) + " must be above 0");
                return value;
            };
        }

        auto NotNegative(const char* what)
        {
            return [what](float value)
            {
                if (RequireFinite(value, what) < 0.0f)
                    throw std::runtime_error(std::string(what) + " must be 0 or more");
                return value;
            };
        }

        auto FiniteVector(const char* what)
        {
            return [what](const HM::Vector3& value) { return RequireFinite(value, what); };
        }

        void RegisterEnums(sol::state& lua)
        {
            lua.new_enum("RigidBodyType",
                         "Static",    static_cast<int>(RigidBodyType::Static),
                         "Kinematic", static_cast<int>(RigidBodyType::Kinematic),
                         "Dynamic",   static_cast<int>(RigidBodyType::Dynamic));
            lua.new_enum("ColliderShape",
                         "Box",     static_cast<int>(ColliderShape::Box),
                         "Sphere",  static_cast<int>(ColliderShape::Sphere),
                         "Capsule", static_cast<int>(ColliderShape::Capsule));
        }

        void RegisterRigidBody(sol::state& lua, HedgehogEngine::EngineContext& context, const std::shared_ptr<PlayGuard>& guard)
        {
            ECS::ECS&                      ecs     = context.GetECS();
            HedgehogEngine::PhysicsSystem& physics = *context.GetPhysicsSystem();

            // A push on the body, refused outside Play and for a vector that is not finite.
            const auto push = [&ecs, &physics, guard](const char* call, void (HedgehogEngine::PhysicsSystem::*apply)(ECS::Entity, const HM::Vector3&))
            {
                return [&ecs, &physics, guard, call, apply](const RigidBodyRef& ref, const HM::Vector3& value)
                {
                    (void)Resolve(ecs, ref);
                    RequireFinite(value, "a vector");
                    if (guard->Allows(call))
                        (physics.*apply)(ref.Entity.Id, value);
                };
            };

            lua.new_usertype<RigidBodyRef>(
                "RigidBody", sol::no_constructor,
                "bodyType",       EnumBodyField(context, &RigidBodyComponent::BodyType, 3, "RigidBodyType"),
                "mass",           BodyField(context, &RigidBodyComponent::Mass, Positive("a mass")),
                "linearDamping",  BodyField(context, &RigidBodyComponent::LinearDamping, NotNegative("a damping")),
                "angularDamping", BodyField(context, &RigidBodyComponent::AngularDamping, NotNegative("a damping")),
                "gravityScale",   BodyField(context, &RigidBodyComponent::GravityScale, Finite("a gravity scale")),
                // World space, per second; zero outside Play.
                "velocity", sol::property(
                    [&ecs, &physics](const RigidBodyRef& ref) { (void)Resolve(ecs, ref); return physics.GetLinearVelocity(ref.Entity.Id); },
                    [&ecs, &physics, guard](const RigidBodyRef& ref, const HM::Vector3& value)
                    {
                        (void)Resolve(ecs, ref);
                        RequireFinite(value, "a velocity");
                        if (guard->Allows("RigidBody.velocity"))
                            physics.SetLinearVelocity(ref.Entity.Id, value);
                    }),
                "angularVelocity", sol::property(
                    [&ecs, &physics](const RigidBodyRef& ref) { (void)Resolve(ecs, ref); return physics.GetAngularVelocity(ref.Entity.Id); },
                    [&ecs, &physics, guard](const RigidBodyRef& ref, const HM::Vector3& value)
                    {
                        (void)Resolve(ecs, ref);
                        RequireFinite(value, "an angular velocity");
                        if (guard->Allows("RigidBody.angularVelocity"))
                            physics.SetAngularVelocity(ref.Entity.Id, value);
                    }),
                // Forces and torques act over the next step: add them in OnFixedUpdate, every step
                // they should last. Impulses change the velocity at once.
                "addForce",          push("RigidBody:addForce()", &HedgehogEngine::PhysicsSystem::AddForce),
                "addImpulse",        push("RigidBody:addImpulse()", &HedgehogEngine::PhysicsSystem::AddImpulse),
                "addTorque",         push("RigidBody:addTorque()", &HedgehogEngine::PhysicsSystem::AddTorque),
                "addAngularImpulse", push("RigidBody:addAngularImpulse()", &HedgehogEngine::PhysicsSystem::AddAngularImpulse),
                sol::meta_function::to_string, ToText<RigidBodyComponent>("RigidBody"));
        }

        void RegisterCollider(sol::state& lua, HedgehogEngine::EngineContext& context)
        {
            lua.new_usertype<ColliderRef>(
                "Collider", sol::no_constructor,
                "shape",       EnumBodyField(context, &ColliderComponent::Shape, 3, "ColliderShape"),
                "center",      BodyField(context, &ColliderComponent::Center, FiniteVector("a collider's center")),
                "size",        BodyField(context, &ColliderComponent::Size, [](const HM::Vector3& value)
                {
                    RequireFinite(value, "a collider's size");
                    if (!(value.x() > 0.0f && value.y() > 0.0f && value.z() > 0.0f))
                        throw std::runtime_error("a collider's size must be above 0 on every axis");
                    return value;
                }),
                "radius",      BodyField(context, &ColliderComponent::Radius, Positive("a radius")),
                "height",      BodyField(context, &ColliderComponent::Height, NotNegative("a height")),
                "isTrigger",   BodyField(context, &ColliderComponent::IsTrigger, [](bool value) { return value; }),
                "layer",       BodyField(context, &ColliderComponent::Layer, [](int32_t value)
                {
                    if (value < 0 || value >= LAYER_COUNT)
                        throw std::runtime_error(std::to_string(value) + " is not a physics layer (0 to 15)");
                    return value;
                }),
                "friction",    BodyField(context, &ColliderComponent::Friction, NotNegative("a friction")),
                "restitution", BodyField(context, &ColliderComponent::Restitution, NotNegative("a restitution")),
                sol::meta_function::to_string, ToText<ColliderComponent>("Collider"));
        }

        void RegisterPhysicsTable(sol::state& lua, HedgehogEngine::EngineContext& context, const std::shared_ptr<PlayGuard>& guard)
        {
            HedgehogEngine::PhysicsSystem& physics = *context.GetPhysicsSystem();
            sol::table                     table   = lua.create_named_table("Physics");

            // raycast(origin, direction, maxDistance?, layerMask?): the nearest collider (triggers
            // skipped) as { entity, point, normal, distance }, or nil.
            table["raycast"] = [&context, &physics, guard](const HM::Vector3& origin, const HM::Vector3& direction,
                                                          sol::optional<float> maxDistance, sol::optional<int64_t> layerMask,
                                                          sol::this_state state) -> sol::object
            {
                RequireFinite(origin, "a ray's origin");
                RequireFinite(direction, "a ray's direction");
                const float distance = maxDistance ? RequireFinite(*maxDistance, "a ray's distance") : 1000.0f;
                const int64_t mask   = layerMask.value_or(0xffff);
                if (mask < 0 || mask > 0xffff)
                    throw std::runtime_error("a layer mask must be 0 to 65535");
                if (!guard->Allows("Physics.raycast()"))
                    return sol::lua_nil;
                const std::optional<HedgehogEngine::PhysicsRayHit> hit =
                    physics.Raycast(origin, direction, distance, static_cast<uint16_t>(mask));
                if (!hit)
                    return sol::lua_nil;
                sol::state_view view(state);
                sol::table      result = view.create_table();
                ECS::ECS&       ecs    = context.GetECS();
                result["entity"]   = ScriptEntity{ hit->Entity, ecs.IsAlive(hit->Entity) ? ecs.GetGeneration(hit->Entity) : 0u };
                result["point"]    = hit->Point;
                result["normal"]   = hit->Normal;
                result["distance"] = hit->Distance;
                return result;
            };

            // Physics.gravity: the running world's gravity, read through __index so every read is
            // current; written only in Play, and kept until the next Play reads the settings again.
            sol::table meta = lua.create_table();
            meta[sol::meta_function::index] = [&physics](sol::table, const std::string& key, sol::this_state state) -> sol::object
            {
                if (key == "gravity")
                    return sol::make_object(state, physics.GetGravity());
                return sol::lua_nil;
            };
            meta[sol::meta_function::new_index] = [&physics, guard](sol::table, const std::string& key, sol::object value)
            {
                if (key != "gravity")
                    throw std::runtime_error("Physics." + key + " cannot be set; only Physics.gravity can");
                if (!value.is<HM::Vector3>())
                    throw std::runtime_error("Physics.gravity must be a Vector3");
                const HM::Vector3 gravity = RequireFinite(value.as<HM::Vector3>(), "Physics.gravity");
                if (guard->Allows("Physics.gravity"))
                    physics.SetGravity(gravity);
            };
            table[sol::metatable_key] = meta;
        }
    }

    void RegisterPhysics(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        auto guard = std::make_shared<PlayGuard>(PlayGuard{ context });
        RegisterEnums(lua);
        RegisterRigidBody(lua, context, guard);
        RegisterCollider(lua, context);
        RegisterPhysicsTable(lua, context, guard);
    }
}
