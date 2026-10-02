#include "Bindings.hpp"
#include "ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

namespace HedgehogScripting::Bindings
{
    namespace
    {
        using HedgehogEngine::TransformComponent;
        using HM::Quaternion;
        using HM::Vector3;
        using TransformRef = ScriptComponentRef<TransformComponent>;

        ECS::HierarchyComponent* FindHierarchy(ECS::ECS& ecs, const ScriptEntity& handle)
        {
            RequireValid(ecs, handle);
            if (!ecs.HasComponent<ECS::HierarchyComponent>(handle.Id))
                return nullptr;
            return &ecs.GetComponent<ECS::HierarchyComponent>(handle.Id);
        }

        // Column `column` of the world matrix as a unit vector. Matrix4x4 stores columns, so
        // 0, 1 and 2 are the entity's world x, y and z axes.
        Vector3 WorldAxis(const TransformComponent& transform, size_t column)
        {
            const HM::Vector4& axis = transform.ObjMatrix[column];
            const Vector3      v(axis.x(), axis.y(), axis.z());
            return v.LengthSqr() > 0.0f ? v.Normalize() : v;
        }

        // entity:getX() (the component's handle, or nil), entity:hasX() and entity:addX() (adds a
        // default one, runs onAdd, and returns its handle; an existing one is returned as is).
        template<typename T, typename OnAdd>
        void AddComponentAccess(sol::usertype<ScriptEntity>& type, ECS::ECS& ecs, const std::string& name, OnAdd onAdd)
        {
            type["get" + name] = [&ecs](const ScriptEntity& e, sol::this_state state) -> sol::object
            {
                RequireValid(ecs, e);
                if (!ecs.HasComponent<T>(e.Id))
                    return sol::lua_nil;
                return sol::make_object(state, ScriptComponentRef<T>{ e });
            };
            type["has" + name] = [&ecs](const ScriptEntity& e)
            {
                RequireValid(ecs, e);
                return ecs.HasComponent<T>(e.Id);
            };
            type["add" + name] = [&ecs, onAdd](const ScriptEntity& e)
            {
                RequireValid(ecs, e);
                if (!ecs.HasComponent<T>(e.Id))
                    onAdd(e.Id);
                return ScriptComponentRef<T>{ e };
            };
        }

        sol::usertype<ScriptEntity> RegisterEntityType(sol::state& lua, ECS::ECS& ecs)
        {
            return lua.new_usertype<ScriptEntity>(
                "Entity", sol::no_constructor,

                "id", sol::readonly_property([](const ScriptEntity& e) { return e.Id; }),
                "isValid", [&ecs](const ScriptEntity& e) { return IsValid(ecs, e); },

                "name", sol::property(
                    [&ecs](const ScriptEntity& e) -> std::string
                    {
                        const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                        return hierarchy != nullptr ? hierarchy->Name : "Entity " + std::to_string(e.Id);
                    },
                    [&ecs](const ScriptEntity& e, const std::string& name)
                    {
                        ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                        if (hierarchy == nullptr)
                            throw std::runtime_error(Describe(e) + " has no name to set");
                        hierarchy->Name = name;
                    }),
                // nil for a top-level entity, whose parent is the scene root.
                "parent", sol::readonly_property([&ecs](const ScriptEntity& e, sol::this_state state) -> sol::object
                {
                    const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                    if (hierarchy == nullptr || hierarchy->Parent == ecs.GetRoot() || !ecs.IsAlive(hierarchy->Parent))
                        return sol::lua_nil;
                    return sol::make_object(state, MakeScriptEntity(ecs, hierarchy->Parent));
                }),
                // A new array of handles, in hierarchy order.
                "children", sol::readonly_property([&ecs](const ScriptEntity& e)
                {
                    std::vector<ScriptEntity>      children;
                    const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                    if (hierarchy != nullptr)
                    {
                        for (const ECS::Entity child : hierarchy->Children)
                            if (ecs.IsAlive(child))
                                children.push_back(MakeScriptEntity(ecs, child));
                    }
                    return sol::as_table(std::move(children));
                }),
                "transform", sol::readonly_property([&ecs](const ScriptEntity& e)
                {
                    const TransformRef ref{ e };
                    (void)Resolve(ecs, ref); // a stale handle or a missing transform fails here
                    return ref;
                }),

                sol::meta_function::equal_to,
                [](const ScriptEntity& a, const ScriptEntity& b) { return a.Id == b.Id && a.Generation == b.Generation; },
                sol::meta_function::to_string, [](const ScriptEntity& e) { return Describe(e); });
        }

