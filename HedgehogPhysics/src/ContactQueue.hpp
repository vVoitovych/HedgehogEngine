#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/ContactListener.h>

#include "HedgehogPhysics/api/PhysicsEvents.hpp"

#include <mutex>
#include <vector>

namespace HP
{
    // Jolt's contact listener. Jolt calls it on its worker threads during a step, so it only appends
    // to a vector under a mutex: no engine code ever runs on a Jolt thread. An Exit names only the
    // two bodies (Jolt gives nothing else once they part); the world fills in their user data when
    // it drains the queue.
    class ContactQueue final : public JPH::ContactListener
    {
    public:
        void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold,
                            JPH::ContactSettings& settings) override;
        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override;

        // The events queued since the last call, in the order Jolt's threads happened to give them.
        void Take(std::vector<ContactEvent>& out);
        void Clear();

    private:
        std::mutex                m_Mutex;
        std::vector<ContactEvent> m_Events;
    };
}
