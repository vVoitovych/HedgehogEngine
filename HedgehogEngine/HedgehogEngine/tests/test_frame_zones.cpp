#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "Profiler/api/Profiler.hpp"

#include <string>

using namespace HedgehogEngine;

namespace
{
    const Profiling::ZoneRecord* FindZone(const Profiling::ThreadRecord& thread, const std::string& name)
    {
        for (const Profiling::ZoneRecord& zone : thread.Zones)
        {
            if (name == zone.Name)
                return &zone;
        }
        return nullptr;
    }

    bool Inside(const Profiling::ZoneRecord& child, const Profiling::ZoneRecord& parent)
    {
        return child.Depth > parent.Depth && child.StartNs >= parent.StartNs
            && child.StartNs + child.DurationNs <= parent.StartNs + parent.DurationNs;
    }
}

TEST_CASE("Frame zones - one Play frame records UpdateContext, its phases, the fixed steps and every system")
{
    EngineContext context;
    const ECS::Entity light = context.GetSceneManager().CreateGameObject();
    context.GetECS().AddComponent(light, LightComponent{});
    context.GetEventBus().Publish(TransformChangedEvent{ light });
    REQUIRE(context.Play());

    Profiling::SetCurrentThreadName("FrameZonesTest");
    Profiling::Clear();
    Profiling::MarkFrame();
    Profiling::SetEnabled(true);
    context.UpdateContext(1.0f, 1.0f / 30.0f); // two fixed steps
    Profiling::MarkFrame();
    Profiling::SetEnabled(false);

    REQUIRE(Profiling::GetFrameCount() >= 1);
    const Profiling::ThreadRecord* thread = nullptr;
    for (const Profiling::ThreadRecord& record : Profiling::GetFrame(0).Threads)
    {
        if (record.ThreadName == "FrameZonesTest")
            thread = &record;
    }
    REQUIRE(thread != nullptr);

    const Profiling::ZoneRecord* update = FindZone(*thread, "UpdateContext");
    REQUIRE(update != nullptr);
    CHECK(update->Depth == 0);
    for (const char* phase : { "Phase: Simulation", "Phase: Animation", "Phase: Transform", "Phase: Late", "Phase: Sync" })
    {
        CAPTURE(phase);
        const Profiling::ZoneRecord* zone = FindZone(*thread, phase);
        REQUIRE(zone != nullptr);
        CHECK(Inside(*zone, *update));
    }

    size_t fixedSteps = 0;
    for (const Profiling::ZoneRecord& zone : thread->Zones)
        fixedSteps += std::string(zone.Name) == "FixedStep" ? 1 : 0;
    CHECK(fixedSteps == 2);
    REQUIRE(FindZone(*thread, "OnUpdate") != nullptr);
    CHECK(Inside(*FindZone(*thread, "OnUpdate"), *FindZone(*thread, "Phase: Simulation")));

    // Systems under their class names, inside the phase that runs them.
    for (const char* system : { "TransformSystem", "HierarchySystem", "LightSystem", "PhysicsSystem" })
    {
        CAPTURE(system);
        CHECK(FindZone(*thread, system) != nullptr);
    }
    const Profiling::ZoneRecord* transform = nullptr;
    for (const Profiling::ZoneRecord& zone : thread->Zones)
    {
        if (std::string(zone.Name) == "TransformSystem" && Inside(zone, *FindZone(*thread, "Phase: Transform")))
            transform = &zone;
    }
    CHECK(transform != nullptr);
    Profiling::Clear();
}

TEST_CASE("Frame zones - nothing is recorded while the profiler is off")
{
    EngineContext context;
    Profiling::SetCurrentThreadName("FrameZonesOffTest");
    Profiling::Clear();
    Profiling::SetEnabled(false);
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    Profiling::MarkFrame();
    CHECK(Profiling::GetFrameCount() == 0);
}
