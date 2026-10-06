// HedgehogTestPlugin: the smallest plugin that uses every kind of registration, for the engine's
// plugin tests. It is not a sample to copy; Plugins/Spinner will be.

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Plugins/PluginApi.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "ECS/api/SystemPhase.hpp"

#include <cstdint>
#include <span>

namespace
{
    // A reflected component: Value is saved, Ticks counts the frames the system ran over it.
    struct TestPluginComponent
    {
        float   Value = 1.0f;
        int32_t Ticks = 0;

        static void* ValueAccessor(void* c) { return &static_cast<TestPluginComponent*>(c)->Value; }
        static void* TicksAccessor(void* c) { return &static_cast<TestPluginComponent*>(c)->Ticks; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::Float, "Value", ValueAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
                { Reflection::TypeTag::Int, "Ticks", TicksAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    // A service the plugin owns.
    struct TestPluginService
    {
        int TransformChanges = 0;
    };

    TestPluginService g_Service;

    // Counts frames into its entities' Ticks, and transform changes into the service, through a
    // subscription it makes in OnRegister and drops in OnUnregister.
    class TestPluginSystem : public ECS::System
    {
    public:
        void OnRegister(ECS::ECS& ecs) override
        {
            m_Bus = ecs.GetServices().Find<HedgehogEngine::EventBus>();
            if (m_Bus)
            {
                m_Subscription = m_Bus->Subscribe<HedgehogEngine::TransformChangedEvent>(
                    [](const HedgehogEngine::TransformChangedEvent&) { ++g_Service.TransformChanges; });
            }
        }

        void OnUnregister(ECS::ECS&) override
        {
            if (m_Bus)
                m_Bus->Unsubscribe(m_Subscription);
            m_Bus = nullptr;
        }

        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Late; }

        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext&) override
        {
            for (const ECS::Entity entity : m_Entities)
                ++ecs.GetComponent<TestPluginComponent>(entity).Ticks;
        }

    private:
        HedgehogEngine::EventBus*      m_Bus          = nullptr;
        HedgehogEngine::SubscriptionId m_Subscription = HedgehogEngine::SubscriptionId::Invalid;
    };

    bool Register(HedgehogEngine::PluginRegistrar& registrar)
    {
        g_Service = TestPluginService{};
        return registrar.RegisterReflectedComponent<TestPluginComponent>(EcsSerialization::ComponentDesc{
                   .Key = "TestPluginComponent", .DisplayName = "Test plugin", .Category = "Test" }) &&
               registrar.RegisterSystem<TestPluginSystem, TestPluginComponent>() != nullptr &&
               registrar.RegisterService(g_Service);
    }
}

HH_PLUGIN("HedgehogTestPlugin", "1.0.0", &Register, nullptr)
