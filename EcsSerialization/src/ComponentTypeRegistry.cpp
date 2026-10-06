#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "Logger/api/Logger.hpp"

#include "yaml-cpp/yaml.h"

#include <algorithm>

namespace EcsSerialization
{
    namespace
    {
        // Every live entity; registering and unregistering are rare, so a scan is enough.
        template<typename Fn>
        void ForEachEntity(const ECS::ECS& ecs, Fn&& fn)
        {
            for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
            {
                if (ecs.IsAlive(entity))
                    fn(entity);
            }
        }

        bool HasStore(const ECS::ECS& ecs)
        {
            return ecs.IsComponentRegistered<UnknownComponentsComponent>();
        }
    }

    bool ComponentTypeRegistry::Unregister(std::string_view key, UnknownData data)
    {
        const auto it = std::find_if(m_Infos.begin(), m_Infos.end(), [key](const ComponentInfo& info) { return info.Key == key; });
        if (it == m_Infos.end())
            return false;

        std::vector<ECS::Entity> kept;
        if (data == UnknownData::Keep)
            kept = KeepData(*it);
        if (!it->m_UnregisterFromEcs(m_Ecs))
        {
            DropKeptEntries(it->Key, kept); // the components stay, so the copies go
            return false;
        }
        m_Serializers.Unregister(key);
        m_Infos.erase(it);
        return true;
    }

    std::vector<ECS::Entity> ComponentTypeRegistry::KeepData(const ComponentInfo& info)
    {
        std::vector<ECS::Entity>  kept;
        const ComponentHandler*   handler = m_Serializers.FindHandler(info.Key);
        if (!handler || !HasStore(m_Ecs))
        {
            LOGWARNING("[Components] Component type '" + info.Key + "' is unregistered without keeping its data: " +
                       (handler ? "the ECS has no UnknownComponentsComponent" : "no handler writes it") + ".");
            return kept;
        }
        ForEachEntity(m_Ecs, [&](ECS::Entity entity)
        {
            if (!info.Has(m_Ecs, entity))
                return;
            YAML::Emitter out;
            out << YAML::BeginMap;
            handler->Serialize(out, m_Ecs, entity);
            out << YAML::EndMap;
            YAML::Emitter value;
            value << YAML::Load(out.c_str())[info.Key];

            if (!m_Ecs.HasComponent<UnknownComponentsComponent>(entity))
                m_Ecs.AddComponent(entity, UnknownComponentsComponent{});
            m_Ecs.GetComponent<UnknownComponentsComponent>(entity).Entries.push_back(UnknownComponent{ info.Key, value.c_str() });
            kept.push_back(entity);
        });
        return kept;
    }

    void ComponentTypeRegistry::DropKeptEntries(const std::string& key, const std::vector<ECS::Entity>& entities)
    {
        for (const ECS::Entity entity : entities)
        {
            auto& entries = m_Ecs.GetComponent<UnknownComponentsComponent>(entity).Entries;
            std::erase_if(entries, [&key](const UnknownComponent& entry) { return entry.Key == key; });
            if (entries.empty())
                m_Ecs.RemoveComponent<UnknownComponentsComponent>(entity);
        }
    }

    void ComponentTypeRegistry::AdoptKeptData(const std::string& key)
    {
        const ComponentHandler* handler = m_Serializers.FindHandler(key);
        if (!handler || !HasStore(m_Ecs))
            return;

        std::vector<ECS::Entity> adopted;
        ForEachEntity(m_Ecs, [&](ECS::Entity entity)
        {
            if (!m_Ecs.HasComponent<UnknownComponentsComponent>(entity))
                return;
            for (const UnknownComponent& entry : m_Ecs.GetComponent<UnknownComponentsComponent>(entity).Entries)
            {
                if (entry.Key != key)
                    continue;
                try
                {
                    handler->Deserialize(m_Ecs, entity, YAML::Load(entry.Yaml));
                    adopted.push_back(entity);
                }
                catch (const YAML::Exception& e)
                {
                    LOGWARNING("[Components] Entity " + std::to_string(entity) + ": the kept data of '" + key +
                               "' does not read (" + e.what() + "); it stays kept.");
                }
                break;
            }
        });
        DropKeptEntries(key, adopted);
    }

    const ComponentInfo* ComponentTypeRegistry::Find(std::string_view key) const
    {
        const auto it = std::find_if(m_Infos.begin(), m_Infos.end(), [key](const ComponentInfo& info) { return info.Key == key; });
        return it == m_Infos.end() ? nullptr : &*it;
    }

    bool ComponentTypeRegistry::CanRegister(ComponentInfo& info, bool knownToEcs, bool isStore) const
    {
        if (info.Key.empty())
        {
            ReportRefused(info.Key, "the key is empty");
            return false;
        }
        if (IsReservedEntityKey(info.Key) || (info.Key == UNKNOWN_COMPONENTS_KEY && !isStore))
        {
            ReportRefused(info.Key, "the key is reserved");
            return false;
        }
        if (Find(info.Key) != nullptr || m_Serializers.FindHandler(info.Key) != nullptr)
        {
            ReportRefused(info.Key, "the key is already registered");
            return false;
        }
        if (knownToEcs)
        {
            ReportRefused(info.Key, "the ECS already has the type");
            return false;
        }
        if (!info.EnabledProperty.empty())
        {
            const auto found = std::find_if(info.Properties.begin(), info.Properties.end(),
                                            [&info](const Reflection::PropertyDescriptor& prop)
                                            { return info.EnabledProperty == prop.name && prop.type == Reflection::TypeTag::Bool; });
            if (found == info.Properties.end())
            {
                ReportRefused(info.Key, "its EnabledProperty '" + info.EnabledProperty + "' is not a reflected bool property");
                return false;
            }
            info.m_EnabledProperty = &*found;
        }
        return true;
    }

    void ComponentTypeRegistry::ReportRefused(const std::string& key, const std::string& why)
    {
        LOGERROR("[Components] Component type '" + key + "' is not registered: " + why + ".");
    }
}
