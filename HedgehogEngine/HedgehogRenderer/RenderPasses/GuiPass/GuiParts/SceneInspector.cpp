#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
    void GuiPass::DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index)
    {
        auto& scene     = context.GetEngineContext().GetScene();
        auto& component = scene.GetHierarchyComponent(entity);

        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
        bool isSelected = m_SelectedObject.has_value() && entity == m_SelectedObject.value();
        if (isSelected)
            nodeFlags |= ImGuiTreeNodeFlags_Selected;

        auto toggleSelection = [&]()
        {
            if (m_SelectedObject.has_value() && m_SelectedObject.value() == entity)
                m_SelectedObject.reset();
            else
                m_SelectedObject = entity;
        };

        if (!component.m_Children.empty())
        {
            bool node_open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());
            if (ImGui::IsItemClicked())
                toggleSelection();
            ++index;
            if (node_open)
            {
                for (auto child : component.m_Children)
                {
                    DrawHierarchyNode(context, child, index);
                }
                ImGui::TreePop();
            }
        }
        else
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(index)),
                nodeFlags, "%s", component.m_Name.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                toggleSelection();
            ++index;
        }
    }

    void GuiPass::DrawSceneInspector(Context::Context& context)
    {
        const float sizeX = 300.0f;
        const float sizeY = static_cast<float>(ImGui::GetIO().DisplaySize.y);

        ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(sizeX, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

        auto& scene = context.GetEngineContext().GetScene();
        ImGui::Begin(scene.GetSceneName().c_str(), nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        {
            int index = 0;
            ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);

            if (ImGui::Button("Create object", sz))
            {
                scene.CreateGameObject(m_SelectedObject);
            }
            if (ImGui::Button("Delete object", sz))
            {
                if (m_SelectedObject.has_value() && m_SelectedObject.value() != scene.GetRoot())
                {
                    scene.DeleteGameObject(m_SelectedObject.value());
                    m_SelectedObject.reset();
                }
            }

            DrawHierarchyNode(context, scene.GetRoot(), index);
        }

        ImGui::End();
    }
}
