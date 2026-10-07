#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltLayers.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>

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

        // A query's layers: the bit of each body's physics layer.
        class LayerMaskFilter final : public JPH::ObjectLayerFilter
        {
        public:
            explicit LayerMaskFilter(uint16_t mask) : m_Mask(mask) {}

            bool ShouldCollide(JPH::ObjectLayer layer) const override
            {
                return (m_Mask >> GetPhysicsLayer(layer) & 1u) != 0;
            }

        private:
            uint16_t m_Mask;
        };

        class SensorFilter final : public JPH::BodyFilter
        {
        public:
            explicit SensorFilter(bool includeSensors) : m_IncludeSensors(includeSensors) {}

            bool ShouldCollideLocked(const JPH::Body& body) const override { return m_IncludeSensors || !body.IsSensor(); }

        private:
            bool m_IncludeSensors;
        };
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

    std::optional<RayHit> PhysicsWorld::CastRay(const HM::Vector3& origin, const HM::Vector3& direction, float maxDistance,
                                                uint16_t layerMask, bool includeSensors) const
    {
        if (!m_State || !IsFinite(origin) || !IsFinite(direction) || !std::isfinite(maxDistance) || maxDistance <= 0.0f)
            return std::nullopt;
        const float length = direction.LengthSlow();
        if (!std::isfinite(length) || length <= 0.0f)
            return std::nullopt;

        // Jolt's ray is its whole length; a hit's fraction is along it.
        const JPH::RRayCast   ray(JPH::RVec3(ToJolt(origin)), ToJolt(direction) * (maxDistance / length));
        JPH::RayCastResult    result;
        const LayerMaskFilter layers(layerMask);
        const SensorFilter    sensors(includeSensors);
        if (!m_State->System->GetNarrowPhaseQuery().CastRay(ray, result, {}, layers, sensors))
            return std::nullopt;

        const JPH::BodyLockRead lock(m_State->System->GetBodyLockInterface(), result.mBodyID);
        if (!lock.Succeeded())
            return std::nullopt;
        const JPH::RVec3 point = ray.GetPointOnRay(result.mFraction);

        RayHit hit;
        hit.Body     = BodyHandle{ result.mBodyID.GetIndexAndSequenceNumber() };
        hit.UserData = lock.GetBody().GetUserData();
        hit.Point    = FromJolt(JPH::Vec3(point));
        hit.Normal   = FromJolt(lock.GetBody().GetWorldSpaceSurfaceNormal(result.mSubShapeID2, point));
        hit.Distance = result.mFraction * maxDistance;
        return hit;
    }
}
