#include "GuiPass.hpp"
#include "GuiPassInfo.hpp"

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

#include "HedgehogWrappers/Wrappeers/RenderPass/RenderPass.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogCommon/Camera/Camera.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"

#include "Logger/Logger.hpp"

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"

#include <vector>
#include <algorithm>

namespace Renderer
{
	GuiPass::GuiPass(const Context::Context& context, const ResourceManager& resourceManager)
	{
		WinManager::WindowManager::SetOnGuiCallback([]() {
			return GuiPass::IsCursorPositionInGUI();
			});
		auto& vulkanContext = context.GetVulkanContext();
		auto& device = vulkanContext.GetDevice();
		std::vector<Wrappers::PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 }
		};

		m_DescriptorAllocator = std::make_unique<Wrappers::DescriptorAllocator>(device, 1000, sizes);
		auto& swapChain = context.GetVulkanContext().GetSwapChain();
		GuiPassInfo passInfo(resourceManager.GetColorBuffer().GetFormat());
		m_RenderPass = std::make_unique<Wrappers::RenderPass>(context.GetVulkanContext().GetDevice(), passInfo.GetInfo());

		// - Imgui init
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;

		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForVulkan(const_cast<GLFWwindow*>(vulkanContext.GetWindowManager().GetGlfwWindow()), true);
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = device.GetNativeInstance();
		initInfo.PhysicalDevice = device.GetNativePhysicalDevice();
		initInfo.Device = device.GetNativeDevice();
		initInfo.QueueFamily = device.GetIndicies().graphicsFamily.value();
		initInfo.Queue = device.GetNativeGraphicsQueue();
		initInfo.PipelineCache = VK_NULL_HANDLE;
		initInfo.DescriptorPool = m_DescriptorAllocator->GetNativeDescriptoPool();
		initInfo.Allocator = nullptr;
		initInfo.MinImageCount = swapChain.GetMinImagesCount();
		initInfo.ImageCount = static_cast<uint32_t>(swapChain.GetSwapChainImagesSize());
		initInfo.CheckVkResultFn = nullptr;
		initInfo.RenderPass = m_RenderPass->GetNativeRenderPass();
		ImGui_ImplVulkan_Init(&initInfo);

		UploadFonts();

		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView()};
		m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			device,
			attacments,
			resourceManager.GetColorBuffer().GetExtent(),
			*m_RenderPass);

	}	

	GuiPass::~GuiPass()
	{
	}

	void GuiPass::Render(Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& frameContext = context.GetFrameContext();
		auto backBufferIndex = frameContext.GetBackBufferIndex();

		auto& threadContext = context.GetThreadContext();
		auto& commandBuffer = threadContext.GetCommandBuffer();

		auto& vulkanContext = context.GetVulkanContext();
		auto& swapChain = vulkanContext.GetSwapChain();
		auto extent = swapChain.GetSwapChainExtent();

		commandBuffer.TransitionImage(
			resourceManager.GetColorBuffer().GetNativeImage(),
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		DrawGui(context);
		ImGui::Render();

		commandBuffer.BeginRenderPass(extent, *m_RenderPass, m_FrameBuffer->GetNativeFrameBuffer());
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.GetNativeCommandBuffer());
		commandBuffer.EndRenderPass();

	}

	void GuiPass::Cleanup(const Context::Context& context)
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		auto& device = context.GetVulkanContext().GetDevice();
		m_FrameBuffer->Cleanup(device);
		m_RenderPass->Cleanup(device);
		m_DescriptorAllocator->Cleanup(device);
	}

	void GuiPass::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& device = context.GetVulkanContext().GetDevice();
		m_FrameBuffer->Cleanup(device);

		auto& swapChain = context.GetVulkanContext().GetSwapChain();
		std::vector<VkImageView> attacments = { resourceManager.GetColorBuffer().GetNativeView() };
		m_FrameBuffer = std::make_unique<Wrappers::FrameBuffer>(
			device,
			attacments,
			swapChain.GetSwapChainExtent(),
			*m_RenderPass);
	}

	bool GuiPass::IsCursorPositionInGUI()
	{
		ImGuiIO& io = ImGui::GetIO();

		if (io.WantCaptureMouse)
			return true;

		return false;
	}

	void GuiPass::UploadFonts()
	{
		ImGui_ImplVulkan_CreateFontsTexture();

	}

	void GuiPass::DrawGui(Context::Context& context)
	{
		DrawInspector(context);
		DrawScene(context);
		ShowAppMainMenuBar(context);
		// TODO remove
		ImGui::ShowDemoWindow();
	}

	void GuiPass::DrawInspector(Context::Context& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2( ImGui::GetIO().DisplaySize.x, 20.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
			
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

	void GuiPass::DrawTitle(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();
		if (scene.IsGameObjectSelected())
		{
			auto entity = scene.GetSelectedGameObject();
			auto& hierarchy = scene.GetHierarchyComponent(entity);
			auto name = hierarchy.mName;
			if (ImGui::CollapsingHeader("Name"))
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

		if (scene.IsGameObjectSelected())
		{
			auto entity = scene.GetSelectedGameObject();

			if (ImGui::CollapsingHeader("Transform Component"))
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

		if (scene.IsGameObjectSelected())
		{
			auto entity = scene.GetSelectedGameObject();
			if (scene.HasMeshComponent(entity))
			{
				if (ImGui::CollapsingHeader("Mesh Component"))
				{
					auto& mesh = scene.GetMeshComponent(entity);
					auto& meshes = scene.GetMeshes();
					int selectedIndex = static_cast<int>(mesh.mMeshIndex.value());

					if (ImGui::BeginCombo("mesh", mesh.mMeshPath.c_str()))
					{
						for (int i = 0; i < meshes.size(); ++i)
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

		if (scene.IsGameObjectSelected())
		{
			auto entity = scene.GetSelectedGameObject();
		
			if (scene.HasRenderComponent(entity))
			{
				if (ImGui::CollapsingHeader("Rendering  Component"))
				{
					auto& render = scene.GetRenderComponent(entity);

					static bool enabled = render.mIsVisible;
					ImGui::Checkbox("Visible", &enabled);
					render.mIsVisible = enabled;

					auto& materials = scene.GetMaterials();

					if (!materials.empty())
					{
						if (ImGui::BeginCombo("material", render.mMaterial.c_str()))
						{
							int selectedIndex = static_cast<int>(render.mMaterialIndex.has_value() ? render.mMaterialIndex.value() : 0);

							for (int i = 0; i < materials.size(); ++i)
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
							static float materialTransparency = materialData.transparency;
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

	void GuiPass::DrawLight(Context::Context& context)
	{
		auto& scene = context.GetEngineContext().GetScene();

		if (scene.IsGameObjectSelected())
		{
			auto entity = scene.GetSelectedGameObject();

			if (scene.HasLightComponent(entity))
			{
				if (ImGui::CollapsingHeader("Light Component"))
				{
					auto& light = scene.GetLightComponent(entity);

					static bool enabled = light.mEnable;
					ImGui::Checkbox("Enabled", &enabled);
					light.mEnable = enabled;

					const char* types[] = { "Direction Light", "Point Light", "Spot Light" };
					static int lightType = static_cast<int>(light.mLightType);
					ImGui::Combo("Type", &lightType, types, IM_ARRAYSIZE(types));
					light.mLightType = static_cast<Scene::LightType>(lightType);

					static float color[3] = { light.mColor.r(), light.mColor.g(), light.mColor.b()};
					ImGui::ColorEdit3("Color", color);
					light.mColor = { color[0], color[1], color[2] };

					static float intencity = light.mIntencity;
					ImGui::SliderFloat("Intencity", &intencity, 0.0f, 3.0f, "ratio = %.03f");
					light.mIntencity = intencity;

					if (lightType > 0)
					{
						static float radius = light.mRadius;
						ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f, "ratio = %.3f");
						light.mRadius = radius;
						if (lightType > 1)
						{
							static float coneAngle = light.mConeAngle;
							ImGui::SliderFloat("Cone angle", &coneAngle, 0.1f, 179.9f, "ratio = %.3f");
							light.mConeAngle = coneAngle;
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

	void GuiPass::DrawHierarchyNode(Context::Context& context, ECS::Entity entity, int& index)
	{
		auto& scene = context.GetEngineContext().GetScene();
		auto& component = scene.GetHierarchyComponent(entity);
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
		bool isSelected = false;
		if (scene.IsGameObjectSelected() && entity == scene.GetSelectedGameObject())
		{
			isSelected = true;
		}
		if (isSelected)
			nodeFlags |= ImGuiTreeNodeFlags_Selected;

		if (component.mChildren.size() > 0)
		{
			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				scene.SelectGameObject(entity);
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
				scene.SelectGameObject(entity);
			}
			++index;
		}
	}

	void GuiPass::ShowAppMainMenuBar(Context::Context& context)
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
				if (ImGui::MenuItem("Create game object")) { scene.CreateGameObject(); }

				ImGui::Separator();
				if (ImGui::BeginMenu("Add component"))
				{
					if (ImGui::MenuItem("Mesh component")) { scene.TryToAddMeshComponent(); }
					if (ImGui::MenuItem("Render component")) { scene.TryToAddRenderComponent(); }
					if (ImGui::MenuItem("Light component")) { scene.TryToAddLightComponent(); }
					if (ImGui::MenuItem("Script component")) { }
					ImGui::EndMenu();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Create material")) { context.GetEngineContext().GetMaterialContainer().CreateNewMaterial(); }
				ImGui::EndMenu();
			}


			ImGui::EndMainMenuBar();
		}
	}

	void GuiPass::DrawScene(Context::Context& context)
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
				scene.CreateGameObject();
			}
			if (ImGui::Button("Delete object", sz))
			{
				scene.DeleteGameObject();
			}

			DrawHierarchyNode(context, scene.GetRoot(), index);

		}
		ImGui::End();
	}


}




