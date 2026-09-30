#include "EntityBindings.hpp"

#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"

#include "ECS/api/ECS.hpp"

#include <stdexcept>
#include <string>

namespace HedgehogScripting::Bindings
{
    using HedgehogEngine::CameraComponent;
    using HedgehogEngine::CameraProjectionType;
    using HedgehogEngine::LightComponent;
    using HedgehogEngine::LightType;
    using HedgehogEngine::MeshComponent;

    namespace
    {
        // A component as scripts see it: the entity handle, with the component looked up on every
        // access. ComponentArray swap-removes, so a pointer into it could come to point at another
        // entity's component; a handle cannot.
        template<typename Component>
        struct ComponentHandle
        {
            EntityHandle Entity;
        };

        using LightHandle  = ComponentHandle<LightComponent>;
        using CameraHandle = ComponentHandle<CameraComponent>;
        using MeshHandle   = ComponentHandle<MeshComponent>;

        template<typename Component>
        const char* NameOf();
        template<>
        const char* NameOf<LightComponent>() { return "LightComponent"; }
        template<>
        const char* NameOf<CameraComponent>() { return "CameraComponent"; }
        template<>
        const char* NameOf<MeshComponent>() { return "MeshComponent"; }

        template<typename Component>
        std::string Describe(const ComponentHandle<Component>& handle)
        {
            return std::string(NameOf<Component>()) + " of " + DescribeEntity(handle.Entity);
        }

        // The component, or a Lua error naming it and the entity once either is gone.
        template<typename Component>
        Component& Resolve(ECS::ECS& ecs, const ComponentHandle<Component>& handle)
        {
            RequireValid(ecs, handle.Entity);
            if (!ecs.HasComponent<Component>(handle.Entity.Id))
                throw std::runtime_error(Describe(handle) + " no longer exists");
            return ecs.GetComponent<Component>(handle.Entity.Id);
        }

        // A read-write property over one field of the component.
        template<typename Component, typename Value>
        auto Field(ECS::ECS& ecs, Value Component::*member)
        {
            using Handle = ComponentHandle<Component>;
            return sol::property([&ecs, member](const Handle& h) -> Value { return Resolve(ecs, h).*member; },
                                 [&ecs, member](const Handle& h, Value value) { Resolve(ecs, h).*member = value; });
        }

        // An enum field, set from its Lua enum table's integer; anything out of range is an error.
        template<typename Component, typename Enum>
        auto EnumField(ECS::ECS& ecs, Enum Component::*member, int count)
        {
            using Handle = ComponentHandle<Component>;
            return sol::property(
                [&ecs, member](const Handle& h) { return static_cast<int>(Resolve(ecs, h).*member); },
                [&ecs, member, count](const Handle& h, int value)
                {
                    if (value < 0 || value >= count)
                        throw std::runtime_error(Describe(h) + ": " + std::to_string(value) +
                                                 " is not a valid enum value");
                    Resolve(ecs, h).*member = static_cast<Enum>(value);
                });
        }

        // Entity:get<Name>() (nil when absent), has<Name>() and add<Name>() (the existing one if
        // there is one; prepare sets up a component that is new).
        template<typename Component, typename Prepare>
        void AddAccessors(sol::usertype<EntityHandle>& entityType, ECS::ECS& ecs, const std::string& name,
                          Prepare prepare)
        {
            using Handle = ComponentHandle<Component>;
            entityType["get" + name] = [&ecs](const EntityHandle& e, sol::this_state state) -> sol::object
            {
                RequireValid(ecs, e);
                if (!ecs.HasComponent<Component>(e.Id))
                    return sol::lua_nil;
                return sol::make_object(state, Handle{ e });
            };
            entityType["has" + name] = [&ecs](const EntityHandle& e)
            {
                RequireValid(ecs, e);
                return ecs.HasComponent<Component>(e.Id);
            };
            entityType["add" + name] = [&ecs, prepare](const EntityHandle& e)
            {
                RequireValid(ecs, e);
                if (!ecs.HasComponent<Component>(e.Id))
                {
                    ecs.AddComponent(e.Id, Component{});
                    prepare(e);
                }
                return Handle{ e };
            };
        }

