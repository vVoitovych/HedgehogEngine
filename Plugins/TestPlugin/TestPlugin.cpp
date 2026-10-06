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
        int PluginEvents     = 0;
    };

    // An event only the plugin knows, so its bus channel is created by the plugin's code.
    struct TestPluginEvent
    {
        int Value = 0;
    };

    TestPluginService g_Service;
    bool              g_LeaveSubscription = false;

    // Counts frames into its entities' Ticks, and transform changes and its own events into the
    // service, through subscriptions it makes in OnRegister and drops in OnUnregister.
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
                m_EventSubscription =
                    m_Bus->Subscribe<TestPluginEvent>([](const TestPluginEvent&) { ++g_Service.PluginEvents; });
            }
        }

        void OnUnregister(ECS::ECS&) override
        {
            if (m_Bus)
            {
                m_Bus->Unsubscribe(m_Subscription);
                m_Bus->Unsubscribe(m_EventSubscription);
            }
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
        HedgehogEngine::SubscriptionId m_Subscription      = HedgehogEngine::SubscriptionId::Invalid;
        HedgehogEngine::SubscriptionId m_EventSubscription = HedgehogEngine::SubscriptionId::Invalid;
    };

    bool Register(HedgehogEngine::PluginRegistrar& registrar)
    {
        g_Service = TestPluginService{};
        const bool registered =
            registrar.RegisterReflectedComponent<TestPluginComponent>(EcsSerialization::ComponentDesc{
                .Key = "TestPluginComponent", .DisplayName = "Test plugin", .Category = "Test" }) &&
            registrar.RegisterSystem<TestPluginSystem, TestPluginComponent>() != nullptr &&
            registrar.RegisterService(g_Service);
        // The mistake the plugin manager guards against: a subscription nothing ever removes.
        if (registered && g_LeaveSubscription)
            registrar.GetEventBus().Subscribe<TestPluginEvent>([](const TestPluginEvent&) { ++g_Service.PluginEvents; });
        return registered;
    }
}

// Hooks for the engine's tests, which open the DLL beside the plugin manager to reach them.
extern "C" __declspec(dllexport) void HedgehogTestPluginLeaveSubscription(bool leave)
{
    g_LeaveSubscription = leave;
}

extern "C" __declspec(dllexport) void HedgehogTestPluginPublishEvent(HedgehogEngine::EventBus* bus)
{
    bus->Publish(TestPluginEvent{ 1 });
}

extern "C" __declspec(dllexport) int HedgehogTestPluginGetEventCount()
{
    return g_Service.PluginEvents;
}

#if defined(HH_TEST_PLUGIN_OLD_API)
// What an older engine's HH_PLUGIN wrote: plugin API version 0.
extern "C" __declspec(dllexport) const HedgehogEngine::HedgehogPluginInfo* HedgehogPluginEntry()
{
    static const HedgehogEngine::HedgehogPluginInfo info{
        0, HedgehogEngine::GetCompilerVersion(), HedgehogEngine::GetDebugBuild(), "HedgehogTestPluginOldApi", "0.9.0", &Register, nullptr
    };
    return &info;
}
#else
HH_PLUGIN("HedgehogTestPlugin", "1.0.0", &Register, nullptr)
#endif
