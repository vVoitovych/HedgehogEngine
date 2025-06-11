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
		auto& scene = context.GetEngineContext().GetScene();
		auto& component = scene.GetHierarchyComponent(entity);
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen;
		bool isSelected = false;
		if (m_SelectedObject.has_value() && entity == m_SelectedObject.value())
		{
			isSelected = true;
		}
		if (isSelected)
			nodeFlags |= ImGuiTreeNodeFlags_Selected;

		if (component.mChildren.size() > 0)
		{
			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked())
			{
				if (m_SelectedObject.has_value())
				{
					if (m_SelectedObject.value() == entity)
						m_SelectedObject.reset();
					else
						m_SelectedObject = entity;
				}
				else
				{
					m_SelectedObject = entity;
				}
			}
			++index;
			if (node_open)
			{
				for (auto entity : component.mChildren)
				{
					DrawHierarchyNode(context, entity, index);
				}
				ImGui::TreePop();
			}
		}
		else
		{
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				if (m_SelectedObject.has_value())
				{
					if (m_SelectedObject.value() == entity)
						m_SelectedObject.reset();
					else
						m_SelectedObject = entity;
				}
				else
				{
					m_SelectedObject = entity;
				}
			}
			++index;
		}
	}

	void GuiPass::DrawSceneInspector(Context::Context& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(sizeX, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		auto& scene = context.GetEngineContext().GetScene();
		ImGui::Begin(scene.GetSceneName().c_str(), NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

		{
			int index = 0;

			ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);

			if (ImGui::Button("Create object", sz))
			{
				scene.CreateGameObject(m_SelectedObject);
			}
			if (ImGui::Button("Delete object", sz))
			{
				if (m_SelectedObject.has_value())
				{
					scene.DeleteGameObject(m_SelectedObject.value());
				}
			}

			DrawHierarchyNode(context, scene.GetRoot(), index);

		}
		ImGui::End();
	}

}