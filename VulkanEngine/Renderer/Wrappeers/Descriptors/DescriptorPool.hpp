#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Renderer
{
	class Device;
	class DescriptorSetLayout;

	class DescriptorPool
	{
	public:
		DescriptorPool(
			const std::unique_ptr<Device>& device,
			const std::vector<VkDescriptorPoolSize> poolSizes,
			const uint32_t descriptorSetsCount
		);

		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool(DescriptorPool&&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;
		DescriptorPool& operator=(DescriptorPool&&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);
		const VkDescriptorPool& GetNativeDescriptoPool() const;

		void AllocDescriptorSets(
			const std::unique_ptr<Device>& device,
			const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
			VkDescriptorSet* descriptorSets
		) const;

		void FreeDescriptorSet(const std::unique_ptr<Device>& device, VkDescriptorSet* pDescriptorSet) const;

	private:
		VkDescriptorPool mDescriptorPool;

	};

}




