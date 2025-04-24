#pragma once

#include <memory>

namespace Context
{
	class VulkanContext;
	class MaterialFrontend;
	class MaterialVulkanLayer;
 
	class Material
	{
    public:
		Material();

		void Init(const VulkanContext& context);
		void Cleanup(const VulkanContext& context);
		
	private:
		void InitBackend(const VulkanContext& context);


    private:
		std::unique_ptr<MaterialFrontend> m_frontend;
		std::unique_ptr<MaterialVulkanLayer> m_backend;

	};

}


