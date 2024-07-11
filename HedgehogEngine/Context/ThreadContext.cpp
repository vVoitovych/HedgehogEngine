#include "ThreadContext.hpp"
#include "VulkanContext.hpp"

#include "Wrappeers/Commands/CommandBuffer.hpp"
#include "Wrappeers/SyncObjects/SyncObject.hpp"
#include "Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Wrappeers/Descriptors/UBO.hpp"
#include "Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Wrappeers/Descriptors/DescriptorSet.hpp"
#include "Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"
#include "Containers/LightContainer.hpp"
#include "Common/EngineDebugBreak.hpp"
#include "Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

#include <stdexcept>

namespace Context
{
	ThreadContext::ThreadContext(const VulkanContext& vulkanContext)
	{
		mCommandBuffers.clear();
		mSyncObjects.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			Wrappers::CommandBuffer commandBuffer(vulkanContext.GetDevice());
			mCommandBuffers.push_back(std::move(commandBuffer));
			Wrappers::SyncObject syncObject(vulkanContext.GetDevice());
			mSyncObjects.push_back(std::move(syncObject));
		}
		uint32_t materialCount = 1;
		std::vector<Wrappers::PoolSizeRatio> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};

		mFrameAllocator = std::make_unique<Wrappers::DescriptorAllocator>(vulkanContext.GetDevice(), MAX_FRAMES_IN_FLIGHT, sizes);

		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		mFrameLayout = std::make_unique<Wrappers::DescriptorSetLayout>(vulkanContext.GetDevice(), builder, VK_SHADER_STAGE_VERTEX_BIT  | VK_SHADER_STAGE_FRAGMENT_BIT);

		mFrameUniforms.clear();
		mFrameSets.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			Wrappers::UBO<FrameUniform> frameUniformBuffer(vulkanContext.GetDevice());
			mFrameUniforms.push_back(std::move(frameUniformBuffer));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			VkDescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = mFrameUniforms[i].GetNativeBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = mFrameUniforms[i].GetBufferSize();

			Wrappers::DescriptorWrites write{};
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			write.descriptorCount = 1;
			write.pBufferInfo = &bufferInfo;
			write.pNext = nullptr;
			std::vector<Wrappers::DescriptorWrites> writes;
			writes.push_back(write);

			Wrappers::DescriptorSet descriptorSet(vulkanContext.GetDevice(), *mFrameAllocator, *mFrameLayout);
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
		ubo.eyePosition = HM::Vector4(frameContext.GetCameraPosition(), 1.0f);
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

	Wrappers::CommandBuffer& ThreadContext::GetCommandBuffer()
	{
		return mCommandBuffers[mFrameIndex];
	}

	Wrappers::SyncObject& ThreadContext::GetSyncObject()
	{
		return mSyncObjects[mFrameIndex];
	}

	const Wrappers::DescriptorSetLayout& ThreadContext::GetLayout() const
	{
		return *mFrameLayout;
	}

	const Wrappers::DescriptorSet& ThreadContext::GetDescriptorSet() const
	{
		return mFrameSets[mFrameIndex];
	}

	Wrappers::DescriptorSet& ThreadContext::GetDescriptorSet()
	{
		return mFrameSets[mFrameIndex];
	}

}


