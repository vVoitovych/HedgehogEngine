#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	struct DescriptorInfo
	{
		int						bindingNumber;
		VkDescriptorType		descriptorType;
		VkShaderStageFlagBits	ShaderStage;

	};

}






