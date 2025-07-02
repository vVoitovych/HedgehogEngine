#pragma once

#include "HedgehogCommon/Common/RendererSettings.hpp"
#include "HedgehogContext/Containers/LightContainer/Light.hpp"

#include <vector>

namespace Wrappers
{
	class CommandBuffer;
	class SyncObject;

}

namespace Context
{
	class VulkanContext;
	class EngineContext;
	class FrameContext;

	class ThreadContext
	{
	public:
		ThreadContext(const VulkanContext& vulkanContext);
		~ThreadContext();

		ThreadContext(const ThreadContext&) = delete;
		ThreadContext& operator=(const ThreadContext&) = delete;

		void Cleanup(const VulkanContext& vulkanContext);

		void NextFrame();
		uint32_t GetFrameIndex() const;

		Wrappers::CommandBuffer& GetCommandBuffer();
		Wrappers::SyncObject& GetSyncObject();

	private:
		std::vector<Wrappers::CommandBuffer> m_CommandBuffers;
		std::vector<Wrappers::SyncObject> m_SyncObjects;

		uint32_t m_FrameIndex = 0;
	};

}



