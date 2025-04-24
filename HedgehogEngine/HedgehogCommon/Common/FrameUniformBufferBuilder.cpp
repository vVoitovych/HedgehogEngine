#include "FrameUniformBufferBuilder.hpp"

#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorLayoutBuilder.hpp"

#include <vulkan/vulkan.h>

namespace Context
{
	Wrappers::DescriptorLayoutBuilder FrameUniformBufferBilder::Build()
	{
		Wrappers::DescriptorLayoutBuilder builder;
		builder.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		return builder;
	}
}

