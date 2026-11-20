#include "api/SceneExtractor.hpp"
#include "Profiler/api/Profiler.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/JointComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace
{
    // The renderer's line list, extended by the vertices this file appends.
    using LineList = std::vector<HX::DebugLineVertex>;

    // Segments per full circle; a half arc takes half of them.
    constexpr size_t CIRCLE_SEGMENTS = HX::SceneExtractor::PHYSICS_DEBUG_CIRCLE_SEGMENTS;

    // The unit circle's points, made once, so drawing a circle calls no trigonometry.
    const std::array<HM::Vector2, CIRCLE_SEGMENTS + 1>& UnitCircle()
    {
        static const std::array<HM::Vector2, CIRCLE_SEGMENTS + 1> points = []
        {
            std::array<HM::Vector2, CIRCLE_SEGMENTS + 1> result{};
            for (size_t i = 0; i <= CIRCLE_SEGMENTS; ++i)
            {
                const float angle = 2.0f * std::numbers::pi_v<float> * static_cast<float>(i) / CIRCLE_SEGMENTS;
                result[i]         = HM::Vector2(std::cos(angle), std::sin(angle));
            }
            return result;
        }();
        return points;
    }

    void AddLine(LineList& lines, const HM::Vector3& a, const HM::Vector3& b, uint32_t color)
    {
        lines.push_back({ a, color });
        lines.push_back({ b, color });
    }

    // `segments` segments of the circle about center in the plane of the unit axes u and v, starting
    // at point `first` of the unit circle (a full circle is CIRCLE_SEGMENTS from 0).
    void AddArc(LineList& lines, const HM::Vector3& center, const HM::Vector3& u, const HM::Vector3& v, float radius,
                size_t first, size_t segments, uint32_t color)
    {
        const auto& circle  = UnitCircle();
        const auto  pointAt = [&](size_t i)
        {
            const HM::Vector2& p = circle[i % CIRCLE_SEGMENTS];
            return center + u * (p.x() * radius) + v * (p.y() * radius);
        };
        for (size_t i = first; i < first + segments; ++i)
            AddLine(lines, pointAt(i), pointAt(i + 1), color);
    }

    // The collider's frame in world space, as the physics system shapes it: the entity's world axes
    // made unit length, its scale their lengths and the centre offset through the whole matrix.
    struct ColliderFrame
    {
        HM::Vector3                Center;
        std::array<HM::Vector3, 3> Axes;
        HM::Vector3                Scale;
    };

    ColliderFrame MakeFrame(const HM::Matrix4x4& world, const HM::Vector3& localCenter)
    {
        ColliderFrame frame;
        const std::array<HM::Vector3, 3> fallback = { HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f),
                                                      HM::Vector3(0.0f, 0.0f, 1.0f) };
        frame.Center = HM::Vector3(world[3].x(), world[3].y(), world[3].z());
        for (size_t i = 0; i < 3; ++i)
        {
            const HM::Vector3 axis(world[i].x(), world[i].y(), world[i].z());
            const float       length = std::sqrt(axis.LengthSqr());
            frame.Scale[i]           = length;
            frame.Axes[i]            = length > 0.0f ? axis / length : fallback[i];
            frame.Center += axis * localCenter[i];
        }
        return frame;
    }

    void AddBox(LineList& lines, const ColliderFrame& frame, const HM::Vector3& size, uint32_t color)
    {
        std::array<HM::Vector3, 3> half;
        for (size_t i = 0; i < 3; ++i)
            half[i] = frame.Axes[i] * (std::abs(size[i]) * 0.5f * frame.Scale[i]);

        // Corner k has bit i set for +half[i].
        const auto corner = [&](size_t k)
        {
            return frame.Center + ((k & 1) ? half[0] : -half[0]) + ((k & 2) ? half[1] : -half[1]) +
                   ((k & 4) ? half[2] : -half[2]);
        };
        for (size_t k = 0; k < 8; ++k)
            for (size_t bit = 1; bit < 8; bit <<= 1)
                if ((k & bit) == 0)
                    AddLine(lines, corner(k), corner(k | bit), color);
    }

    void AddSphere(LineList& lines, const ColliderFrame& frame, float radius, uint32_t color)
    {
        const auto& a = frame.Axes;
        AddArc(lines, frame.Center, a[0], a[1], radius, 0, CIRCLE_SEGMENTS, color);
        AddArc(lines, frame.Center, a[1], a[2], radius, 0, CIRCLE_SEGMENTS, color);
        AddArc(lines, frame.Center, a[2], a[0], radius, 0, CIRCLE_SEGMENTS, color);
    }

    // Along local +Z: a circle at each end of the cylinder, four lines joining them, and over each
    // end two half arcs, in the planes of X and Y with Z.
    void AddCapsule(LineList& lines, const ColliderFrame& frame, float radius, float halfHeight, uint32_t color)
    {
        const auto&       a    = frame.Axes;
        const HM::Vector3 top  = frame.Center + a[2] * halfHeight;
        const HM::Vector3 base = frame.Center - a[2] * halfHeight;
        AddArc(lines, top, a[0], a[1], radius, 0, CIRCLE_SEGMENTS, color);
        AddArc(lines, base, a[0], a[1], radius, 0, CIRCLE_SEGMENTS, color);
        for (const HM::Vector3& side : { a[0], -a[0], a[1], -a[1] })
            AddLine(lines, base + side * radius, top + side * radius, color);

        // Over the top from the angle 0 to pi, under the base from pi to 2 pi.
        constexpr size_t HALF = CIRCLE_SEGMENTS / 2;
        AddArc(lines, top, a[0], a[2], radius, 0, HALF, color);
        AddArc(lines, top, a[1], a[2], radius, 0, HALF, color);
        AddArc(lines, base, a[0], a[2], radius, HALF, HALF, color);
        AddArc(lines, base, a[1], a[2], radius, HALF, HALF, color);
    }

    uint32_t ChooseColor(const ECS::ECS& ecs, ECS::Entity entity, const HedgehogEngine::ColliderComponent& collider)
    {
        using HX::SceneExtractor;
        if (collider.IsTrigger)
            return SceneExtractor::PHYSICS_DEBUG_TRIGGER_COLOR;
        if (!ecs.HasComponent<HedgehogEngine::RigidBodyComponent>(entity))
            return SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR;
        switch (ecs.GetComponent<HedgehogEngine::RigidBodyComponent>(entity).BodyType)
        {
        case HedgehogEngine::RigidBodyType::Static:    return SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR;
        case HedgehogEngine::RigidBodyType::Kinematic: return SceneExtractor::PHYSICS_DEBUG_KINEMATIC_COLOR;
        case HedgehogEngine::RigidBodyType::Dynamic:   return SceneExtractor::PHYSICS_DEBUG_DYNAMIC_COLOR;
        }
        return SceneExtractor::PHYSICS_DEBUG_STATIC_COLOR;
    }
}

