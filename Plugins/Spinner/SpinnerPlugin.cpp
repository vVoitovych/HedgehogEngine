// Spinner: the sample plugin. One component, one system, registered through the PluginRegistrar
// and undone by it when the plugin unloads. Copy this folder to start a plugin of your own.

#include "SpinnerComponent.hpp"
#include "SpinnerSystem.hpp"

#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Plugins/PluginApi.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

namespace
{
    bool RegisterSpinner(HedgehogEngine::PluginRegistrar& registrar)
    {
        return registrar.RegisterReflectedComponent<Spinner::SpinnerComponent>(EcsSerialization::ComponentDesc{
                   .Key             = "SpinnerComponent",
                   .DisplayName     = "Spinner",
                   .Category        = "Samples",
                   .Icon            = "transform",
                   .EnabledProperty = "Enabled" }) &&
               registrar.RegisterSystem<Spinner::SpinnerSystem, Spinner::SpinnerComponent, HedgehogEngine::TransformComponent>() !=
                   nullptr;
    }
}

HH_PLUGIN("Spinner", "1.0.0", &RegisterSpinner, nullptr)
