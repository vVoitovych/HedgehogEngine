#include "EditorGui.hpp"
#include "EditorTheme.hpp"
#include "Widgets/IconWidgets.hpp"
#include "Widgets/TransformGizmo.hpp"

#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "imgui.h"

namespace Editor
{
    void EditorGui::DrawTransformToolButtons()
    {
        const ImVec4 iconTint   = ImGui::GetStyle().Colors[ImGuiCol_Text];
        const ImVec4 activeFill = Theme::Resolve(Theme::PLAY_TINT);

        // The tool in use is filled, as the play state's button is.
        const auto toolButton = [&](const char* id, EditorIcon icon, TransformTool tool, const char* tooltip)
        {
            const bool active = m_Settings.GizmoTool == tool;
            if (active)
                ImGui::PushStyleColor(ImGuiCol_Button, activeFill);
            if (IconButton(id, GetIcon(icon), ICON_SIZE_SMALL, iconTint))
                m_Settings.GizmoTool = tool;
            if (active)
                ImGui::PopStyleColor();
            ImGui::SetItemTooltip("%s", tooltip);
        };

        toolButton("##ToolMove", EditorIcon::ToolMove, TransformTool::Move, "Move (1)");
        ImGui::SameLine();
        toolButton("##ToolRotate", EditorIcon::ToolRotate, TransformTool::Rotate, "Rotate (2)");
        ImGui::SameLine();
        toolButton("##ToolScale", EditorIcon::ToolScale, TransformTool::Scale, "Scale (3)");

        // The axes the handles follow; scaling always follows the object's own.
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x * 3.0f);
        const bool local = m_Settings.GizmoSpace == TransformSpace::Local;
        if (IconButton("##ToolSpace", GetIcon(local ? EditorIcon::SpaceLocal : EditorIcon::SpaceWorld), ICON_SIZE_SMALL,
                       iconTint))
            m_Settings.GizmoSpace = local ? TransformSpace::World : TransformSpace::Local;
        ImGui::SetItemTooltip("%s", local ? "Local axes (click for world axes)" : "World axes (click for local axes)");
    }

    void EditorGui::HandleTransformToolKeys()
    {
        // Not while a field takes text, nor in the middle of a handle's drag.
        if (!m_SceneViewFocused || ImGui::GetIO().WantTextInput || ImGui::IsMouseDown(ImGuiMouseButton_Left))
            return;
        if (ImGui::IsKeyPressed(ImGuiKey_1, false))
            m_Settings.GizmoTool = TransformTool::Move;
        else if (ImGui::IsKeyPressed(ImGuiKey_2, false))
            m_Settings.GizmoTool = TransformTool::Rotate;
        else if (ImGui::IsKeyPressed(ImGuiKey_3, false))
            m_Settings.GizmoTool = TransformTool::Scale;
    }

    bool EditorGui::DrawSceneGizmo(HedgehogEngine::Engine& context, const HM::Vector2& imageMin, const HM::Vector2& imageSize)
    {
        if (!m_SelectedEntity)
            return false;
        auto&             engineContext = context.GetEngineContext();
        auto&             ecs           = engineContext.GetECS();
        const ECS::Entity entity        = *m_SelectedEntity;
        // The scene root has no place of its own to move.
        if (!ecs.IsAlive(entity) || entity == ecs.GetRoot() || !ecs.HasComponent<HedgehogEngine::TransformComponent>(entity))
            return false;
        auto& transform = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);

        HM::Matrix4x4 parentWorld = HM::Matrix4x4::GetIdentity();
        if (ecs.HasComponent<ECS::HierarchyComponent>(entity))
        {
            const ECS::Entity parent = ecs.GetComponent<ECS::HierarchyComponent>(entity).Parent;
            if (ecs.IsAlive(parent) && ecs.HasComponent<HedgehogEngine::TransformComponent>(parent))
                parentWorld = ecs.GetComponent<HedgehogEngine::TransformComponent>(parent).ObjMatrix;
        }

        const HedgehogEngine::Camera& camera = engineContext.GetCamera();
        TransformGizmoCamera          gizmoCamera;
        gizmoCamera.View       = camera.GetViewMatrix();
        gizmoCamera.FovRadians = camera.GetFov();
        gizmoCamera.NearPlane  = camera.GetNearPlane();
        gizmoCamera.FarPlane   = camera.GetFarPlane();

        const TransformGizmoResult result =
            DrawTransformGizmo(gizmoCamera, ImVec2(imageMin.x(), imageMin.y()), ImVec2(imageSize.x(), imageSize.y()),
                               m_Settings.GizmoTool, m_Settings.GizmoSpace, transform.ObjMatrix);
        if (!result.World)
            return result.Active;

        const std::optional<LocalTransform> dragged = DecomposeWorldMatrix(*result.World, parentWorld, transform.Rotation);
        if (!dragged)
            return result.Active;
        LocalTransform values{ transform.Position, transform.Rotation, transform.Scale };
        ApplyTransformTool(m_Settings.GizmoTool, *dragged, values);
        transform.Position = values.Position;
        transform.Rotation = values.Rotation;
        transform.Scale    = values.Scale;
        // As an inspector edit: the Transform phase applies it next frame, and an instance's
        // overrides are worked out again.
        engineContext.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
        m_PrefabOverrides.Stale = true;
        return result.Active;
    }
}
