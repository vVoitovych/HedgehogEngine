#include "MaterialVulkanLayer.hpp"

#include "MaterialFrontend.hpp"

#include "HedgehogWrappers/Wrappeers/Pipeline/Pipeline.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"

namespace Context
{
	MaterialVulkanLayer::MaterialVulkanLayer(const Context::VulkanContext& context, const MaterialFrontend& material)
	{
	}

	MaterialVulkanLayer::~MaterialVulkanLayer()
	{
	}

	MaterialVulkanLayer::MaterialVulkanLayer(MaterialVulkanLayer&& rhs) noexcept
		: m_DescriptorSetLayout(std::move(rhs.m_DescriptorSetLayout))
		, m_Pipeline(std::move(rhs.m_Pipeline))
	{
		rhs.m_DescriptorSetLayout = nullptr;
		rhs.m_Pipeline = nullptr;
	}

	MaterialVulkanLayer& MaterialVulkanLayer::operator=(MaterialVulkanLayer&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_DescriptorSetLayout = std::move(rhs.m_DescriptorSetLayout);
			m_Pipeline = std::move(rhs.m_Pipeline);

			rhs.m_DescriptorSetLayout = nullptr;
			rhs.m_Pipeline = nullptr;
		}
		return *this;
	}

	void MaterialVulkanLayer::Init()
	{
	}

}