        // Makes MeshSystem notice a changed MeshPath now, as the inspector does: it registers a new
        // mesh for the resource catalog's next update, or reverts a path that does not exist.
        void RefreshMesh(ECS::ECS& ecs, const FS::FileSystemManager& fileSystem, ECS::Entity entity)
        {
            if (ecs.HasSystem<HedgehogEngine::MeshSystem>())
                ecs.GetSystem<HedgehogEngine::MeshSystem>()->Update(ecs, entity, fileSystem);
        }
    }

    void RegisterComponents(sol::state& lua, sol::usertype<EntityHandle>& entityType, ECS::ECS& ecs,
                            const FS::FileSystemManager& fileSystem)
    {
        lua.new_enum("LightType", "DirectionLight", LightType::DirectionLight, "PointLight", LightType::PointLight,
                     "SpotLight", LightType::SpotLight);
        lua.new_enum("CameraProjectionType", "Perspective", CameraProjectionType::Perspective, "Orthographic",
                     CameraProjectionType::Orthographic);

        lua.new_usertype<LightHandle>(
            "Light", sol::no_constructor,
            "entity", sol::readonly_property([](const LightHandle& h) { return h.Entity; }),
            "enable", Field(ecs, &LightComponent::Enable),
            "lightType", EnumField(ecs, &LightComponent::LightType, 3),
            "color", Field(ecs, &LightComponent::Color),
            "intensity", Field(ecs, &LightComponent::Intensity),
            "radius", Field(ecs, &LightComponent::Radius),
            "coneAngle", Field(ecs, &LightComponent::ConeAngle),
            "castShadows", Field(ecs, &LightComponent::CastShadows),
            sol::meta_function::to_string, [](const LightHandle& h) { return Describe(h); });

        lua.new_usertype<CameraHandle>(
            "Camera", sol::no_constructor,
            "entity", sol::readonly_property([](const CameraHandle& h) { return h.Entity; }),
            "isEnabled", Field(ecs, &CameraComponent::IsEnabled),
            "projectionType", EnumField(ecs, &CameraComponent::ProjectionType, 2),
            "fov", Field(ecs, &CameraComponent::Fov),
            "orthoSize", Field(ecs, &CameraComponent::OrthoSize),
            "nearPlane", Field(ecs, &CameraComponent::NearPlane),
            "farPlane", Field(ecs, &CameraComponent::FarPlane),
            "layerMask", Field(ecs, &CameraComponent::LayerMask),
            "graphName", Field(ecs, &CameraComponent::GraphName),
            "priority", Field(ecs, &CameraComponent::Priority),
            sol::meta_function::to_string, [](const CameraHandle& h) { return Describe(h); });

        lua.new_usertype<MeshHandle>(
            "Mesh", sol::no_constructor,
            "entity", sol::readonly_property([](const MeshHandle& h) { return h.Entity; }),
            // Relative to assets://, as in the inspector. A path that does not exist is logged and
            // reverted to the previous mesh.
            "meshPath", sol::property(
                [&ecs](const MeshHandle& h) { return Resolve(ecs, h).MeshPath; },
                [&ecs, &fileSystem](const MeshHandle& h, const std::string& path)
                {
                    Resolve(ecs, h).MeshPath = path;
                    RefreshMesh(ecs, fileSystem, h.Entity.Id);
                }),
            sol::meta_function::to_string, [](const MeshHandle& h) { return Describe(h); });

        const auto nothing = [](const EntityHandle&) {};
        AddAccessors<LightComponent>(entityType, ecs, "Light", nothing);
        AddAccessors<CameraComponent>(entityType, ecs, "Camera", nothing);
        AddAccessors<MeshComponent>(entityType, ecs, "Mesh", [&ecs, &fileSystem](const EntityHandle& e)
        {
            // A new mesh component shows the default mesh until the script picks another.
            ecs.GetComponent<MeshComponent>(e.Id).MeshPath = HedgehogEngine::MeshSystem::sDefaultMeshPath;
            RefreshMesh(ecs, fileSystem, e.Id);
        });
    }
}
