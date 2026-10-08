#include "api/SceneExtractor.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
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
    void SceneExtractor::ExtractPhysicsDebug(const ECS::ECS& ecs, const HedgehogEngine::PhysicsSystem& physicsSystem,
                                             RenderScene& outScene) const
    {
        LineList& lines = outScene.DebugLines;
        for (const ECS::Entity entity : physicsSystem.GetEntities())
        {
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
    }
}
