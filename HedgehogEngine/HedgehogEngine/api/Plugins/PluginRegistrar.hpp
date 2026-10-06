#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace HedgehogEngine
{
    // What a plugin registers through. Every registration that succeeds records how to undo it;
    // UnregisterAll undoes them, last first (so a system goes before the components its signature
    // names), and lets go of every record. The undo steps are made where the templates are
    // instantiated, so they are the plugin's own code: UnregisterAll must run while its module is
    // still loaded (the destructor runs it too, for a registrar that is simply dropped).
    //
    // Components are unregistered keeping their data (UnknownData::Keep), so an entity's values
    // stay in the scene and come back when the plugin registers the type again. A refused
    // registration records nothing and returns false, nullptr or SubscriptionId::Invalid.
    class PluginRegistrar
    {
    public:
        HEDGEHOG_ENGINE_API PluginRegistrar(EngineContext& engine, std::string pluginName);
        HEDGEHOG_ENGINE_API ~PluginRegistrar();

        PluginRegistrar(const PluginRegistrar&)            = delete;
        PluginRegistrar& operator=(const PluginRegistrar&) = delete;

        [[nodiscard]] HEDGEHOG_ENGINE_API const std::string& GetPluginName() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API EngineContext& GetEngine();
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::ECS& GetECS();
        [[nodiscard]] HEDGEHOG_ENGINE_API EcsSerialization::ComponentTypeRegistry& GetComponentTypes();
        [[nodiscard]] HEDGEHOG_ENGINE_API EventBus& GetEventBus();

        // A reflected component (T::GetProperties()) under desc.Key.
        template<typename T>
        bool RegisterReflectedComponent(EcsSerialization::ComponentDesc desc)
        {
            const std::string key = desc.Key;
            if (!GetComponentTypes().RegisterReflected<T>(std::move(desc)))
                return false;
            Record("component '" + key + "'",
                   [this, key] { return GetComponentTypes().Unregister(key, EcsSerialization::UnknownData::Keep); });
            return true;
        }

        // A component with its own serializer, which must add a handler of desc.Key.
        template<typename T>
        bool RegisterCustomComponent(EcsSerialization::ComponentDesc desc,
                                     const std::function<void(EcsSerialization::ComponentSerializerRegistry&)>& registerSerializer)
        {
            const std::string key = desc.Key;
            if (!GetComponentTypes().RegisterCustom<T>(std::move(desc), registerSerializer))
                return false;
            Record("component '" + key + "'",
                   [this, key] { return GetComponentTypes().Unregister(key, EcsSerialization::UnknownData::Keep); });
            return true;
        }

        // A system built from args, its signature the Components named (none: it sees no entity).
        // Refused when the ECS already has a system of type T.
        template<typename T, typename... Components, typename... Args>
        std::shared_ptr<T> RegisterSystem(Args&&... args)
        {
            ECS::ECS& ecs = GetECS();
            if (ecs.HasSystem<T>())
            {
                ReportRefused("a system of that type is already registered");
                return nullptr;
            }
            std::shared_ptr<T> system = ecs.RegisterSystem<T>(std::forward<Args>(args)...);
            if constexpr (sizeof...(Components) > 0)
            {
                ECS::Signature signature;
                (signature.set(ecs.GetComponentType<Components>()), ...);
                ecs.SetSystemSignature<T>(signature);
            }
            Record("a system", [this] { return GetECS().UnregisterSystem<T>(); });
            return system;
        }

        // A service the plugin owns and keeps alive until UnregisterAll. Refused when the ECS
        // already has a service of type T.
        template<typename T>
        bool RegisterService(T& service)
        {
            ECS::ServiceRegistry& services = GetECS().GetServices();
            if (services.Has<T>())
            {
                ReportRefused("a service of that type is already registered");
                return false;
            }
            services.Register(service);
            Record("a service", [this] { return GetECS().GetServices().Unregister<T>(); });
            return true;
        }

        template<typename TEvent>
        SubscriptionId Subscribe(std::function<void(const TEvent&)> handler)
        {
            const SubscriptionId id = GetEventBus().Subscribe<TEvent>(std::move(handler));
            Record("an event subscription", [this, id] { return GetEventBus().Unsubscribe(id); });
            return id;
        }

        // Undoes every registration, last first, logging one that cannot be undone (a system
        // while the ECS is dispatching, a component a system outside the plugin requires); safe
        // to call twice.
        HEDGEHOG_ENGINE_API void UnregisterAll();

        // How many registrations are recorded.
        [[nodiscard]] HEDGEHOG_ENGINE_API size_t GetRegistrationCount() const;

    private:
        struct Registration
        {
            std::string           What;
            std::function<bool()> Undo;
        };

        HEDGEHOG_ENGINE_API void Record(std::string what, std::function<bool()> undo);
        HEDGEHOG_ENGINE_API void ReportRefused(const std::string& why) const;

        EngineContext&            m_Engine;
        std::string               m_PluginName;
        std::vector<Registration> m_Registrations;
    };
}
