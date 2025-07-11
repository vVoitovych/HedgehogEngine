#include "GuiPass.hpp"
#include "GuiPassInfo.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/RendererContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

#include "HedgehogWrappers/Wrappeers/RenderPass/RenderPass.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/FrameBuffer/FrameBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"

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
		auto& rendererContext = context.GetRendererContext();
		auto& commandBuffer = rendererContext.GetCommandBuffer();

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
		DrawSceneInspector(context);
		DrawMainMenu(context);
		DrawSettingsWindow(context);
		// TODO remove
		ImGui::ShowDemoWindow();
	}



}




