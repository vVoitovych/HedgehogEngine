#include "TransformGizmo.hpp"

#include "imgui.h"
// ImGuizmo.h does not include ImGui itself.
#include "ImGuizmo.h"

namespace Editor
{
    namespace
    {
        static_assert(sizeof(HM::Matrix4x4) == 16 * sizeof(float),
                      "ImGuizmo reads a matrix as 16 floats, column after column, as HM::Matrix4x4 stores them.");

        ImGuizmo::OPERATION ToOperation(TransformTool tool)
        {
            switch (tool)
            {
            case TransformTool::Move:   return ImGuizmo::TRANSLATE;
            case TransformTool::Rotate: return ImGuizmo::ROTATE;
            case TransformTool::Scale:  return ImGuizmo::SCALE;
            }
            return ImGuizmo::TRANSLATE;
        }
    }

    void BeginTransformGizmoFrame()
    {
        ImGuizmo::BeginFrame();
    }

    TransformGizmoResult DrawTransformGizmo(const TransformGizmoCamera& camera, const ImVec2& imageMin,
                                            const ImVec2& imageSize, TransformTool tool, TransformSpace space,
                                            const HM::Matrix4x4& world)
    {
        TransformGizmoResult result;
        if (imageSize.x <= 0.0f || imageSize.y <= 0.0f)
            return result;

        // A right-handed perspective with clip y up, as ImGuizmo expects: the image's projection
        // (HX::MakeProjection) without its Vulkan y flip, so the handles sit on the drawn object.
        const HM::Matrix4x4 projection =
            HM::Matrix4x4::Perspective(camera.FovRadians, imageSize.x / imageSize.y, camera.NearPlane, camera.FarPlane);

        ImDrawList& drawList = *ImGui::GetWindowDrawList();
        drawList.PushClipRect(imageMin, ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y), true);
        ImGuizmo::SetDrawlist(&drawList);
        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetRect(imageMin.x, imageMin.y, imageSize.x, imageSize.y);

        const ImGuizmo::OPERATION operation = ToOperation(tool);
        HM::Matrix4x4             matrix    = world;
        const bool changed = ImGuizmo::Manipulate(camera.View.GetBuffer(), projection.GetBuffer(), operation,
                                                  space == TransformSpace::Local ? ImGuizmo::LOCAL : ImGuizmo::WORLD,
                                                  matrix.GetBuffer());
        drawList.PopClipRect();

        if (changed)
            result.World = matrix;
        result.Active = ImGuizmo::IsOver(operation); // a drag included
        return result;
    }
}
