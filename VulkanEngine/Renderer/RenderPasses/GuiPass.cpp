#include "GuiPass.h"
#include "GuiPassInfo.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/FrameContext.hpp"
#include "Renderer/Context/EngineContext.hpp"

#include "Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorPool.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "Renderer/Wrappeers/Resources/Image/ImageManagement.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"

#include "Scene/Scene.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_vulkan.h"
#include "ImGui/imgui_impl_glfw.h"

#include <vector>

namespace Renderer
{
	GuiPass::GuiPass(const std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();
		auto& device = vulkanContext->GetDevice();
		std::vector<VkDescriptorPoolSize> poolSizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		mDescriptorPool = std::make_unique<DescriptorPool>(device, poolSizes, 1000 * 11);
		auto& swapChain = context->GetVulkanContext()->GetSwapChain();
		GuiPassInfo passInfo(swapChain->GetFormat());
		mRenderPass = std::make_unique<RenderPass>(context->GetVulkanContext()->GetDevice(), passInfo.GetInfo());

		// - Imgui init
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		(void)io;

		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForVulkan(vulkanContext->GetWindowManager()->GetGlfwWindow(), true);
		ImGui_ImplVulkan_InitInfo initInfo = {};
		initInfo.Instance = device->GetNativeInstance();
		initInfo.PhysicalDevice = device->GetNativePhysicalDevice();
		initInfo.Device = device->GetNativeDevice();
		initInfo.QueueFamily = device->GetIndicies().mGraphicsFamily.value();
		initInfo.Queue = device->GetNativeGraphicsQueue();
		initInfo.PipelineCache = VK_NULL_HANDLE;
		initInfo.DescriptorPool = mDescriptorPool->GetNativeDescriptoPool();
		initInfo.Allocator = nullptr;
		initInfo.MinImageCount = swapChain->GetMinImagesCount();
		initInfo.ImageCount = static_cast<uint32_t>(swapChain->GetSwapChainImagesSize());
		initInfo.CheckVkResultFn = nullptr;
		ImGui_ImplVulkan_Init(&initInfo, mRenderPass->GetNativeRenderPass());

		UploadFonts();

		mFrameBuffers.clear();
		size_t swapChainImagesSize = swapChain->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			std::vector<VkImageView> attacments = { swapChain->GetNativeSwapChainImageView(i) };
			FrameBuffer frameBuffer(
				device,
				attacments,
				swapChain->GetSwapChainExtent(),
				mRenderPass);
			mFrameBuffers.push_back(std::move(frameBuffer));
		}
	}	

	GuiPass::~GuiPass()
	{
	}

	void GuiPass::Render(std::unique_ptr<RenderContext>& context)
	{
		auto& frameContext = context->GetFrameContext();
		auto backBufferIndex = frameContext->GetBackBufferIndex();

		auto& threadContext = context->GetThreadContext();
		auto& commandBuffer = threadContext->GetCommandBuffer();

		auto& vulkanContext = context->GetVulkanContext();
		auto& swapChain = vulkanContext->GetSwapChain();
		auto extent = swapChain->GetSwapChainExtent();

		ImageManagement::RecordTransitionImageLayout(
			1, 
			VK_IMAGE_LAYOUT_UNDEFINED, 
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			false, 
			commandBuffer.GetNativeCommandBuffer(), 
			swapChain->GetSwapChainImage(backBufferIndex));

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		DrawGui(context);
		ImGui::Render();

		commandBuffer.BeginRenderPass(extent, mRenderPass, mFrameBuffers[backBufferIndex].GetNativeFrameBuffer());
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer.GetNativeCommandBuffer());
		commandBuffer.EndRenderPass();

	}

	void GuiPass::Cleanup(const std::unique_ptr<RenderContext>& context)
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		auto& device = context->GetVulkanContext()->GetDevice();
		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(device);
		}
		mRenderPass->Cleanup(device);
		mDescriptorPool->Cleanup(device);
	}

	void GuiPass::ResizeResources(const std::unique_ptr<RenderContext>& context)
	{
		auto& device = context->GetVulkanContext()->GetDevice();
		for (size_t i = 0; i < mFrameBuffers.size(); ++i)
		{
			mFrameBuffers[i].Cleanup(device);
		}
		auto& swapChain = context->GetVulkanContext()->GetSwapChain();
		mFrameBuffers.clear();
		size_t swapChainImagesSize = swapChain->GetSwapChainImagesSize();
		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			std::vector<VkImageView> attacments = { swapChain->GetNativeSwapChainImageView(i) };
			FrameBuffer frameBuffer(
				device,
				attacments,
				swapChain->GetSwapChainExtent(),
				mRenderPass);
			mFrameBuffers.push_back(std::move(frameBuffer));
		}
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

	void GuiPass::DrawGui(const std::unique_ptr<RenderContext>& context)
	{
		DrawScene(context);
		DrawInspector(context);

		// TODO remove
		ImGui::ShowDemoWindow();
	}

	void GuiPass::DrawInspector(const std::unique_ptr<RenderContext>& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(
			ImVec2(sizeX, sizeY),
			ImGuiCond_Always
		);
		ImGui::SetNextWindowPos(
			ImVec2(
				ImGui::GetIO().DisplaySize.x,
				0.0f
			),
			ImGuiCond_Always,
			ImVec2(1.0f, 0.0f)
		);
			
		ImGui::Begin(
			"Inspector",
			NULL,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove
		);
		{
			if (ImGui::CollapsingHeader("Transform"))
			{
				static float x = 0.0f;
				static float y = 0.0f;
				static float z = 0.0f;

				static float r_x = 0.0f;
				static float r_y = 0.0f;
				static float r_z = 0.0f;

				static float s_x = 0.0f;
				static float s_y = 0.0f;
				static float s_z = 0.0f;

				ImGui::SeparatorText("Position");
				ImGui::DragFloat("pos x", &x, 0.005f);
				ImGui::DragFloat("pos y", &y, 0.005f);
				ImGui::DragFloat("pos z", &z, 0.005f);

				ImGui::SeparatorText("Rotation");
				ImGui::DragFloat("rot x", &r_x, 0.005f);
				ImGui::DragFloat("rot y", &r_y, 0.005f);
				ImGui::DragFloat("rot z", &r_z, 0.005f);

				ImGui::SeparatorText("Scale");
				ImGui::DragFloat("scale x", &s_x, 0.005f);
				ImGui::DragFloat("scale y", &s_y, 0.005f);
				ImGui::DragFloat("scale z", &s_z, 0.005f);
			}
		}

		ImGui::End();
	
	}

	void GuiPass::DrawHierarchyNode(const std::unique_ptr<RenderContext>& context, ECS::Entity entity, int& index)
	{
		auto& scene = context->GetEngineContext()->GetScene();
		auto& component = scene.GetHierarchyComponent(entity);
		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
		const bool isSelected = (mSelectionMask & (1 << index)) != 0 && mNodeClicked != -1;
		if (isSelected)
			nodeFlags |= ImGuiTreeNodeFlags_Selected;

		if (component.mChildren.size() > 0)
		{
			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)index, nodeFlags, component.mName.c_str());
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				mNodeClicked = index;
				mSelectedNode = entity;
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
				mNodeClicked = index;
				mSelectedNode = entity;
			}
			++index;
		}
	}


	void GuiPass::DrawScene(const std::unique_ptr<RenderContext>& context)
	{
		float sizeX, sizeY, paddingY;

		sizeX = 300.0f;
		sizeY = float(ImGui::GetIO().DisplaySize.y);
		paddingY = 0.05f;
		ImGui::SetNextWindowSize(ImVec2(sizeX, sizeY), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(sizeX, 0.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

		ImGui::Begin("Scene", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

		{
			auto& scene = context->GetEngineContext()->GetScene();

			int index = 0;

			ImVec2 sz = ImVec2(-FLT_MIN, 0.0f);

			if (ImGui::Button("Create object", sz))
			{
				if (mNodeClicked != -1)
				{
					scene.CreateGameObject(mSelectedNode);
				}
				else
				{
					scene.CreateGameObject();
				}
			}
			if (ImGui::Button("Delete object", sz))
			{
				if (mNodeClicked != -1)
				{
					scene.DeleteGameObject(mSelectedNode);
					mNodeClicked = -1;
				}
			}

			DrawHierarchyNode(context, scene.GetRoot(), index);

			if (mNodeClicked != -1)
			{
				mSelectionMask = (1 << mNodeClicked);
			}
		}
		ImGui::End();
	}


}




