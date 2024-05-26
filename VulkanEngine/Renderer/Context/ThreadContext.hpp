#pragma once

#include "Renderer/Common/RendererSettings.hpp"
#include "Renderer/Containers/Light.hpp"

#include <memory>
#include <vector>

namespace Renderer
{
	class VulkanContext;
	class EngineContext;
	class FrameContext;
	class CommandBuffer;
	class SyncObject;

	class DescriptorSetLayout;
	class DescriptorAllocator;
	class DescriptorSet;

	template<typename T>
	class UBO;

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

		CommandBuffer& GetCommandBuffer();
		SyncObject& GetSyncObject();

		const DescriptorSetLayout& GetLayout() const;
		const DescriptorSet& GetDescriptorSet() const;
		DescriptorSet& GetDescriptorSet();

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
		std::vector<CommandBuffer> mCommandBuffers;
		std::vector<SyncObject> mSyncObjects;

		std::unique_ptr<DescriptorSetLayout> mFrameLayout;
		std::unique_ptr<DescriptorAllocator> mFrameAllocator;

		std::vector<UBO<FrameUniform>> mFrameUniforms;
		std::vector<DescriptorSet> mFrameSets;

		uint32_t mFrameIndex = 0;
	};

}



