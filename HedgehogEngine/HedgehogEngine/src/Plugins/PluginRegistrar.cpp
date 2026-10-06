#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    PluginRegistrar::PluginRegistrar(EngineContext& engine, std::string pluginName)
        : m_Engine(engine)
        , m_PluginName(std::move(pluginName))
    {
    }

    PluginRegistrar::~PluginRegistrar()
    {
        UnregisterAll();
    }

    const std::string& PluginRegistrar::GetPluginName() const { return m_PluginName; }
    EngineContext&     PluginRegistrar::GetEngine() { return m_Engine; }
    ECS::ECS&          PluginRegistrar::GetECS() { return m_Engine.GetECS(); }
    EventBus&          PluginRegistrar::GetEventBus() { return m_Engine.GetEventBus(); }

    EcsSerialization::ComponentTypeRegistry& PluginRegistrar::GetComponentTypes()
    {
        return m_Engine.GetComponentTypes();
    }

    void PluginRegistrar::UnregisterAll()
    {
        // Each undo is taken off the list before it runs, so one that registers or unregisters
        // through this registrar cannot disturb the walk.
        while (!m_Registrations.empty())
        {
            Registration registration = std::move(m_Registrations.back());
            m_Registrations.pop_back();
            if (!registration.Undo())
                LOGERROR("[Plugin] " + m_PluginName + ": " + registration.What + " could not be unregistered.");
        }
    }

    size_t PluginRegistrar::GetRegistrationCount() const { return m_Registrations.size(); }

    void PluginRegistrar::Record(std::string what, std::function<bool()> undo)
    {
        m_Registrations.push_back(Registration{ std::move(what), std::move(undo) });
    }

    void PluginRegistrar::ReportRefused(const std::string& why) const
    {
        LOGERROR("[Plugin] " + m_PluginName + ": registration refused: " + why + ".");
    }
}
