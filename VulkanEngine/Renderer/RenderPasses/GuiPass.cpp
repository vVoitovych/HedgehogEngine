#include "GuiPass.h"
#include "GuiPassInfo.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include "Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorPool.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/FrameBuffer/FrameBuffer.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"

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

		ApplyStyle();

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
		auto extent = vulkanContext->GetSwapChain()->GetSwapChainExtent();

		// Start the Dear ImGui frame
		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// begin ImGui here
		ImGui::ShowDemoWindow();

		// end ImGui here
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

	void GuiPass::ApplyStyle()
	{
		auto& style = ImGui::GetStyle();
		style.FrameRounding = 4.0f;
		style.WindowBorderSize = 0.0f;
		style.PopupBorderSize = 0.0f;
		style.GrabRounding = 4.0f;

		ImVec4* colors = ImGui::GetStyle().Colors;
		colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.73f, 0.75f, 0.74f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 0.94f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
		colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.84f, 0.66f, 0.66f, 0.40f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.84f, 0.66f, 0.66f, 0.67f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.47f, 0.22f, 0.22f, 0.67f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.47f, 0.22f, 0.22f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.47f, 0.22f, 0.22f, 0.67f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.34f, 0.16f, 0.16f, 1.00f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.71f, 0.39f, 0.39f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.84f, 0.66f, 0.66f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.47f, 0.22f, 0.22f, 0.65f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.71f, 0.39f, 0.39f, 0.65f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
		colors[ImGuiCol_Header] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.84f, 0.66f, 0.66f, 0.65f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.84f, 0.66f, 0.66f, 0.00f);
		colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_ResizeGrip] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
		colors[ImGuiCol_ResizeGripActive] = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
		colors[ImGuiCol_Tab] = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
		colors[ImGuiCol_TabActive] = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
	}

	void GuiPass::UploadFonts()
	{
		ImGui_ImplVulkan_CreateFontsTexture();

	}


}




