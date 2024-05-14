#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;
	class DescriptorPool;
	class DescriptorSetLayout;
	class Buffer;
	class Image;
	class Sampler;

	class DescriptorSet
	{
	public:
		DescriptorSet(
			const Device& device,
			const DescriptorPool& descriptorPool,
			const DescriptorSetLayout& descriptorSetLayout, 
			const Buffer& ubo,
			const Image& image, 
			const Sampler& sampler);
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		void Cleanup(const Device& device, const DescriptorPool& descriptionPool);

		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






