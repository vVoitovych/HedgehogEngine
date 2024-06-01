#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Hedgehog
{
	namespace Wrappers
	{
		class Device;

		class DescriptorLayoutBuilder
		{
			friend class DescriptorSetLayout;

		public:
			void AddBinding(uint32_t binding, VkDescriptorType type);
			void ClearBindings();

		private:
			VkDescriptorSetLayout Build(const Device& device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);

		private:
			std::vector<VkDescriptorSetLayoutBinding> mBindings;
		};
	}
}



