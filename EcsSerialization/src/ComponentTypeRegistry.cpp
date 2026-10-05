#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace EcsSerialization
{
    bool ComponentTypeRegistry::Unregister(std::string_view key)
    {
        const auto it = std::find_if(m_Infos.begin(), m_Infos.end(), [key](const ComponentInfo& info) { return info.Key == key; });
        if (it == m_Infos.end() || !it->m_UnregisterFromEcs(m_Ecs))
            return false;
        m_Serializers.Unregister(key);
        m_Infos.erase(it);
        return true;
    }

    const ComponentInfo* ComponentTypeRegistry::Find(std::string_view key) const
    {
        const auto it = std::find_if(m_Infos.begin(), m_Infos.end(), [key](const ComponentInfo& info) { return info.Key == key; });
        return it == m_Infos.end() ? nullptr : &*it;
    }

    bool ComponentTypeRegistry::CanRegister(ComponentInfo& info, bool knownToEcs) const
    {
        if (info.Key.empty())
        {
            ReportRefused(info.Key, "the key is empty");
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
