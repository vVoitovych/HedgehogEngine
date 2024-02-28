#pragma once

#include <vulkan/vulkan.h>
#include <memory>

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
			const std::unique_ptr<Device>& device,
			const std::unique_ptr<DescriptorPool>& descriptorPool,
			const std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout, 
			const std::unique_ptr<Buffer>& ubo,
			const Image& image, 
			const Sampler& sampler);
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		void Cleanup(const std::unique_ptr<Device>& device, const std::unique_ptr<DescriptorPool>& descriptionPool);

		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






