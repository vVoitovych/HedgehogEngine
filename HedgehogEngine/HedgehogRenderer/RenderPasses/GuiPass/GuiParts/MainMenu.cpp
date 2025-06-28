#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"

#include "Scene/Scene.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	void GuiPass::DrawMainMenu(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New")) { scene.ResetScene(); }
				if (ImGui::MenuItem("Rename")) { scene.RenameScene(); }
				if (ImGui::MenuItem("Open")) { scene.Load(); }
				if (ImGui::MenuItem("Save")) { scene.Save(); }

				ImGui::Separator();
				if (ImGui::MenuItem("Quit", "Alt+F4")) {}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Create"))
			{
				if (ImGui::MenuItem("Create game object")) { scene.CreateGameObject(m_SelectedObject); }

				ImGui::Separator();
				if (ImGui::BeginMenu("Add component"))
				{
					if (ImGui::MenuItem("Mesh component")) 
					{
						if (m_SelectedObject.has_value())
							scene.AddMeshComponent(m_SelectedObject.value());
					}
					if (ImGui::MenuItem("Render component")) 
					{ 
						if (m_SelectedObject.has_value())
							scene.AddRenderComponent(m_SelectedObject.value()); 
					}
					if (ImGui::MenuItem("Light component")) 
					{ 
						if (m_SelectedObject.has_value())
							scene.AddLightComponent(m_SelectedObject.value()); 
					}
					if (ImGui::MenuItem("Script component")) 
					{
						if (m_SelectedObject.has_value())
							scene.AddScriptComponent(m_SelectedObject.value());
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Create material")) { context.GetEngineContext().GetMaterialContainer().CreateNewMaterial(); }
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Settings"))
			{
				if (ImGui::MenuItem("Settings")) { ShowMaterialWindow(); }

				ImGui::EndMenu();
			}


			ImGui::EndMainMenuBar();
		}
	}
}

