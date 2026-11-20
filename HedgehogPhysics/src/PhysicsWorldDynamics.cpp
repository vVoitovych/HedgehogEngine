#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>

#include <cmath>

namespace HP
{
    namespace
    {
        JPH::BodyID ToBodyID(BodyHandle handle) { return JPH::BodyID(handle.Value); }

        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }
    }

    bool PhysicsWorld::IsMotion(BodyHandle body, bool kinematicToo) const
    {
        if (!IsValid(body))
            return false;
        const JPH::EMotionType motion = m_State->System->GetBodyInterface().GetMotionType(ToBodyID(body));
        return motion == JPH::EMotionType::Dynamic || (kinematicToo && motion == JPH::EMotionType::Kinematic);
    }

    HM::Vector3 PhysicsWorld::GetLinearVelocity(BodyHandle body) const
    {
        if (!IsValid(body))
            return HM::Vector3(0.0f, 0.0f, 0.0f);
        return FromJolt(m_State->System->GetBodyInterface().GetLinearVelocity(ToBodyID(body)));
    }

    void PhysicsWorld::SetLinearVelocity(BodyHandle body, const HM::Vector3& velocity)
    {
        if (!IsFinite(velocity) || !IsMotion(body, true))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.SetLinearVelocity(ToBodyID(body), ToJolt(velocity));
        bodies.ActivateBody(ToBodyID(body));
    }

    HM::Vector3 PhysicsWorld::GetAngularVelocity(BodyHandle body) const
    {
        if (!IsValid(body))
            return HM::Vector3(0.0f, 0.0f, 0.0f);
        return FromJolt(m_State->System->GetBodyInterface().GetAngularVelocity(ToBodyID(body)));
    }

    void PhysicsWorld::SetAngularVelocity(BodyHandle body, const HM::Vector3& velocity)
    {
        if (!IsFinite(velocity) || !IsMotion(body, true))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.SetAngularVelocity(ToBodyID(body), ToJolt(velocity));
        bodies.ActivateBody(ToBodyID(body));
    }

    void PhysicsWorld::AddForce(BodyHandle body, const HM::Vector3& force)
    {
        if (IsFinite(force) && IsMotion(body, false))
            m_State->System->GetBodyInterface().AddForce(ToBodyID(body), ToJolt(force));
    }

    void PhysicsWorld::AddForceAtPoint(BodyHandle body, const HM::Vector3& force, const HM::Vector3& point)
    {
        if (IsFinite(force) && IsFinite(point) && IsMotion(body, false))
            m_State->System->GetBodyInterface().AddForce(ToBodyID(body), ToJolt(force), JPH::RVec3(ToJolt(point)));
    }

    void PhysicsWorld::AddTorque(BodyHandle body, const HM::Vector3& torque)
    {
        if (IsFinite(torque) && IsMotion(body, false))
            m_State->System->GetBodyInterface().AddTorque(ToBodyID(body), ToJolt(torque));
    }

    void PhysicsWorld::AddImpulse(BodyHandle body, const HM::Vector3& impulse)
    {
        if (!IsFinite(impulse) || !IsMotion(body, false))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.AddImpulse(ToBodyID(body), ToJolt(impulse));
        bodies.ActivateBody(ToBodyID(body));
    }

    void PhysicsWorld::AddImpulseAtPoint(BodyHandle body, const HM::Vector3& impulse, const HM::Vector3& point)
    {
        if (!IsFinite(impulse) || !IsFinite(point) || !IsMotion(body, false))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.AddImpulse(ToBodyID(body), ToJolt(impulse), JPH::RVec3(ToJolt(point)));
        bodies.ActivateBody(ToBodyID(body));
    }

    void PhysicsWorld::AddAngularImpulse(BodyHandle body, const HM::Vector3& impulse)
    {
        if (!IsFinite(impulse) || !IsMotion(body, false))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.AddAngularImpulse(ToBodyID(body), ToJolt(impulse));
        bodies.ActivateBody(ToBodyID(body));
    }

    float PhysicsWorld::GetMass(BodyHandle body) const
    {
        if (!IsMotion(body, false))
            return 0.0f;
        const JPH::BodyLockRead lock(m_State->System->GetBodyLockInterface(), ToBodyID(body));
        if (!lock.Succeeded())
            return 0.0f;
        const float inverseMass = lock.GetBody().GetMotionProperties()->GetInverseMass();
        return inverseMass > 0.0f ? 1.0f / inverseMass : 0.0f;
    }
}
