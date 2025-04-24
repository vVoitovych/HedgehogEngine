#include "MaterialEditor.hpp"

#include "HedgehogContext/Context/Context.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	bool MaterialEditor::s_MaterialWindowShow = false;

	void MaterialEditor::Draw(Context::Context& context)
	{
		if (s_MaterialWindowShow)
		{
			float sizeX = 300.0f;
			float sizeY = 300.0f;
			ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(450.0f, 150.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));

			ImGui::Begin("Material Editor", &s_MaterialWindowShow, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New")) {  }
					if (ImGui::MenuItem("Open")) {  }
					if (ImGui::MenuItem("Save")) {  }

					ImGui::Separator();
					if (ImGui::MenuItem("Quit")) { s_MaterialWindowShow = false; }
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			ImGui::End();
		}
	}

	void MaterialEditor::ShowMaterialWindow()
	{
		s_MaterialWindowShow = true;
	}

}



