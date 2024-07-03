#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;
	class DescriptorAllocator;
	class DescriptorSetLayout;
	class Buffer;
	class Image;
	class Sampler;

	struct DescriptorWrites
	{
		VkDescriptorType                 descriptorType;
		uint32_t                         dstBinding;
		uint32_t                         dstArrayElement;
		uint32_t                         descriptorCount;
		const VkDescriptorImageInfo* pImageInfo;
		const VkDescriptorBufferInfo* pBufferInfo;
		const VkBufferView* pTexelBufferView;
		const void* pNext;
	};

	class DescriptorSet
	{
		friend class DescriptorAllocator;

	public:
		DescriptorSet(
			const Device& device,
			const DescriptorAllocator& allocator,
			const DescriptorSetLayout& descriptorSetLayout);
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		void Update(const Device& device, std::vector<DescriptorWrites>& descriptorWrites);
		void Cleanup(const Device& device, const DescriptorAllocator& allocator);

		const VkDescriptorSet* GetNativeSet() const;
		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






