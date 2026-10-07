#include "ContactQueue.hpp"

#include "JoltConversions.hpp"

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/Shape/SubShapeIDPair.h>

namespace HP
{
    void ContactQueue::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold,
                                      JPH::ContactSettings& /*settings*/)
    {
        ContactEvent event;
        event.Type      = ContactEventType::Enter;
        event.BodyA     = BodyHandle{ body1.GetID().GetIndexAndSequenceNumber() };
        event.BodyB     = BodyHandle{ body2.GetID().GetIndexAndSequenceNumber() };
        event.UserDataA = body1.GetUserData();
        event.UserDataB = body2.GetUserData();
        event.IsSensor  = body1.IsSensor() || body2.IsSensor();
        // Halfway between the two surfaces at the first contact point; Jolt's normal points from
        // body 1 towards body 2.
        JPH::RVec3 point = manifold.mBaseOffset;
        if (!manifold.mRelativeContactPointsOn1.empty())
            point = 0.5f * (manifold.GetWorldSpaceContactPointOn1(0) + manifold.GetWorldSpaceContactPointOn2(0));
        event.Point  = FromJolt(JPH::Vec3(point));
        event.Normal = FromJolt(manifold.mWorldSpaceNormal);

        const std::lock_guard lock(m_Mutex);
        m_Events.push_back(event);
    }

    void ContactQueue::OnContactRemoved(const JPH::SubShapeIDPair& pair)
    {
        ContactEvent event;
        event.Type  = ContactEventType::Exit;
        event.BodyA = BodyHandle{ pair.GetBody1ID().GetIndexAndSequenceNumber() };
        event.BodyB = BodyHandle{ pair.GetBody2ID().GetIndexAndSequenceNumber() };

        const std::lock_guard lock(m_Mutex);
        m_Events.push_back(event);
    }

    void ContactQueue::Take(std::vector<ContactEvent>& out)
    {
        const std::lock_guard lock(m_Mutex);
        out.insert(out.end(), m_Events.begin(), m_Events.end());
        m_Events.clear();
    }

    void ContactQueue::Clear()
    {
        const std::lock_guard lock(m_Mutex);
        m_Events.clear();
    }
}
