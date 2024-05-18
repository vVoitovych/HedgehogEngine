#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;
	class DescriptorSetLayout;

	class DescriptorPool
	{
	public:
		DescriptorPool(const Device& device, const std::vector<VkDescriptorPoolSize> poolSizes, const uint32_t descriptorSetsCount);

		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool(DescriptorPool&&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;
		DescriptorPool& operator=(DescriptorPool&&) = delete;

		void Cleanup(const Device& device);
		const VkDescriptorPool& GetNativeDescriptoPool() const;

		void AllocDescriptorSets(
			const Device& device,
			const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
			VkDescriptorSet* descriptorSets
		) const;

		void FreeDescriptorSet(const Device& device, VkDescriptorSet* pDescriptorSet) const;

	private:
		VkDescriptorPool mDescriptorPool;

	};

}




