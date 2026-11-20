#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltLayers.hpp"
#include "JoltShapes.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

#include <algorithm>
#include <cmath>

namespace HP
{
    namespace
    {
        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }

        bool IsPositive(float value) { return std::isfinite(value) && value > 0.0f; }

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

        // The query shape's Jolt shape, or null for one a query refuses (a height field, a size, scale,
        // centre or pose that is not finite and positive).
        JPH::Ref<JPH::Shape> MakeQueryShape(const ShapeDesc& shape, const BodyPose& pose)
        {
            const HM::Vector3& scale = shape.Scale;
            if (!IsPositive(std::abs(scale.x())) || !IsPositive(std::abs(scale.y())) || !IsPositive(std::abs(scale.z())) ||
                !IsFinite(shape.Center) || !IsFinite(pose.Position) || !std::isfinite(pose.Rotation.x()) ||
                !std::isfinite(pose.Rotation.y()) || !std::isfinite(pose.Rotation.z()) || !std::isfinite(pose.Rotation.w()))
                return nullptr;
            switch (shape.Type)
            {
            case ShapeType::Box:
            {
                const HM::Vector3& half = shape.HalfExtents;
                if (!IsPositive(half.x()) || !IsPositive(half.y()) || !IsPositive(half.z()))
                    return nullptr;
            }
                break;
            case ShapeType::Sphere:
                if (!IsPositive(shape.Radius))
                    return nullptr;
                break;
            case ShapeType::Capsule:
                if (!IsPositive(shape.Radius) || !std::isfinite(shape.HalfHeight) || shape.HalfHeight < 0.0f)
                    return nullptr;
                break;
            case ShapeType::HeightField:
                return nullptr;
            }
            return MakeShape(shape);
        }

        JPH::RMat44 ToWorldTransform(const BodyPose& pose)
        {
            return JPH::RMat44::sRotationTranslation(ToJolt(pose.Rotation), JPH::RVec3(ToJolt(pose.Position)));
        }
    }

    std::optional<RayHit> PhysicsWorld::CastRay(const HM::Vector3& origin, const HM::Vector3& direction, float maxDistance,
                                                uint16_t layerMask, bool includeSensors) const
    {
        if (!m_State || !IsFinite(origin) || !IsFinite(direction) || !IsPositive(maxDistance))
            return std::nullopt;
        const float length = direction.LengthSlow();
        if (!IsPositive(length))
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

    std::optional<ShapeHit> PhysicsWorld::CastShape(const ShapeDesc& shape, const BodyPose& pose,
                                                    const HM::Vector3& direction, float maxDistance, uint16_t layerMask,
                                                    bool includeSensors) const
    {
        if (!m_State || !IsFinite(direction) || !IsPositive(maxDistance))
            return std::nullopt;
        const float length = direction.LengthSlow();
        if (!IsPositive(length))
            return std::nullopt;
        const JPH::Ref<JPH::Shape> joltShape = MakeQueryShape(shape, pose);
        if (!joltShape)
            return std::nullopt;

        const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
            joltShape, JPH::Vec3::sReplicate(1.0f), ToWorldTransform(pose), ToJolt(direction) * (maxDistance / length));
        JPH::ShapeCastSettings settings;
        // A body the shape already touches counts, at distance 0, whichever way the shape moves.
        settings.mReturnDeepestPoint    = true;
        settings.mBackFaceModeConvex    = JPH::EBackFaceMode::CollideWithBackFaces;
        settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
        const LayerMaskFilter                                       layers(layerMask);
        const SensorFilter                                          sensors(includeSensors);
        m_State->System->GetNarrowPhaseQuery().CastShape(cast, settings, JPH::RVec3::sZero(), collector, {}, layers,
                                                         sensors);
        if (!collector.HadHit())
            return std::nullopt;

        const JPH::ShapeCastResult& result = collector.mHit;
        const JPH::BodyLockRead     lock(m_State->System->GetBodyLockInterface(), result.mBodyID2);
        if (!lock.Succeeded())
            return std::nullopt;
        ShapeHit hit;
        hit.Body     = BodyHandle{ result.mBodyID2.GetIndexAndSequenceNumber() };
        hit.UserData = lock.GetBody().GetUserData();
        hit.Point    = FromJolt(result.mContactPointOn2);
        // The penetration axis points from the shape into the body.
        const JPH::Vec3 axis = result.mPenetrationAxis;
        hit.Normal   = axis.IsNearZero() ? HM::Vector3(0.0f, 0.0f, 0.0f) : FromJolt(-axis.Normalized());
        hit.Distance = result.mFraction * maxDistance;
        return hit;
    }

    void PhysicsWorld::Overlap(const ShapeDesc& shape, const BodyPose& pose, std::vector<OverlapHit>& out,
                               uint16_t layerMask, bool includeSensors) const
    {
        out.clear();
        if (!m_State)
            return;
        const JPH::Ref<JPH::Shape> joltShape = MakeQueryShape(shape, pose);
        if (!joltShape)
            return;

        const JPH::RMat44 centerOfMass = ToWorldTransform(pose).PreTranslated(joltShape->GetCenterOfMass());
        JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
        const LayerMaskFilter                                      layers(layerMask);
        const SensorFilter                                         sensors(includeSensors);
        m_State->System->GetNarrowPhaseQuery().CollideShape(joltShape, JPH::Vec3::sReplicate(1.0f), centerOfMass,
                                                            JPH::CollideShapeSettings(), JPH::RVec3::sZero(), collector,
                                                            {}, layers, sensors);

        for (const JPH::CollideShapeResult& result : collector.mHits)
        {
            const BodyHandle body{ result.mBodyID2.GetIndexAndSequenceNumber() };
            if (std::any_of(out.begin(), out.end(), [body](const OverlapHit& hit) { return hit.Body == body; }))
                continue;
            const JPH::BodyLockRead lock(m_State->System->GetBodyLockInterface(), result.mBodyID2);
            if (lock.Succeeded())
                out.push_back(OverlapHit{ body, lock.GetBody().GetUserData() });
        }
        std::sort(out.begin(), out.end(), [](const OverlapHit& a, const OverlapHit& b) {
            return a.UserData != b.UserData ? a.UserData < b.UserData : a.Body.Value < b.Body.Value;
        });
    }
}
