#pragma once

#include <memory>

namespace Context
{
	class VulkanContext;
}

namespace Wrappers
{
	class Pipeline;
	class DescriptorSetLayout;
}

namespace Context
{  
	class MaterialFrontend;

	class MaterialVulkanLayer
	{
    public:
		MaterialVulkanLayer(const Context::VulkanContext& context, const MaterialFrontend& material);
		~MaterialVulkanLayer();

		void Init();

    private:
		std::unique_ptr<Wrappers::DescriptorSetLayout> m_DescriptorSetLayout;
		std::unique_ptr<Wrappers::Pipeline> m_Pipeline;

	};

}


