#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltLayers.hpp"
#include "JoltRuntime.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>

namespace HP
{
    namespace
    {
        // Jolt's scratch memory for one step.
        constexpr uint32_t TEMP_ALLOCATOR_BYTES = 10 * 1024 * 1024;
        // Room for body pairs and contacts per body; Jolt reports running out when a step needs more.
        constexpr uint32_t PAIRS_PER_BODY = 4;
        constexpr uint32_t MIN_PAIRS      = 1024;

        static_assert(MAX_BODIES_LIMIT == JPH::PhysicsSystem::cMaxBodiesLimit, "HP::MAX_BODIES_LIMIT must match Jolt's");

        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }
    }

    // Declared in the order things are built, so they are destroyed in reverse: the physics system
    // before the job system, allocator and filters it refers to.
    struct JoltState
    {
        BroadPhaseLayerMap                   LayerMap;
        ObjectVsBroadPhaseFilter             BroadPhaseFilter;
        ObjectLayerPairFilter                PairFilter;
        std::unique_ptr<JPH::TempAllocator>  TempAllocator;
        std::unique_ptr<JPH::JobSystem>      JobSystem;
        std::unique_ptr<JPH::PhysicsSystem>  System;
        int32_t                              WorkerThreads = 0;
    };

    PhysicsWorld::PhysicsWorld() = default;

    PhysicsWorld::~PhysicsWorld() { Shutdown(); }

    bool PhysicsWorld::Init(const PhysicsWorldDesc& desc)
    {
        Shutdown();
        if (desc.MaxBodies == 0 || desc.MaxBodies > MAX_BODIES_LIMIT)
        {
            LOGERROR("[Physics] The world cannot hold", desc.MaxBodies, "bodies: 1 to", MAX_BODIES_LIMIT, "are allowed.");
            return false;
        }
        if (!IsFinite(desc.Gravity))
        {
            LOGERROR("[Physics] The world's gravity is not finite.");
            return false;
        }
        if (desc.WorkerThreads < 0)
        {
            LOGERROR("[Physics] The world cannot run on", desc.WorkerThreads, "worker threads: 0 or more are allowed.");
            return false;
        }

        AcquireJoltRuntime();
        auto state = std::make_unique<JoltState>();
        state->PairFilter.SetMatrix(desc.Collisions);
        state->TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(TEMP_ALLOCATOR_BYTES);
        if (desc.WorkerThreads == 0)
            state->JobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
        else
            state->JobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
                                                                          desc.WorkerThreads);
        state->WorkerThreads = desc.WorkerThreads;

        const uint32_t pairs = std::max(MIN_PAIRS, desc.MaxBodies * PAIRS_PER_BODY);
        state->System        = std::make_unique<JPH::PhysicsSystem>();
        state->System->Init(desc.MaxBodies, 0, pairs, pairs, state->LayerMap, state->BroadPhaseFilter, state->PairFilter);
        state->System->SetGravity(ToJolt(desc.Gravity));

        m_State = std::move(state);
        return true;
    }

    void PhysicsWorld::Shutdown()
    {
        if (!m_State)
            return;
        m_State.reset();
        ReleaseJoltRuntime();
    }

    bool PhysicsWorld::IsInitialized() const { return m_State != nullptr; }

    void PhysicsWorld::SetGravity(const HM::Vector3& gravity)
    {
        if (!m_State)
            return;
        if (!IsFinite(gravity))
        {
            LOGWARNING("[Physics] A gravity that is not finite is ignored.");
            return;
        }
        m_State->System->SetGravity(ToJolt(gravity));
    }

    HM::Vector3 PhysicsWorld::GetGravity() const
    {
        return m_State ? FromJolt(m_State->System->GetGravity()) : HM::Vector3(0.0f, 0.0f, 0.0f);
    }

    void PhysicsWorld::SetCollisionMatrix(const CollisionMatrix& matrix)
    {
        if (m_State)
            m_State->PairFilter.SetMatrix(matrix);
    }

    CollisionMatrix PhysicsWorld::GetCollisionMatrix() const
    {
        return m_State ? m_State->PairFilter.GetMatrix() : MakeFullCollisionMatrix();
    }

    uint32_t PhysicsWorld::GetBodyCount() const { return m_State ? m_State->System->GetNumBodies() : 0; }

    int32_t PhysicsWorld::GetWorkerThreadCount() const { return m_State ? m_State->WorkerThreads : 0; }

    bool PhysicsWorld::Step(float dt)
    {
        if (!m_State || !std::isfinite(dt) || dt <= 0.0f)
            return false;
        const JPH::EPhysicsUpdateError error =
            m_State->System->Update(dt, 1, m_State->TempAllocator.get(), m_State->JobSystem.get());
        if (error == JPH::EPhysicsUpdateError::None)
            return true;
        LOGERROR("[Physics] The step ran out of room (Jolt error", static_cast<uint32_t>(error), "); raise the world's MaxBodies.");
        return false;
    }
}
