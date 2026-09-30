#include "EntityBindings.hpp"

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogMath/api/Quaternion.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include <stdexcept>
#include <vector>

namespace HedgehogScripting::Bindings
{
    using HedgehogEngine::TransformComponent;
    using HM::Quaternion;
    using HM::Vector3;

    namespace
    {
        ECS::HierarchyComponent* FindHierarchy(ECS::ECS& ecs, const EntityHandle& handle)
        {
            RequireValid(ecs, handle);
            if (!ecs.HasComponent<ECS::HierarchyComponent>(handle.Id))
                return nullptr;
            return &ecs.GetComponent<ECS::HierarchyComponent>(handle.Id);
        }

        sol::usertype<EntityHandle> RegisterEntityType(sol::state& lua, ECS::ECS& ecs)
        {
            return lua.new_usertype<EntityHandle>(
                "Entity", sol::no_constructor,

                "id", sol::readonly_property([](const EntityHandle& e) { return e.Id; }),
                "generation", sol::readonly_property([](const EntityHandle& e) { return e.Generation; }),
                "isValid", [&ecs](const EntityHandle& e) { return IsValid(ecs, e); },

                "name", sol::readonly_property([&ecs](const EntityHandle& e) -> std::string
                {
                    const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                    return hierarchy != nullptr ? hierarchy->Name : "Entity " + std::to_string(e.Id);
                }),
                // nil for a top-level entity, whose parent is the scene root.
                "parent", sol::readonly_property([&ecs](const EntityHandle& e, sol::this_state state) -> sol::object
                {
                    const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                    if (hierarchy == nullptr || hierarchy->Parent == ecs.GetRoot() || !ecs.IsAlive(hierarchy->Parent))
                        return sol::lua_nil;
                    return sol::make_object(state, MakeEntityHandle(ecs, hierarchy->Parent));
                }),
                // A new array of handles, in hierarchy order.
                "children", sol::readonly_property([&ecs](const EntityHandle& e)
                {
                    std::vector<EntityHandle>      children;
                    const ECS::HierarchyComponent* hierarchy = FindHierarchy(ecs, e);
                    if (hierarchy != nullptr)
                    {
                        for (ECS::Entity child : hierarchy->Children)
                            if (ecs.IsAlive(child))
                                children.push_back(MakeEntityHandle(ecs, child));
                    }
                    return sol::as_table(std::move(children));
                }),
                "transform", sol::readonly_property([&ecs](const EntityHandle& e)
                {
                    (void)GetTransform(ecs, e); // a stale handle or a missing transform fails here
                    return TransformHandle{ e };
                }),

                sol::meta_function::equal_to,
                [](const EntityHandle& a, const EntityHandle& b) { return a.Id == b.Id && a.Generation == b.Generation; },
                sol::meta_function::to_string, [](const EntityHandle& e) { return DescribeEntity(e); });
        }

