#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"
#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/MeshContainer/Mesh.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <stdexcept>

namespace Renderer
{


	void GuiPass::DrawTitle(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();
		if (m_SelectedObject.has_value())
		{
			auto entity = m_SelectedObject.value();
			auto& hierarchy = scene.GetHierarchyComponent(entity);
			auto name = hierarchy.mName;
			if (ImGui::CollapsingHeader("Name", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::InputText("input text", &name[0], name.capacity() + 1))
				{
					hierarchy.mName = name;
				}
			}
		}
	}

	void GuiPass::DrawTransform(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();

		if (m_SelectedObject.has_value())
		{
			auto entity = m_SelectedObject.value();

			if (ImGui::CollapsingHeader("Transform Component", ImGuiTreeNodeFlags_DefaultOpen))
			{

				auto& transform = scene.GetTransformComponent(entity);
				ImGui::SeparatorText("Position");
				ImGui::DragFloat("pos x", &transform.mPososition.x(), 0.5f);
				ImGui::DragFloat("pos y", &transform.mPososition.y(), 0.5f);
				ImGui::DragFloat("pos z", &transform.mPososition.z(), 0.5f);

				ImGui::SeparatorText("Rotation");
				ImGui::DragFloat("rot x", &transform.mRotation.x(), 0.5f);
				ImGui::DragFloat("rot y", &transform.mRotation.y(), 0.5f);
				ImGui::DragFloat("rot z", &transform.mRotation.z(), 0.5f);

				ImGui::SeparatorText("Scale");
				ImGui::DragFloat("scale x", &transform.mScale.x(), 0.5f);
				ImGui::DragFloat("scale y", &transform.mScale.y(), 0.5f);
				ImGui::DragFloat("scale z", &transform.mScale.z(), 0.5f);
			}
		}
	}

