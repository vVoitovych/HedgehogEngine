#pragma once

#include <cassert>
#include <typeindex>
#include <unordered_map>

namespace ECS
{
    // Shared objects a system needs but does not own (an event bus, a file system, an audio
    // engine), found by type, so systems take no constructor or Update arguments for them and the
    // ECS stays free of what they are. Pointers are non-owning: whoever registers a service keeps it
    // alive for as long as any system that uses it is registered, and unregisters it before it goes.
    class ServiceRegistry
    {
    public:
        // One service per type; registering a type twice asserts and keeps the first.
        template<typename T>
        void Register(T& service)
        {
            const std::type_index typeId = typeid(T);
            assert(m_Services.find(typeId) == m_Services.end() && "Service already registered.");
            m_Services.insert({ typeId, &service });
        }

        // Returns whether a service of type T was registered.
        template<typename T>
        bool Unregister()
        {
            return m_Services.erase(typeid(T)) > 0;
        }

        // The service of type T, or nullptr when there is none.
        template<typename T>
        [[nodiscard]] T* Find() const
        {
            const auto found = m_Services.find(typeid(T));
            return found == m_Services.end() ? nullptr : static_cast<T*>(found->second);
        }

        // The service of type T, which must be registered.
        template<typename T>
        [[nodiscard]] T& Get() const
        {
            T* service = Find<T>();
            assert(service && "Service used before being registered.");
            return *service;
        }

        template<typename T>
        [[nodiscard]] bool Has() const
        {
            return m_Services.find(typeid(T)) != m_Services.end();
        }

    private:
        std::unordered_map<std::type_index, void*> m_Services;
    };
}