        void RegisterTransformType(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus)
        {
            // Every write goes through here, so TransformSystem rebuilds the matrix this frame.
            auto change = [&ecs, &eventBus](const TransformHandle& t, auto&& edit)
            {
                edit(GetTransform(ecs, t.Entity));
                eventBus.Publish(HedgehogEngine::TransformChangedEvent{ t.Entity.Id });
            };
            auto rotationOf = [&ecs](const TransformHandle& t)
            {
                return Quaternion::FromEuler(GetTransform(ecs, t.Entity).Rotation);
            };

            lua.new_usertype<TransformHandle>(
                "Transform", sol::no_constructor,

                "position", sol::property(
                    [&ecs](const TransformHandle& t) { return GetTransform(ecs, t.Entity).Position; },
                    [change](const TransformHandle& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Position = value; }); }),
                // Euler degrees, exactly as stored and shown in the inspector.
                "eulerAngles", sol::property(
                    [&ecs](const TransformHandle& t) { return GetTransform(ecs, t.Entity).Rotation; },
                    [change](const TransformHandle& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Rotation = value; }); }),
                // The same rotation as a Quat; writing it stores its Euler angles.
                "rotation", sol::property(
                    rotationOf,
                    [change](const TransformHandle& t, const Quaternion& value)
                    { change(t, [&](TransformComponent& c) { c.Rotation = value.Normalize().ToEuler(); }); }),
                "scale", sol::property(
                    [&ecs](const TransformHandle& t) { return GetTransform(ecs, t.Entity).Scale; },
                    [change](const TransformHandle& t, const Vector3& value)
                    { change(t, [&](TransformComponent& c) { c.Scale = value; }); }),

                // The local axes in the parent's space; -Z is forward, as for a camera.
                "forward", sol::readonly_property([rotationOf](const TransformHandle& t)
                                                  { return rotationOf(t) * Vector3(0.0f, 0.0f, -1.0f); }),
                "right", sol::readonly_property([rotationOf](const TransformHandle& t)
                                                { return rotationOf(t) * Vector3(1.0f, 0.0f, 0.0f); }),
                "up", sol::readonly_property([rotationOf](const TransformHandle& t)
                                             { return rotationOf(t) * Vector3(0.0f, 1.0f, 0.0f); }),

                // Moves by offset, in the parent's space.
                "translate", [change](const TransformHandle& t, const Vector3& offset)
                { change(t, [&](TransformComponent& c) { c.Position = c.Position + offset; }); },
                // Turns about the entity's own axes: rotation becomes rotation * turn.
                "rotate", sol::overload(
                    [change](const TransformHandle& t, const Quaternion& turn)
                    {
                        change(t, [&](TransformComponent& c)
                               { c.Rotation = (Quaternion::FromEuler(c.Rotation) * turn).Normalize().ToEuler(); });
                    },
                    [change](const TransformHandle& t, float x, float y, float z)
                    {
                        const Quaternion turn = Quaternion::FromEuler(x, y, z);
                        change(t, [&](TransformComponent& c)
                               { c.Rotation = (Quaternion::FromEuler(c.Rotation) * turn).Normalize().ToEuler(); });
                    }),

                sol::meta_function::to_string,
                [](const TransformHandle& t) { return "Transform of " + DescribeEntity(t.Entity); });
        }
    }

    EntityHandle MakeEntityHandle(const ECS::ECS& ecs, ECS::Entity entity)
    {
        return EntityHandle{ entity, ecs.GetGeneration(entity) };
    }

    bool IsValid(const ECS::ECS& ecs, const EntityHandle& handle)
    {
        return handle.Id != ECS::INVALID_ENTITY && ecs.IsAlive(handle.Id) &&
               ecs.GetGeneration(handle.Id) == handle.Generation;
    }

    std::string DescribeEntity(const EntityHandle& handle)
    {
        return "Entity " + std::to_string(handle.Id) + " (generation " + std::to_string(handle.Generation) + ")";
    }

    void RequireValid(const ECS::ECS& ecs, const EntityHandle& handle)
    {
        if (!IsValid(ecs, handle))
            throw std::runtime_error(DescribeEntity(handle) + " no longer exists");
    }

    TransformComponent& GetTransform(ECS::ECS& ecs, const EntityHandle& handle)
    {
        RequireValid(ecs, handle);
        if (!ecs.HasComponent<TransformComponent>(handle.Id))
            throw std::runtime_error(DescribeEntity(handle) + " has no transform");
        return ecs.GetComponent<TransformComponent>(handle.Id);
    }

    void RegisterEntity(sol::state& lua, ECS::ECS& ecs, HedgehogEngine::EventBus& eventBus,
                        const FS::FileSystemManager& fileSystem)
    {
        sol::usertype<EntityHandle> entityType = RegisterEntityType(lua, ecs);
        RegisterTransformType(lua, ecs, eventBus);
        RegisterComponents(lua, entityType, ecs, fileSystem);
    }
}
