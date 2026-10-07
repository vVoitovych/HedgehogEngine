#pragma once

#include "ContactQueue.hpp"
#include "JoltLayers.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystem.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace HP
{
    // What the world remembers of a body for its contact events: an Exit names only body ids, and
    // may come in the step after the body was destroyed.
    struct BodyRecord
    {
        uint64_t UserData = 0;
        bool     IsSensor = false;
    };

    // What a running PhysicsWorld owns. Declared in the order things are built, so they are
    // destroyed in reverse: the physics system before the job system, allocator, filters and contact
    // queue it uses.
    struct JoltState
    {
        BroadPhaseLayerMap                  LayerMap;
        ObjectVsBroadPhaseFilter            BroadPhaseFilter;
        ObjectLayerPairFilter               PairFilter;
        ContactQueue                        Contacts;
        std::unique_ptr<JPH::TempAllocator> TempAllocator;
        std::unique_ptr<JPH::JobSystem>     JobSystem;
        std::unique_ptr<JPH::PhysicsSystem> System;
        int32_t                             WorkerThreads = 0;

        // Every body made, by its handle's value. A destroyed body's record waits in Destroyed until
        // the next step has reported its Exits.
        std::unordered_map<uint32_t, BodyRecord> Records;
        std::vector<uint32_t>                    Destroyed;

        // The events of the steps since the last drain.
        std::vector<ContactEvent> Pending;
        // Jolt removes a pair's contact when its bodies fall asleep and adds it again when they wake:
        // those pairs wait here (by MakePairKey) instead of giving an Exit and an Enter. The flag is
        // set once a step has ended with one of them awake and no new contact; if the next step
        // brings none either, they have parted.
        std::map<uint64_t, bool> SleepingContacts;
    };
}
