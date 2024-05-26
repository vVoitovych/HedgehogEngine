#include "ThreadContext.hpp"
#include "VulkanContext.hpp"

#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/SyncObjects/SyncObject.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Descriptors/UBO.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSet.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/Context/FrameContext.hpp"
#include "Renderer/Containers/LightContainer.hpp"

#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include <stdexcept>

namespace Renderer
{
	ThreadContext::ThreadContext(const VulkanContext& vulkanContext)
	{
		mCommandBuffers.clear();
		mSyncObjects.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			CommandBuffer commandBuffer(vulkanContext.GetDevice());
			mCommandBuffers.push_back(std::move(commandBuffer));
			SyncObject syncObject(vulkanContext.GetDevice());
			mSyncObjects.push_back(std::move(syncObject));
		}
		uint32_t materialCount = 1;
		std::vector<PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};

		mFrameAllocator = std::make_unique<DescriptorAllocator>(vulkanContext.GetDevice(), MAX_FRAMES_IN_FLIGHT, sizes);

		DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		mFrameLayout = std::make_unique<DescriptorSetLayout>(vulkanContext.GetDevice(), builder, VK_SHADER_STAGE_VERTEX_BIT  | VK_SHADER_STAGE_FRAGMENT_BIT);

		mFrameUniforms.clear();
		mFrameSets.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			UBO<FrameUniform> frameUniformBuffer(vulkanContext.GetDevice());
			mFrameUniforms.push_back(std::move(frameUniformBuffer));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = mFrameUniforms[i].GetNativeBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = mFrameUniforms[i].GetBufferSize();

			DescriptorWrites write{};
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.pBufferInfo = &bufferInfo;
			write.pNext = nullptr;
			std::vector<DescriptorWrites> writes;
			writes.push_back(write);

			DescriptorSet descriptorSet(vulkanContext.GetDevice(), *mFrameAllocator, *mFrameLayout);
			descriptorSet.Update(vulkanContext.GetDevice(), writes);
			mFrameSets.push_back(std::move(descriptorSet));
		}
		LOGINFO("Thread context Initialized");
	}

	ThreadContext::~ThreadContext()
	{
	}

	void ThreadContext::Cleanup(const VulkanContext& vulkanContext)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Cleanup(vulkanContext.GetDevice());
			mSyncObjects[i].Cleanup(vulkanContext.GetDevice());
		}
		mCommandBuffers.clear();
		mSyncObjects.clear();

		for (auto& frameUniform : mFrameUniforms)
		{
			frameUniform.Cleanup(vulkanContext.GetDevice());
		}
		mFrameUniforms.clear();

		for (auto& frameSet : mFrameSets)
		{
			frameSet.Cleanup(vulkanContext.GetDevice(), *mFrameAllocator);
		}
		mFrameSets.clear();

		mFrameLayout->Cleanup(vulkanContext.GetDevice());
		mFrameAllocator->Cleanup(vulkanContext.GetDevice());

		LOGINFO("Thread context cleaned");
	}

	void ThreadContext::Update(const EngineContext& engineContext, const FrameContext& frameContext)
	{
		const auto& lightContainer = engineContext.GetLightContainer();

		FrameUniform ubo{};
		ubo.view = frameContext.GetCameraViewMatrix();
		ubo.proj = frameContext.GetCameraProjMatrix();
		ubo.eyePosition = glm::vec4(frameContext.GetCameraPosition(), 1.0f);
		ubo.lightCount = lightContainer.GetLightCount();
		const auto& lights = lightContainer.GetLights();
		for (size_t i = 0; i < ubo.lightCount; ++i)
		{
			ubo.lights[i] = lights[i];
		}
		mFrameUniforms[mFrameIndex].UpdateUniformBuffer(ubo);
	}

	void ThreadContext::NextFrame()
	{
		mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	CommandBuffer& ThreadContext::GetCommandBuffer()
	{
		return mCommandBuffers[mFrameIndex];
	}

	SyncObject& ThreadContext::GetSyncObject()
	{
		return mSyncObjects[mFrameIndex];
	}

	const DescriptorSetLayout& ThreadContext::GetLayout() const
	{
		return *mFrameLayout;
	}

	const DescriptorSet& ThreadContext::GetDescriptorSet() const
	{
		return mFrameSets[mFrameIndex];
	}

	DescriptorSet& ThreadContext::GetDescriptorSet()
	{
		return mFrameSets[mFrameIndex];
	}

}


