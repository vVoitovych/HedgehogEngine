#include "DescriptorSetLayout.hpp"
#include "DescriptorLayoutBuilder.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"

#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <array>

namespace Wrappers
{
	DescriptorSetLayout::DescriptorSetLayout(
		const Device& device, 
		DescriptorLayoutBuilder& builder, 
		VkShaderStageFlags shaderStages, 
		void* pNext, 
		VkDescriptorSetLayoutCreateFlags flags)
		: m_DescriptorSetLayout(nullptr)
	{
		m_DescriptorSetLayout = builder.Build(device, shaderStages, pNext, flags);
		LOGINFO("Descriptor set layout initialized");
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		if (m_DescriptorSetLayout != nullptr)
		{
			LOGERROR("Vulkan description set layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& rhs) noexcept
		: m_DescriptorSetLayout(rhs.m_DescriptorSetLayout)
	{
		rhs.m_DescriptorSetLayout = nullptr;
	}

	DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_DescriptorSetLayout = rhs.m_DescriptorSetLayout;
			rhs.m_DescriptorSetLayout = nullptr;
		}
		return *this;
	}

	void DescriptorSetLayout::Cleanup(const Device& device)
	{
		vkDestroyDescriptorSetLayout(device.GetNativeDevice(), m_DescriptorSetLayout, nullptr);
		m_DescriptorSetLayout = nullptr;
		LOGINFO("Descriptor set layout cleaned");
	}
	VkDescriptorSetLayout DescriptorSetLayout::GetNativeLayout() const
	{
		return m_DescriptorSetLayout;
	}
	const VkDescriptorSetLayout* DescriptorSetLayout::GetNativeLayoutPtr() const
	{
		return &m_DescriptorSetLayout;
	}
}

