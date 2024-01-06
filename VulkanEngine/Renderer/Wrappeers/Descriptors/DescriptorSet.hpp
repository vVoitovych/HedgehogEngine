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
			UBO& ubo, 
			std::unique_ptr<TextureImage>& image, 
			std::unique_ptr<TextureSampler>& sampler);
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		DescriptorSet(DescriptorSet&& other) noexcept;
		DescriptorSet& operator=(DescriptorSet&& other) noexcept;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkDescriptorSet* GetNativeSet();
	private:
		VkDescriptorSet mDescriptorSet;

	};


}






