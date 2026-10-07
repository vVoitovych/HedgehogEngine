#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include "JoltConversions.hpp"
#include "JoltLayers.hpp"
#include "JoltState.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace HP
{
    namespace
    {
        // A non-uniform scale on a sphere or capsule counts as uniform within this.
        constexpr float UNIFORM_SCALE_TOLERANCE = 1e-4f;

        JPH::BodyID ToBodyID(BodyHandle handle) { return JPH::BodyID(handle.Value); }

        BodyHandle ToHandle(JPH::BodyID id) { return BodyHandle{ id.GetIndexAndSequenceNumber() }; }

        bool IsFinite(const HM::Vector3& vector)
        {
            return std::isfinite(vector.x()) && std::isfinite(vector.y()) && std::isfinite(vector.z());
        }

        bool IsPositive(float value) { return std::isfinite(value) && value > 0.0f; }

        HM::Vector3 Abs(const HM::Vector3& vector)
        {
            return HM::Vector3(std::abs(vector.x()), std::abs(vector.y()), std::abs(vector.z()));
        }

        // Why desc cannot make a body, or empty.
        std::string CheckBodyDesc(const BodyDesc& desc)
        {
            const ShapeDesc& shape = desc.Shape;
            const HM::Vector3 scale = Abs(shape.Scale);
            if (!IsPositive(scale.x()) || !IsPositive(scale.y()) || !IsPositive(scale.z()))
                return "its scale is not finite and non-zero";
            if (!IsFinite(shape.Center))
                return "its shape's centre is not finite";
            switch (shape.Type)
            {
            case ShapeType::Box:
                if (!IsPositive(shape.HalfExtents.x()) || !IsPositive(shape.HalfExtents.y()) || !IsPositive(shape.HalfExtents.z()))
                    return "a box's half extents must be positive";
                break;
            case ShapeType::Sphere:
                if (!IsPositive(shape.Radius))
                    return "a sphere's radius must be positive";
                break;
            case ShapeType::Capsule:
                if (!IsPositive(shape.Radius) || !std::isfinite(shape.HalfHeight) || shape.HalfHeight < 0.0f)
                    return "a capsule needs a positive radius and a half height of 0 or more";
                break;
            }
            if (desc.Layer >= PHYSICS_LAYER_COUNT)
                return "its layer " + std::to_string(desc.Layer) + " is not below " + std::to_string(PHYSICS_LAYER_COUNT);
            if (!IsFinite(desc.Pose.Position) || !std::isfinite(desc.Pose.Rotation.x()) ||
                !std::isfinite(desc.Pose.Rotation.y()) || !std::isfinite(desc.Pose.Rotation.z()) ||
                !std::isfinite(desc.Pose.Rotation.w()))
                return "its pose is not finite";
            if (desc.Motion == MotionType::Dynamic && !IsPositive(desc.Mass))
                return "a dynamic body's mass must be positive";
            return {};
        }

        // The shape with the body's scale baked into its size, offset by its scaled centre; a capsule
        // turned from Jolt's +Y axis to +Z.
        JPH::Ref<JPH::Shape> MakeShape(const ShapeDesc& shape)
        {
            const HM::Vector3 scale   = Abs(shape.Scale);
            const float       largest = std::max({ scale.x(), scale.y(), scale.z() });
            const float       least   = std::min({ scale.x(), scale.y(), scale.z() });
            if (shape.Type != ShapeType::Box && largest - least > UNIFORM_SCALE_TOLERANCE * largest)
                LOGWARNING("[Physics] A sphere or capsule cannot be scaled unevenly; it takes its largest scale,", largest, ".");

            JPH::Ref<JPH::Shape> base;
            JPH::Quat            rotation = JPH::Quat::sIdentity();
            switch (shape.Type)
            {
            case ShapeType::Box:
            {
                const JPH::Vec3 halfExtents(shape.HalfExtents.x() * scale.x(), shape.HalfExtents.y() * scale.y(),
                                            shape.HalfExtents.z() * scale.z());
                base = new JPH::BoxShape(halfExtents, std::min(JPH::cDefaultConvexRadius, halfExtents.ReduceMin()));
                break;
            }
            case ShapeType::Sphere:
                base = new JPH::SphereShape(shape.Radius * largest);
                break;
            case ShapeType::Capsule:
                if (shape.HalfHeight * largest <= 0.0f)
                    base = new JPH::SphereShape(shape.Radius * largest);
                else
                    base = new JPH::CapsuleShape(shape.HalfHeight * largest, shape.Radius * largest);
                rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisX(), 0.5f * JPH::JPH_PI);
                break;
            }

            const JPH::Vec3 center(shape.Center.x() * scale.x(), shape.Center.y() * scale.y(), shape.Center.z() * scale.z());
            if (center.IsNearZero() && rotation.IsClose(JPH::Quat::sIdentity()))
                return base;
            return new JPH::RotatedTranslatedShape(center, rotation, base);
        }

        JPH::EMotionType ToJolt(MotionType motion)
        {
            switch (motion)
            {
            case MotionType::Static:    return JPH::EMotionType::Static;
            case MotionType::Kinematic: return JPH::EMotionType::Kinematic;
            case MotionType::Dynamic:   return JPH::EMotionType::Dynamic;
            }
            return JPH::EMotionType::Static;
        }
    }

    BodyHandle PhysicsWorld::CreateBody(const BodyDesc& desc)
    {
        if (!m_State)
        {
            LOGERROR("[Physics] A body cannot be made: the world is not running.");
            return {};
        }
        if (const std::string problem = CheckBodyDesc(desc); !problem.empty())
        {
            LOGERROR("[Physics] A body cannot be made:", problem + ".");
            return {};
        }

        JPH::BodyCreationSettings settings(MakeShape(desc.Shape), JPH::RVec3(ToJolt(desc.Pose.Position)),
                                           ToJolt(desc.Pose.Rotation), ToJolt(desc.Motion),
                                           ToObjectLayer(desc.Layer, desc.Motion != MotionType::Static));
        settings.mIsSensor       = desc.IsSensor;
        settings.mFriction       = desc.Friction;
        settings.mRestitution    = desc.Restitution;
        settings.mLinearDamping  = desc.LinearDamping;
        settings.mAngularDamping = desc.AngularDamping;
        settings.mGravityFactor  = desc.GravityFactor;
        settings.mUserData       = desc.UserData;
        if (desc.Motion == MotionType::Dynamic)
        {
            settings.mOverrideMassProperties       = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.Mass;
        }

        const JPH::BodyID id = m_State->System->GetBodyInterface().CreateAndAddBody(
            settings, desc.Motion == MotionType::Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
        if (id.IsInvalid())
        {
            LOGERROR("[Physics] A body cannot be made: the world holds its maximum of bodies.");
            return {};
        }
        return ToHandle(id);
    }

    void PhysicsWorld::DestroyBody(BodyHandle body)
    {
        if (!IsValid(body))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.RemoveBody(ToBodyID(body));
        bodies.DestroyBody(ToBodyID(body));
    }

    void PhysicsWorld::DestroyAllBodies()
    {
        if (!m_State)
            return;
        JPH::BodyIDVector ids;
        m_State->System->GetBodies(ids);
        if (ids.empty())
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        bodies.RemoveBodies(ids.data(), static_cast<int>(ids.size()));
        bodies.DestroyBodies(ids.data(), static_cast<int>(ids.size()));
    }

    bool PhysicsWorld::IsValid(BodyHandle body) const
    {
        if (!m_State || !body.IsSet())
            return false;
        // The lock checks the sequence number: a reused index is not this body.
        const JPH::BodyLockRead lock(m_State->System->GetBodyLockInterface(), ToBodyID(body));
        return lock.Succeeded();
    }

    BodyPose PhysicsWorld::GetPose(BodyHandle body) const
    {
        if (!IsValid(body))
            return {};
        JPH::RVec3 position;
        JPH::Quat  rotation;
        m_State->System->GetBodyInterface().GetPositionAndRotation(ToBodyID(body), position, rotation);
        return { FromJolt(JPH::Vec3(position)), FromJolt(rotation) };
    }

    void PhysicsWorld::SetPose(BodyHandle body, const BodyPose& pose)
    {
        if (!IsValid(body) || !IsFinite(pose.Position))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        const bool          moving = bodies.GetMotionType(ToBodyID(body)) != JPH::EMotionType::Static;
        bodies.SetPositionAndRotation(ToBodyID(body), JPH::RVec3(ToJolt(pose.Position)), ToJolt(pose.Rotation),
                                      moving ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    }

    void PhysicsWorld::MoveKinematic(BodyHandle body, const BodyPose& pose, float dt)
    {
        if (!IsValid(body) || !IsFinite(pose.Position) || !IsPositive(dt))
            return;
        JPH::BodyInterface& bodies = m_State->System->GetBodyInterface();
        if (bodies.GetMotionType(ToBodyID(body)) != JPH::EMotionType::Kinematic)
            return;
        bodies.MoveKinematic(ToBodyID(body), JPH::RVec3(ToJolt(pose.Position)), ToJolt(pose.Rotation), dt);
    }

    MotionType PhysicsWorld::GetMotionType(BodyHandle body) const
    {
        if (!IsValid(body))
            return MotionType::Static;
        switch (m_State->System->GetBodyInterface().GetMotionType(ToBodyID(body)))
        {
        case JPH::EMotionType::Kinematic: return MotionType::Kinematic;
        case JPH::EMotionType::Dynamic:   return MotionType::Dynamic;
        default:                          return MotionType::Static;
        }
    }

    uint64_t PhysicsWorld::GetUserData(BodyHandle body) const
    {
        return IsValid(body) ? m_State->System->GetBodyInterface().GetUserData(ToBodyID(body)) : 0;
    }

    void PhysicsWorld::GetActiveBodies(std::vector<BodyHandle>& out) const
    {
        out.clear();
        if (!m_State)
            return;
        JPH::BodyIDVector ids;
        m_State->System->GetActiveBodies(JPH::EBodyType::RigidBody, ids);
        out.reserve(ids.size());
        for (const JPH::BodyID id : ids)
            out.push_back(ToHandle(id));
    }
}
