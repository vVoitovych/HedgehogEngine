#pragma once

#include <memory>

namespace Wrappers
{
	class Pipeline;
	class DescriptorSetLayout;
}

namespace Context
{  
	class MaterialVilkanLayer
	{
    public:
        MaterialVilkanLayer();


    private:
		std::unique_ptr<Wrappers::DescriptorSetLayout> m_DescriptorSetLayout;
		std::unique_ptr<Wrappers::Pipeline> m_Pipeline;

	};

}


