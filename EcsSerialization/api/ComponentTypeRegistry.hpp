#pragma once

#include "EcsSerializationApi.hpp"
#include "ComponentSerializerRegistry.hpp"
#include "Reflection/PropertyDescriptor.hpp"
#include "UnknownComponents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <algorithm>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace EcsSerialization
{
    // What a component type is called and how the editor offers it. Names only, never editor types,
    // so the engine, the editor and plugins can share one list without depending on each other.
    struct ComponentDesc
    {
        std::string Key;              // the YAML key, unique in the registry
        std::string DisplayName;      // what it is called: the inspector's header ("UI canvas")
        std::string Category;         // groups menu entries ("", "UI", "Audio", ...)
        int         SortOrder = 0;    // where menus and the inspector list it, ascending
        std::string Icon;             // an icon name the editor maps to its own icons
        bool        Addable     = true; // offered by Add Component
        bool        Removable   = true; // offered by the inspector's Remove component
        bool        Inspectable = true; // drawn by the inspector
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

    // What Unregister does with the data of the entities holding the component.
    enum class UnknownData
    {
        Drop, // the components go with the type
        Keep  // each is written into the entity's UnknownComponentsComponent, so it is saved and
              // a later registration of the key gets it back
    };

    // The one list of component types: registering one registers it in the ECS, gives the
    // serializer registry its handler (reflected, custom, or none for a type written some other
    // way, as the hierarchy is) and keeps a ComponentInfo that menus and the inspector iterate.
    // Both registries must outlive this one. A refused registration (an empty key, a reserved
    // entity key (IsReservedEntityKey) or UNKNOWN_COMPONENTS_KEY for another type than the store,
    // a key used twice, a type the ECS already has, an EnabledProperty that names no reflected bool
    // property, a custom serializer that adds no handler of the key) logs one error and changes
    // nothing.
    //
    // Data kept for plugins that are not loaded (UnknownComponentsComponent, filled by
    // EcsSerializer): registering a type with a handler gives every entity holding an unknown
    // component of its key that component, read by the handler, and drops the kept entry (and the
    // store, once empty). Unregister(key, UnknownData::Keep) puts it back.
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
        // system requires the type). With UnknownData::Keep each entity's component is first
        // written through its handler into the entity's UnknownComponentsComponent (dropped, with
        // one warning, when the ECS has no such store or the type no handler). Pointers from Find
        // and GetInfos are invalidated.
        ECS_SERIALIZATION_API bool Unregister(std::string_view key, UnknownData data = UnknownData::Drop);

        // In registration order (the order serializers write a scene's components in).
        [[nodiscard]] const std::vector<ComponentInfo>& GetInfos() const { return m_Infos; }
        // By SortOrder, ties in registration order: the order menus and the inspector list them in.
        [[nodiscard]] std::vector<const ComponentInfo*> GetInfosInOrder() const
        {
            std::vector<const ComponentInfo*> ordered;
            ordered.reserve(m_Infos.size());
            for (const ComponentInfo& info : m_Infos)
                ordered.push_back(&info);
            std::stable_sort(ordered.begin(), ordered.end(),
                             [](const ComponentInfo* a, const ComponentInfo* b) { return a->SortOrder < b->SortOrder; });
            return ordered;
        }
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
            if (!CanRegister(info, m_Ecs.IsComponentRegistered<T>(), std::is_same_v<T, UnknownComponentsComponent>))
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
            AdoptKeptData(m_Infos.back().Key);
            return true;
        }

        // Checks the key, the ECS and the enabled property, and resolves the latter; logs a refusal.
        // isStore allows UNKNOWN_COMPONENTS_KEY, for UnknownComponentsComponent itself.
        ECS_SERIALIZATION_API bool CanRegister(ComponentInfo& info, bool knownToEcs, bool isStore) const;
        // Gives every entity holding kept data of key its component, through key's handler.
        ECS_SERIALIZATION_API void AdoptKeptData(const std::string& key);
        // Writes every entity's component of info into its UnknownComponentsComponent; returns the
        // entities given an entry, so a refused unregistration can take them back.
        std::vector<ECS::Entity> KeepData(const ComponentInfo& info);
        void                     DropKeptEntries(const std::string& key, const std::vector<ECS::Entity>& entities);
        ECS_SERIALIZATION_API static void ReportRefused(const std::string& key, const std::string& why);

        ECS::ECS&                    m_Ecs;
        ComponentSerializerRegistry& m_Serializers;
        std::vector<ComponentInfo>   m_Infos;
    };
}
