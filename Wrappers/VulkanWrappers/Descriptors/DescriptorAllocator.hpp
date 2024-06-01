#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Hedgehog
{
	namespace Wrappers
	{
		class Device;
		class DescriptorSetLayout;

		struct PoolSizeRatio
		{
			VkDescriptorType type;
			float ratio;
		};

		class DescriptorAllocator
		{
			friend class DescriptorSet;

		public:
			DescriptorAllocator(const Device& device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios);
			~DescriptorAllocator();

			DescriptorAllocator(const DescriptorAllocator&) = delete;
			DescriptorAllocator(DescriptorAllocator&&) = delete;
			DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
			DescriptorAllocator& operator=(DescriptorAllocator&&) = delete;

			void Cleanup(const Device& device);
			const VkDescriptorPool GetNativeDescriptoPool() const;
			VkDescriptorPool GetNativeDescriptoPool();

		private:
			VkDescriptorSet Allocate(const Device& device, const DescriptorSetLayout& layout) const;
			void FreeDescriptorSet(const Device& device, VkDescriptorSet* pDescriptorSet) const;

		private:
			VkDescriptorPool mPool;

		};
	}
}









