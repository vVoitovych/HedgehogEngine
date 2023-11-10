#pragma once

#include "VulkanEngine/Renderer/Wrappeers/FrameBuffer/BackBuffers.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "VulkanEngine/Renderer/Common/RendererSettings.hpp"

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class RenderContext
	{
	public:
		RenderContext() = default;
		~RenderContext() = default;

		void Initialize(Device& device);
		void Cleanup(Device& device);

		CommandBuffer& GetCommandBuffer(size_t index);
		VkExtent2D GetExtend() const;
		uint32_t GetCurrentFrame() const;

		void NextFrame();
	private:
		uint32_t mCurrentFrame;
		VkExtent2D mExtend;

		BackBuffers mBackBuffers;
		CommandBuffer mCommandBuffers[MAX_FRAMES_IN_FLIGHT];

	};
}


