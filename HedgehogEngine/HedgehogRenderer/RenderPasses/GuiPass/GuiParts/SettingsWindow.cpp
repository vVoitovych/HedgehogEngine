#include "SettingsWindow.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"
#include "HedgehogSettings/Settings/ShadowmapingSettings.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

namespace Renderer
{
	bool SettingsWindow::s_MaterialWindowShow = false;

	void SettingsWindow::Draw(Context::Context& context)
	{
		if (s_MaterialWindowShow)
		{
			float sizeX = 600.0f;
			float sizeY = 400.0f;
			ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Appearing);
			ImGui::SetNextWindowPos(ImVec2(750.0f, 150.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));

			ImGui::Begin("Material Editor", &s_MaterialWindowShow, ImGuiWindowFlags_MenuBar);
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("Main"))
				{

					if (ImGui::MenuItem("Quit")) { s_MaterialWindowShow = false; }
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}

			auto& settings = context.GetEngineContext().GetSettings();
			
			if (ImGui::CollapsingHeader("Shadow settings", 32))
			{
				auto& shadowmapSettings = settings.GetShadowmapSettings();

				static float split1 = shadowmapSettings->GetSplit1();
				static float split2 = shadowmapSettings->GetSplit2();
				static float split3 = shadowmapSettings->GetSplit3();

				static int shadowmapSize = shadowmapSettings->GetShadowmapSize();
				if (ImGui::InputInt("Shadowmap size", &shadowmapSize))
				{
					shadowmapSettings->SetShadowmapSize(shadowmapSize);
					auto originalSize = shadowmapSettings->GetShadowmapSize();
					if (originalSize != shadowmapSize)
						shadowmapSize = originalSize;
				}

				static int cascatesCount = shadowmapSettings->GetCascadesCount();
				if (ImGui::SliderInt("Cascades count", &cascatesCount, 1, 4))
				{
					shadowmapSettings->SetCascadesCount(cascatesCount);
					shadowmapSettings->SetDefaultSplits();

					split1 = shadowmapSettings->GetSplit1();
					split2 = shadowmapSettings->GetSplit2();
					split3 = shadowmapSettings->GetSplit3();
				}

				if (cascatesCount > 1)
				{
					ImGui::SliderFloat("Split 1", &split1, 0.0f, split2);
					shadowmapSettings->SetSplit1(split1);
				}

				if (cascatesCount > 2)
				{
					ImGui::SliderFloat("Split 2", &split2, split1, split3);
					shadowmapSettings->SetSplit2(split2);
				}

				if (cascatesCount > 3)
				{
					ImGui::SliderFloat("Split 3", &split3, split2, 100.0f);
					shadowmapSettings->SetSplit3(split3);
				}
			}


			ImGui::End();
		}
	}

	void SettingsWindow::ShowMaterialWindow()
	{
		s_MaterialWindowShow = true;
	}

}