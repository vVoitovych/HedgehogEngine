#include "DescriptorSetLayout.hpp"
#include "DescriptorLayoutBuilder.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{
	DescriptorSetLayout::DescriptorSetLayout(
		const Device& device, 
		DescriptorLayoutBuilder& builder, 
		VkShaderStageFlags shaderStages, 
		void* pNext, 
		VkDescriptorSetLayoutCreateFlags flags)
		: mDescriptorSetLayout(nullptr)
	{
		mDescriptorSetLayout = builder.Build(device, shaderStages, pNext, flags);
		LOGINFO("Descriptor set layout initialized");
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		if (mDescriptorSetLayout != nullptr)
		{
			LOGERROR("Vulkan description set layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorSetLayout::Cleanup(const Device& device)
	{
		vkDestroyDescriptorSetLayout(device.GetNativeDevice(), mDescriptorSetLayout, nullptr);
		mDescriptorSetLayout = nullptr;
		LOGINFO("Descriptor set layout cleaned");
	}
	VkDescriptorSetLayout DescriptorSetLayout::GetNativeLayout() const
	{
		return mDescriptorSetLayout;
	}
	const VkDescriptorSetLayout* DescriptorSetLayout::GetNativeLayoutPtr() const
	{
		return &mDescriptorSetLayout;
	}
}