namespace HX
{
    namespace
    {
        void ExtractCharacters(const ECS::ECS& ecs, LineList& lines);
        void ExtractJoints(const ECS::ECS& ecs, LineList& lines);
    }

    void SceneExtractor::ExtractPhysicsDebug(const ECS::ECS& ecs, const HedgehogEngine::PhysicsSystem& physicsSystem,
                                             RenderScene& outScene) const
    {
        HH_PROFILE_ZONE("Extract: collider lines");
        LineList& lines = outScene.DebugLines;
        // The view exists only where the controller component is registered.
        const bool characters = ecs.HasSystem<HedgehogEngine::CharacterControllerListSystem>();
        for (const ECS::Entity entity : physicsSystem.GetEntities())
        {
            // A character's collider makes no body (its capsule is drawn below).
            if (characters && ecs.HasComponent<HedgehogEngine::CharacterControllerComponent>(entity))
                continue;
            const auto& collider  = ecs.GetComponent<HedgehogEngine::ColliderComponent>(entity);
            const auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);
            const ColliderFrame frame = MakeFrame(transform.ObjMatrix, collider.Center);
            const uint32_t      color = ChooseColor(ecs, entity, collider);
            // A sphere or capsule takes its largest scale, as its body does.
            const float largest = std::max({ frame.Scale.x(), frame.Scale.y(), frame.Scale.z() });

            switch (collider.Shape)
            {
            case HedgehogEngine::ColliderShape::Box:
                AddBox(lines, frame, collider.Size, color);
                break;
            case HedgehogEngine::ColliderShape::Sphere:
                AddSphere(lines, frame, collider.Radius * largest, color);
                break;
            case HedgehogEngine::ColliderShape::Capsule:
                AddCapsule(lines, frame, collider.Radius * largest,
                           std::max(0.0f, collider.Height * 0.5f - collider.Radius) * largest, color);
                break;
            }
        }

