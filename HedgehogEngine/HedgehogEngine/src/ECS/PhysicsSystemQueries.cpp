#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"

#include "HedgehogPhysics/api/PhysicsWorld.hpp"

#include <algorithm>
#include <cmath>

namespace HedgehogEngine
{
    namespace
    {
        HP::ShapeDesc MakeSphere(float radius)
        {
            HP::ShapeDesc shape;
            shape.Type   = HP::ShapeType::Sphere;
            shape.Radius = radius;
            return shape;
        }

        HP::ShapeDesc MakeBox(const HM::Vector3& halfExtents)
        {
            HP::ShapeDesc shape;
            shape.Type        = HP::ShapeType::Box;
            shape.HalfExtents = halfExtents;
            return shape;
        }

        // A collider's capsule: its height end to end, the cylinder between its caps never negative.
        HP::ShapeDesc MakeCapsule(float radius, float height)
        {
            HP::ShapeDesc shape;
            shape.Type       = HP::ShapeType::Capsule;
            shape.Radius     = radius;
            shape.HalfHeight = std::max(0.0f, height * 0.5f - radius);
            if (!std::isfinite(height))
                shape.HalfHeight = height;
            return shape;
        }

        HP::BodyPose MakePose(const HM::Vector3& center, const HM::Quaternion& rotation = HM::Quaternion())
        {
            HP::BodyPose pose;
            pose.Position = center;
            pose.Rotation = rotation;
            return pose;
        }

        std::optional<PhysicsRayHit> Cast(const HP::PhysicsWorld* world, const HP::ShapeDesc& shape, const HP::BodyPose& pose,
                                          const HM::Vector3& direction, float maxDistance, uint16_t layerMask)
        {
            if (!world || !world->IsInitialized())
                return std::nullopt;
            const std::optional<HP::ShapeHit> hit = world->CastShape(shape, pose, direction, maxDistance, layerMask);
            if (!hit)
                return std::nullopt;
            return PhysicsRayHit{ static_cast<ECS::Entity>(hit->UserData), hit->Point, hit->Normal, hit->Distance };
        }

        void Overlap(const HP::PhysicsWorld* world, const HP::ShapeDesc& shape, const HP::BodyPose& pose, uint16_t layerMask,
                     bool includeTriggers, std::vector<ECS::Entity>& out)
        {
            out.clear();
            if (!world || !world->IsInitialized())
                return;
            std::vector<HP::OverlapHit> hits;
            world->Overlap(shape, pose, hits, layerMask, includeTriggers);
            // Sorted by user data, the entity; an entity's one body appears once.
            for (const HP::OverlapHit& hit : hits)
            {
                const auto entity = static_cast<ECS::Entity>(hit.UserData);
                if (out.empty() || out.back() != entity)
                    out.push_back(entity);
            }
        }
    }

    std::optional<PhysicsRayHit> PhysicsSystem::SphereCast(const HM::Vector3& center, float radius,
                                                           const HM::Vector3& direction, float maxDistance,
                                                           uint16_t layerMask) const
    {
        return Cast(m_World, MakeSphere(radius), MakePose(center), direction, maxDistance, layerMask);
    }

    std::optional<PhysicsRayHit> PhysicsSystem::BoxCast(const HM::Vector3& center, const HM::Vector3& halfExtents,
                                                        const HM::Quaternion& rotation, const HM::Vector3& direction,
                                                        float maxDistance, uint16_t layerMask) const
    {
        return Cast(m_World, MakeBox(halfExtents), MakePose(center, rotation), direction, maxDistance, layerMask);
    }

    std::optional<PhysicsRayHit> PhysicsSystem::CapsuleCast(const HM::Vector3& center, float radius, float height,
                                                            const HM::Quaternion& rotation, const HM::Vector3& direction,
                                                            float maxDistance, uint16_t layerMask) const
    {
        return Cast(m_World, MakeCapsule(radius, height), MakePose(center, rotation), direction, maxDistance, layerMask);
    }

    void PhysicsSystem::OverlapSphere(const HM::Vector3& center, float radius, uint16_t layerMask, bool includeTriggers,
                                      std::vector<ECS::Entity>& out) const
    {
        Overlap(m_World, MakeSphere(radius), MakePose(center), layerMask, includeTriggers, out);
    }

    void PhysicsSystem::OverlapBox(const HM::Vector3& center, const HM::Vector3& halfExtents,
                                   const HM::Quaternion& rotation, uint16_t layerMask, bool includeTriggers,
                                   std::vector<ECS::Entity>& out) const
    {
        Overlap(m_World, MakeBox(halfExtents), MakePose(center, rotation), layerMask, includeTriggers, out);
    }

    void PhysicsSystem::OverlapCapsule(const HM::Vector3& center, float radius, float height,
                                       const HM::Quaternion& rotation, uint16_t layerMask, bool includeTriggers,
                                       std::vector<ECS::Entity>& out) const
    {
        Overlap(m_World, MakeCapsule(radius, height), MakePose(center, rotation), layerMask, includeTriggers, out);
    }
}