	void GuiPass::DrawMesh(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();

		if (m_SelectedObject.has_value())
		{
			auto entity = m_SelectedObject.value();
			if (scene.HasMeshComponent(entity))
			{
				if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& mesh = scene.GetMeshComponent(entity);
					auto& meshes = scene.GetMeshes();
					uint64_t selectedIndex = mesh.mMeshIndex.value();

					if (ImGui::BeginCombo("mesh", mesh.mMeshPath.c_str()))
					{
						for (uint64_t i = 0; i < meshes.size(); ++i)
						{
							const bool isSelected = (selectedIndex == i);
							if (ImGui::Selectable(meshes[i].c_str(), isSelected))
							{
								selectedIndex = i;
								scene.ChangeMeshComponent(entity, meshes[i]);
							}

							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					if (ImGui::Button("Load mesh"))
					{
						scene.LoadMesh(entity);
					}

					if (ImGui::Button("Remove component"))
					{
						scene.RemoveMeshComponent();
					}

				}
			}
		}
	}

	void GuiPass::DrawRender(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();

		if (m_SelectedObject.has_value())
		{
			auto entity = m_SelectedObject.value();

			if (scene.HasRenderComponent(entity))
			{
				if (ImGui::CollapsingHeader("Rendering  Component", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& render = scene.GetRenderComponent(entity);

					bool enabled = render.mIsVisible;
					if (ImGui::Checkbox("Visible", &enabled))
					{
						render.mIsVisible = enabled;
					}

					auto& materials = scene.GetMaterials();

					if (!materials.empty())
					{
						if (ImGui::BeginCombo("material", render.mMaterial.c_str()))
						{
							uint64_t selectedIndex = render.mMaterialIndex.has_value() ? render.mMaterialIndex.value() : 0;

							for (uint64_t i = 0; i < materials.size(); ++i)
							{
								const bool isSelected = (selectedIndex == i);
								if (ImGui::Selectable(materials[i].c_str(), isSelected))
								{
									selectedIndex = i;
									render.mMaterial = materials[i];
									scene.UpdateMaterialComponent(entity);
								}

								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
					}
					if (ImGui::Button("Load material"))
					{
						scene.LoadMaterial(entity);
					}
					if (!materials.empty())
					{
						if (ImGui::Button("Save material"))
						{
							context.GetEngineContext().GetMaterialContainer().SaveMaterial(render.mMaterialIndex.value());
						}
					}
					if (ImGui::Button("Remove component"))
					{
						scene.RemoveRenderComponent();
					}
					if (!materials.empty() && render.mMaterialIndex.has_value())
					{
						ImGui::SeparatorText("Material");
						auto& materialContainer = context.GetEngineContext().GetMaterialContainer();
						auto& textuteContainer = context.GetEngineContext().GetTextureContainer();

						auto& materialData = materialContainer.GetMaterialDataByIndex(render.mMaterialIndex.value());

						const char* types[] = { "Opaque", "Cutoff", "Transparent" };
						int materialType = static_cast<int>(materialData.type);
						ImGui::Combo("Type", &materialType, types, IM_ARRAYSIZE(types));
						materialData.type = static_cast<Context::MaterialType>(materialType);

						auto& textures = textuteContainer.GetTexturePathes();
						int selectedIndex = static_cast<int>(textuteContainer.GetTextureIndex(materialData.baseColor));

						if (ImGui::BeginCombo("baseColor", materialData.baseColor.c_str()))
						{
							for (int i = 0; i < textures.size(); ++i)
							{
								const bool isSelected = (selectedIndex == i);
								if (ImGui::Selectable(textures[i].c_str(), isSelected))
								{
									selectedIndex = i;
									materialData.baseColor = textures[i];
									materialContainer.SetMaterialDirty(render.mMaterialIndex.value());
								}

								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
						if (ImGui::Button("Load texture"))
						{
							materialContainer.LoadBaseTexture(render.mMaterialIndex.value(), context.GetVulkanContext(), textuteContainer);
						}


						if (materialData.type == Context::MaterialType::Transparent)
						{
							float materialTransparency = materialData.transparency;
							ImGui::SliderFloat("slider float", &materialTransparency, 0.0f, 1.0f, "ratio = %.3f");
							if (materialData.transparency != materialTransparency)
							{
								materialData.transparency = materialTransparency;
								materialContainer.SetMaterialDirty(render.mMaterialIndex.value());
							}
						}
					}
				}
			}
		}
	}

	Scene::LightType LightIndexToLightType(int index)
	{
		if (index == 0) return Scene::LightType::DirectionLight;
		if (index == 1) return Scene::LightType::PointLight;
		if (index == 2) return Scene::LightType::SpotLight;
		throw std::runtime_error("Wrong light index");
	}

	int LightTypeToLightIndex(Scene::LightType type)
	{
		switch (type)
		{
		case Scene::LightType::DirectionLight: return 0;
			break;
		case Scene::LightType::PointLight: return 1;
			break;
		case Scene::LightType::SpotLight: return 2;
			break;
		default: throw std::runtime_error("Wrong light type");
			break;
		}
	}

	void GuiPass::DrawLight(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();

		if (m_SelectedObject.has_value())
		{
			auto entity = m_SelectedObject.value();

			if (scene.HasLightComponent(entity))
			{
				if (ImGui::CollapsingHeader("Light Component", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& light = scene.GetLightComponent(entity);

					bool enabled = light.m_Enable;
					if (ImGui::Checkbox("Enabled", &enabled))
					{
						light.m_Enable = enabled;
					}

					const char* types[] = { "Direction Light", "Point Light", "Spot Light" };
					int lightType = LightTypeToLightIndex(light.m_LightType);
					if (ImGui::Combo("Type", &lightType, types, IM_ARRAYSIZE(types)))
					{
						light.m_LightType = LightIndexToLightType(lightType);
					}

					float color[3] = { light.m_Color.r(), light.m_Color.g(), light.m_Color.b() };
					if (ImGui::ColorEdit3("Color", color))
					{
						light.m_Color = { color[0], color[1], color[2] };
					}

					float intencity = light.m_Intencity;
					if (ImGui::SliderFloat("Intencity", &intencity, 0.0f, 3.0f, "ratio = %.03f"))
					{
						light.m_Intencity = intencity;
					}

					if (lightType > 0)
					{
						float radius = light.m_Radius;
						if (ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f, "ratio = %.3f"))
						{
							light.m_Radius = radius;
						}

						if (lightType > 1)
						{
							float coneAngle = light.m_ConeAngle;
							if (ImGui::SliderFloat("Cone angle", &coneAngle, 0.1f, 179.9f, "ratio = %.3f"))
							{
								light.m_ConeAngle = coneAngle;
							}
						}
					}

					if (lightType == 0)
					{
						bool castShadows = light.m_CastShadows;
						if (ImGui::Checkbox("Cast shadows", &castShadows))
						{
							scene.UpdateShadowCastin(entity, castShadows);
						}
					}

					if (ImGui::Button("Remove component"))
					{
						scene.RemoveLightComponent();
					}
				}
			}
		}
	}

	void GuiPass::DrawInspector(Context::Context& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		ImGui::Begin(
			"Inspector",
			NULL,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
		);
		DrawTitle(context);
		DrawTransform(context);
		DrawLight(context);
		DrawMesh(context);
		DrawRender(context);

		ImGui::End();
	}
}