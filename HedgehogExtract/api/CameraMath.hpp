#pragma once

#include "RenderScene.hpp"

#include "HedgehogMath/api/Common.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

// A RenderCamera's matrices, shared by everything that has to agree on them: the renderer draws a
// view with them, and editor picking casts its ray through the same projection. Header-only, so
// code that compiles only a few of the renderer's sources needs no extra library.
namespace HX
{
    struct Ray
    {
        HM::Vector3 Origin    = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Direction = HM::Vector3(0.0f, 0.0f, -1.0f); // unit length
    };

    [[nodiscard]] inline HM::Matrix4x4 MakeViewMatrix(const RenderCamera& camera)
    {
        return camera.WorldMatrix.Inverse();
    }

    // Vertical field of view in degrees, a Vulkan-style (Y down) clip space.
    [[nodiscard]] inline HM::Matrix4x4 MakeProjection(const RenderCamera& camera, float aspect)
    {
        if (camera.ProjectionType == CameraProjectionType::Orthographic)
        {
            const float halfHeight = camera.OrthoSize * 0.5f;
            const float halfWidth  = halfHeight * aspect;
            return HM::Matrix4x4::Ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, camera.NearPlane,
                                        camera.FarPlane);
        }
        HM::Matrix4x4 proj = HM::Matrix4x4::Perspective(HM::ToRadians(camera.Fov), aspect, camera.NearPlane,
                                                        camera.FarPlane);
        proj[1][1] *= -1.0f;
        return proj;
    }

    // The world-space ray through a point of the camera's image: (u, v) in [0, 1], (0, 0) the top
    // left corner. It starts at the eye for a perspective camera and on the near plane for an
    // orthographic one, so everything the camera can see lies at t >= 0.
    [[nodiscard]] inline Ray MakePickRay(const RenderCamera& camera, float aspect, float u, float v)
    {
        const HM::Matrix4x4 inverseViewProj = (MakeProjection(camera, aspect) * MakeViewMatrix(camera)).Inverse();
        const auto unproject = [&](float depth)
        {
            const HM::Vector4 point = inverseViewProj * HM::Vector4(2.0f * u - 1.0f, 2.0f * v - 1.0f, depth, 1.0f);
            return HM::Vector3(point.x() / point.w(), point.y() / point.w(), point.z() / point.w());
        };

        // Perspective depth runs 0..1 and orthographic -1..1 (HM::Matrix4x4): either way these two
        // points lie on the ray, in front of the camera.
        const HM::Vector3 nearPoint = unproject(0.0f);
        const HM::Vector3 farPoint  = unproject(1.0f);

        Ray ray;
        ray.Direction = (farPoint - nearPoint).Normalize();
        if (camera.ProjectionType == CameraProjectionType::Orthographic)
        {
            ray.Origin = unproject(-1.0f);
        }
        else
        {
            const HM::Vector4& eye = camera.WorldMatrix[3]; // the translation column
            ray.Origin = HM::Vector3(eye.x(), eye.y(), eye.z());
        }
        return ray;
    }
}
