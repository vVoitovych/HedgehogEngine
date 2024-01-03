#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class DescriptorSetLayout;
	class UBO;
	class TextureImage;
	class TextureSampler;

	class DescriptorSet
	{
	public:
		DescriptorSet(
			const std::unique_ptr<Device>& device, 
			std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout, 
			std::unique_ptr<UBO>& ubo, 
			std::unique_ptr<TextureImage>& image, 
			std::unique_ptr<TextureSampler>& sampler);
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






