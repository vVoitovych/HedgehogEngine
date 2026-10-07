#pragma once

#include "JoltLayers.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <memory>

namespace HP
{
    // What a running PhysicsWorld owns. Declared in the order things are built, so they are
    // destroyed in reverse: the physics system before the job system, allocator and filters it uses.
    struct JoltState
    {
        BroadPhaseLayerMap                  LayerMap;
        ObjectVsBroadPhaseFilter            BroadPhaseFilter;
        ObjectLayerPairFilter               PairFilter;
        std::unique_ptr<JPH::TempAllocator> TempAllocator;
        std::unique_ptr<JPH::JobSystem>     JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> System;
        int32_t                             WorkerThreads = 0;
    };
}
