#pragma once

#include "EcsSerializationApi.hpp"
#include "ComponentSerializerRegistry.hpp"
#include "Reflection/PropertyDescriptor.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace EcsSerialization
{
    // What a component type is called and how the editor offers it. Names only, never editor types,
    // so the engine, the editor and plugins can share one list without depending on each other.
    struct ComponentDesc
    {
        std::string Key;              // the YAML key, unique in the registry
        std::string DisplayName;      // the label menus and the inspector show
        std::string Category;         // groups menu entries ("", "UI", "Audio", ...)
        std::string Icon;             // an icon name the editor maps to its own icons
        bool        Addable   = true; // offered by Add Component
        bool        Removable = true; // offered by the inspector's Remove component
        // A reflected bool property switching the component on and off (the inspector's header
        // checkbox), or empty.
        std::string EnabledProperty;
        // Adds a default component to an entity; empty adds T{}.
        std::function<void(ECS::ECS&, ECS::Entity)> AddDefault;
    };

    // A registered component type, used type-erased.
    struct ComponentInfo : ComponentDesc
    {
        // The reflected properties (empty for a type with no GetProperties()).
        std::span<const Reflection::PropertyDescriptor> Properties;
        std::function<bool(const ECS::ECS&, ECS::Entity)> Has;
        // The entity's component, which it must have.
        std::function<void*(ECS::ECS&, ECS::Entity)> Get;
        std::function<void(ECS::ECS&, ECS::Entity)>  Remove;

        // The component's enabled flag (EnabledProperty), or nullptr when it has none.
        [[nodiscard]] bool* Enabled(void* component) const
        {
            return m_EnabledProperty ? Reflection::FieldPtr<bool>(component, *m_EnabledProperty) : nullptr;
        }

    private:
        friend class ComponentTypeRegistry;

        const Reflection::PropertyDescriptor* m_EnabledProperty = nullptr;
        // Takes the type out of the ECS; false when the ECS refuses (a system requires it).
        std::function<bool(ECS::ECS&)> m_UnregisterFromEcs;
    };

    // The one list of component types: registering one registers it in the ECS, gives the
    // serializer registry its handler (reflected, custom, or none for a type written some other
    // way, as the hierarchy is) and keeps a ComponentInfo that menus and the inspector iterate.
    // Both registries must outlive this one. A refused registration (an empty key or one used
    // twice, a type the ECS already has, an EnabledProperty that names no reflected bool property,
    // a custom serializer that adds no handler of the key) logs one error and changes nothing.
    class ComponentTypeRegistry
    {
    public:
        ComponentTypeRegistry(ECS::ECS& ecs, ComponentSerializerRegistry& serializers)
            : m_Ecs(ecs)
            , m_Serializers(serializers)
        {
        }

        ComponentTypeRegistry(const ComponentTypeRegistry&)            = delete;
        ComponentTypeRegistry& operator=(const ComponentTypeRegistry&) = delete;

        // A reflected component (T::GetProperties()), written by the serializer's RegisterReflected.
        template<typename T>
            requires requires { T::GetProperties(); }
        bool RegisterReflected(ComponentDesc desc)
        {
            return Register<T>(std::move(desc), [](ComponentSerializerRegistry& serializers, const std::string& key)
                               { serializers.RegisterReflected<T>(key.c_str()); });
        }

        // A component with its own serializer: registerSerializer must add a handler of desc.Key.
        template<typename T>
        bool RegisterCustom(ComponentDesc desc, const std::function<void(ComponentSerializerRegistry&)>& registerSerializer)
        {
            return Register<T>(std::move(desc), [&registerSerializer](ComponentSerializerRegistry& serializers, const std::string&)
                               { registerSerializer(serializers); });
        }

        // A component no handler writes (or one the serializer writes itself, as the hierarchy).
        template<typename T>
        bool RegisterUnserialized(ComponentDesc desc)
        {
            return Register<T>(std::move(desc), {});
        }

        // Takes the type out of the ECS (every entity loses it), the serializer registry and the
        // list. False, changing nothing, for an unknown key or when the ECS refuses (a registered
        // system requires the type). Pointers from Find and GetInfos are invalidated.
        ECS_SERIALIZATION_API bool Unregister(std::string_view key);

        // In registration order.
        [[nodiscard]] const std::vector<ComponentInfo>& GetInfos() const { return m_Infos; }
        // nullptr for an unknown key.
        [[nodiscard]] ECS_SERIALIZATION_API const ComponentInfo* Find(std::string_view key) const;

    private:
        using SerializerRegistration = std::function<void(ComponentSerializerRegistry&, const std::string&)>;

        template<typename T>
        bool Register(ComponentDesc desc, const SerializerRegistration& registerSerializer)
        {
            ComponentInfo info;
            static_cast<ComponentDesc&>(info) = std::move(desc);
            if constexpr (requires { T::GetProperties(); })
                info.Properties = T::GetProperties();
            if (!CanRegister(info, m_Ecs.IsComponentRegistered<T>()))
                return false;

            m_Ecs.RegisterComponent<T>();
            if (registerSerializer)
            {
                registerSerializer(m_Serializers, info.Key);
                if (m_Serializers.FindHandler(info.Key) == nullptr)
                {
                    (void)m_Ecs.UnregisterComponent<T>();
                    ReportRefused(info.Key, "its serializer added no handler of that key");
                    return false;
                }
            }

            if (!info.AddDefault)
                info.AddDefault = [](ECS::ECS& ecs, ECS::Entity e) { ecs.AddComponent(e, T{}); };
            info.Has                 = [](const ECS::ECS& ecs, ECS::Entity e) { return ecs.HasComponent<T>(e); };
            info.Get                 = [](ECS::ECS& ecs, ECS::Entity e) -> void* { return &ecs.GetComponent<T>(e); };
            info.Remove              = [](ECS::ECS& ecs, ECS::Entity e) { ecs.RemoveComponent<T>(e); };
            info.m_UnregisterFromEcs = [](ECS::ECS& ecs) { return ecs.UnregisterComponent<T>(); };
            m_Infos.push_back(std::move(info));
            return true;
        }

        // Checks the key, the ECS and the enabled property, and resolves the latter; logs a refusal.
        ECS_SERIALIZATION_API bool CanRegister(ComponentInfo& info, bool knownToEcs) const;
        ECS_SERIALIZATION_API static void ReportRefused(const std::string& key, const std::string& why);

        ECS::ECS&                    m_Ecs;
        ComponentSerializerRegistry& m_Serializers;
        std::vector<ComponentInfo>   m_Infos;
    };
}