        void RegisterTransformType(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus)
        {
            // Every write goes through here, so TransformSystem and HierarchySystem, which run
            // after the play-mode update, rebuild the matrices this frame.
            auto change = [&ecs, &eventBus](const TransformRef& t, auto&& edit)
            {
                edit(Resolve(ecs, t));
                eventBus.Publish(HedgehogEngine::TransformChangedEvent{ t.Entity.Id });
            };
            auto rotationOf = [&ecs](const TransformRef& t) { return Quaternion::FromEuler(Resolve(ecs, t).Rotation); };
            auto turn       = [change](const TransformRef& t, const Quaternion& by)
            {
                change(t, [&](TransformComponent& c) { c.Rotation = (Quaternion::FromEuler(c.Rotation) * by).Normalize().ToEuler(); });
            };

            lua.new_usertype<TransformRef>(
                "Transform", sol::no_constructor,

                "position", sol::property(
                    [&ecs](const TransformRef& t) { return Resolve(ecs, t).Position; },
                    [change](const TransformRef& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Position = value; }); }),
                // Euler degrees, exactly as stored and shown in the inspector.
                "eulerAngles", sol::property(
                    [&ecs](const TransformRef& t) { return Resolve(ecs, t).Rotation; },
                    [change](const TransformRef& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Rotation = value; }); }),
                // The same rotation as a Quat; writing it stores its Euler angles.
                "rotation", sol::property(
                    rotationOf,
                    [change](const TransformRef& t, const Quaternion& value)
                    { change(t, [&](TransformComponent& c) { c.Rotation = value.Normalize().ToEuler(); }); }),
                "scale", sol::property(
                    [&ecs](const TransformRef& t) { return Resolve(ecs, t).Scale; },
                    [change](const TransformRef& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Scale = value; }); }),

                // World axes from the last computed world matrix; -Z is forward, as for a camera.
                "right", sol::readonly_property([&ecs](const TransformRef& t) { return WorldAxis(Resolve(ecs, t), 0); }),
                "up", sol::readonly_property([&ecs](const TransformRef& t) { return WorldAxis(Resolve(ecs, t), 1); }),
                "forward", sol::readonly_property([&ecs](const TransformRef& t) { return -WorldAxis(Resolve(ecs, t), 2); }),

                // Moves by offset, in the parent's space.
                "translate", [change](const TransformRef& t, const Vector3& offset)
                { change(t, [&](TransformComponent& c) { c.Position = c.Position + offset; }); },
                // Turns about the entity's own axes, by Euler degrees or a Quat.
                "rotate", sol::overload(
                    [turn](const TransformRef& t, const Vector3& eulerDegrees) { turn(t, Quaternion::FromEuler(eulerDegrees)); },
                    [turn](const TransformRef& t, float x, float y, float z) { turn(t, Quaternion::FromEuler(x, y, z)); },
                    [turn](const TransformRef& t, const Quaternion& by) { turn(t, by); }),

                sol::meta_function::to_string, [](const TransformRef& t) { return "Transform of " + Describe(t.Entity); });
        }
    }

    void RegisterEntity(sol::state& lua, HedgehogEngine::EngineContext& context)
    {
        ECS::ECS&                 ecs      = context.GetECS();
        HedgehogEngine::EventBus& eventBus = context.GetEventBus();

        sol::usertype<ScriptEntity> entity = RegisterEntityType(lua, ecs);
        RegisterTransformType(lua, ecs, eventBus);

        // Added as the editor's Add Component menu adds them. A new light learns its position
        // and direction from its world matrix, so its transform is marked changed.
        AddComponentAccess<HedgehogEngine::LightComponent>(entity, ecs, "Light", [&ecs, &eventBus](ECS::Entity id)
        {
            ecs.AddComponent(id, HedgehogEngine::LightComponent{});
            if (ecs.HasComponent<TransformComponent>(id))
                eventBus.Publish(HedgehogEngine::TransformChangedEvent{ id });
        });
        AddComponentAccess<HedgehogEngine::CameraComponent>(entity, ecs, "Camera", [&ecs](ECS::Entity id)
        {
            ecs.AddComponent(id, HedgehogEngine::CameraComponent{});
        });
        AddComponentAccess<HedgehogEngine::MeshComponent>(entity, ecs, "Mesh", [&context](ECS::Entity id)
        {
            ECS::ECS& world = context.GetECS();
            world.AddComponent(id, HedgehogEngine::MeshComponent{ HedgehogEngine::MeshSystem::sDefaultMeshPath });
            context.GetMeshSystem()->Update(world, id, context.GetFileSystem());
        });
        AddComponentAccess<HedgehogEngine::AnimatorComponent>(entity, ecs, "Animator", [&ecs](ECS::Entity id)
        {
            ecs.AddComponent(id, HedgehogEngine::AnimatorComponent{});
        });
    }
}