        // Characters stand upright, unscaled, at their entity's world position, as the physics
        // system makes them.
        if (characters)
            ExtractCharacters(ecs, lines);
        if (ecs.HasSystem<HedgehogEngine::JointListSystem>())
            ExtractJoints(ecs, lines);
    }

    namespace
    {
        void ExtractCharacters(const ECS::ECS& ecs, LineList& lines)
        {
            for (const ECS::Entity entity : ecs.GetSystem<HedgehogEngine::CharacterControllerListSystem>()->GetEntities())
            {
                const auto&   controller = ecs.GetComponent<HedgehogEngine::CharacterControllerComponent>(entity);
                const auto&   world      = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity).ObjMatrix;
                ColliderFrame frame;
                frame.Center = HM::Vector3(world[3].x(), world[3].y(), world[3].z()) + controller.Center;
                frame.Axes   = { HM::Vector3(1.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 1.0f, 0.0f),
                                 HM::Vector3(0.0f, 0.0f, 1.0f) };
                frame.Scale  = HM::Vector3(1.0f, 1.0f, 1.0f);
                AddCapsule(lines, frame, controller.Radius, std::max(0.0f, controller.Height * 0.5f - controller.Radius),
                           SceneExtractor::PHYSICS_DEBUG_CHARACTER_COLOR);
            }
        }

        void ExtractJoints(const ECS::ECS& ecs, LineList& lines)
        {
            const auto origin = [](const HM::Matrix4x4& world)
            {
                return HM::Vector3(world[3].x(), world[3].y(), world[3].z());
            };
            for (const ECS::Entity entity : ecs.GetSystem<HedgehogEngine::JointListSystem>()->GetEntities())
            {
                const auto& joint = ecs.GetComponent<HedgehogEngine::JointComponent>(entity);
                if (!joint.Enabled)
                    continue;
                const HM::Matrix4x4& world = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity).ObjMatrix;
                const auto column = [&world](int index)
                {
                    return HM::Vector3(world[index].x(), world[index].y(), world[index].z());
                };
                const HM::Vector3 anchor = origin(world) + column(0) * joint.Anchor.x() + column(1) * joint.Anchor.y() +
                                           column(2) * joint.Anchor.z();

                // A cross at the anchor.
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float       size = SceneExtractor::PHYSICS_DEBUG_JOINT_CROSS;
                    const HM::Vector3 arm(axis == 0 ? size : 0.0f, axis == 1 ? size : 0.0f, axis == 2 ? size : 0.0f);
                    AddLine(lines, anchor - arm, anchor + arm, SceneExtractor::PHYSICS_DEBUG_JOINT_COLOR);
                }

                // A line to what it joins: the connected entity's origin, or a distance joint's world point.
                const ECS::Entity connected = joint.ConnectedEntity;
                if (connected != ECS::INVALID_ENTITY && connected != entity && ecs.IsAlive(connected) &&
                    ecs.HasComponent<HedgehogEngine::TransformComponent>(connected))
                    AddLine(lines, anchor, origin(ecs.GetComponent<HedgehogEngine::TransformComponent>(connected).ObjMatrix),
                            SceneExtractor::PHYSICS_DEBUG_JOINT_COLOR);
                else if (connected == ECS::INVALID_ENTITY && joint.Type == HedgehogEngine::JointType::Distance)
                    AddLine(lines, anchor, joint.ConnectedAnchor, SceneExtractor::PHYSICS_DEBUG_JOINT_COLOR);

                // A hinge's axis or a slider's direction.
                if (joint.Type == HedgehogEngine::JointType::Hinge || joint.Type == HedgehogEngine::JointType::Slider)
                {
                    const HM::Vector3 axis = column(0) * joint.Axis.x() + column(1) * joint.Axis.y() + column(2) * joint.Axis.z();
                    const float       length = axis.LengthSlow();
                    if (length > 0.0f)
                        AddLine(lines, anchor, anchor + axis * (SceneExtractor::PHYSICS_DEBUG_JOINT_AXIS / length), SceneExtractor::PHYSICS_DEBUG_JOINT_COLOR);
                }
            }
        }
    }
}
