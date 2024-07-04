#pragma once

#include "Common/RendererSettings.hpp"
#include "Containers/Light.hpp"

#include <memory>
#include <vector>

namespace Wrappers
{
	class CommandBuffer;
	class SyncObject;

	class DescriptorSetLayout;
	class DescriptorAllocator;
	class DescriptorSet;

	template<typename T>
	class UBO;

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
		void Update(const EngineContext& engineContext, const FrameContext& frameContext);

		void NextFrame();

		Wrappers::CommandBuffer& GetCommandBuffer();
		Wrappers::SyncObject& GetSyncObject();

		const Wrappers::DescriptorSetLayout& GetLayout() const;
		const Wrappers::DescriptorSet& GetDescriptorSet() const;
		Wrappers::DescriptorSet& GetDescriptorSet();

	private:
		struct FrameUniform
		{
			alignas(16) glm::mat4 view;
			alignas(16) glm::mat4 proj;
			alignas(16) glm::vec4 eyePosition;
			alignas(16) Light lights[MAX_LIGHTS_COUNT];
			size_t lightCount;
		};

	private:
		std::vector<Wrappers::CommandBuffer> mCommandBuffers;
		std::vector<Wrappers::SyncObject> mSyncObjects;

		std::unique_ptr<Wrappers::DescriptorSetLayout> mFrameLayout;
		std::unique_ptr<Wrappers::DescriptorAllocator> mFrameAllocator;

		std::vector<Wrappers::UBO<FrameUniform>> mFrameUniforms;
		std::vector<Wrappers::DescriptorSet> mFrameSets;

		uint32_t mFrameIndex = 0;
	};

}



