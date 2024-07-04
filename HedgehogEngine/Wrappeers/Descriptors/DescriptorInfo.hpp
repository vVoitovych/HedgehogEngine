#pragma once

#include <vulkan/vulkan.h>

namespace Wrappers
{
	struct DescriptorInfo
	{
		int						bindingNumber;
		VkDescriptorType		descriptorType;
		VkShaderStageFlagBits	shaderStage;
	};
}

